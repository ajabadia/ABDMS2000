#pragma once

/*
 * Equalizer — DOS repisas en cascada, sobre la maquina compartida.
 *
 * POR QUE SIGUE EXISTIENDO ESTA CLASE Y NO SE USA `CascadeShelfEq` DIRECTAMENTE.
 * La API que llama el producto —`setLowFreqIndex`, `setHighGainDB`— es la del
 * MS2000, con las CUATRO frecuencias por banda del hardware, y eso no cambia:
 * `SynthEngine`, `WasmBridge` y el panel WebUI la usan tal cual y no tienen por
 * que saber nada. Por eso esto es un SHIM, con la misma forma que
 * `Source/Core/AudioThreadSnapshot.h`.
 *
 * LO QUE SE MIGRÓ Y LO QUE NO.
 *   - MIGRÓ: el biquad, el cálculo de coeficientes, la banda muerta de 0,05 dB y
 *     los cuatro estados de la forma II transpuesta (`ShelfFilter`).
 *   - MIGRÓ: la ORDEN de las dos repisas, que hasta ahora eran dos llamadas
 *     escritas aquí sin una sola comprobación que las mirara. Eso es
 *     `CascadeShelfEq`.
 *   - MIGRÓ: las tablas de 160/250/400/600 Hz y 4000/6000/8000/12000 Hz. Aquí
 *     estaban como "política de producto", y el argumento ya no se sostiene: el
 *     módulo tiene las del RE-201, del DeepMind 12 y de la Juno-106, que son
 *     exactamente el mismo tipo de cosa. Ahora están en
 *     `ABDSharedCode/DspEffects/profiles/MS2000EqProfile.h` y esta clase ni las
 *     menciona. Lo que queda del shim es la FORMA de la API, que sí es del
 *     producto.
 *   - NO MIGRÓ, y no se puede: que `prepare` conserve la posición que el panel
 *     tiene puesta. Ver `prepare`, abajo.
 *
 * EL CAMBIO DE SONIDO, DELIBERADO Y MEDIDO.
 * El original usaba `std::sqrt`, `std::pow`, `std::cos` y `std::sin` de libm. El
 * módulo no puede: solo tiene lo que hay en `DspCore/DspMath.h`, y `sqrt` no está
 * entre ellas. El sustituto medido es `exp2 (0.5 * log2 (A))`, y el resultado no
 * es el mismo bit a bit:
 *
 *     std::pow  -> dsp::pow                2 ulps
 *     std::sqrt -> exp2 (0.5 * log2 (A))   2 ulps
 *     std::cos  -> dsp::cos                5 ulps
 *     std::sin  -> dsp::sin                5 ulps
 *     los cuatro juntos, en un coeficiente     hasta 509 ulps
 *     y en la RESPUESTA del filtro            0,012 dB
 *     y en la SEÑAL, las dos bandas            -74,8 dBFS de pico
 *
 * Cuatro ulps en un coeficiente son unos 2e-07 relativos. En las unidades que se
 * oyen, el cambio es de 0,012 dB de respuesta, que es inaudible, y es el precio
 * de que el filtro valga lo mismo a 32, 44,1 y 48 kHz: el original usaba la libm
 * de la máquina, y esas cuatro funciones no tienen por qué dar el mismo último
 * bit en un compilador que en otro. La referencia congelada del módulo
 * (`DspEffects/DspEffectsTests.cpp`, `namespace frozen`) lleva una copia de las
 * fórmulas originales con libm y mide la diferencia, así que este cambio de
 * sonido está fijado, no escrito.
 *
 * UNA DIFERENCIA DE COMPORTAMIENTO QUE NO ES DE SONIDO. El `ShelfFilter` limita
 * la frecuencia a 0,45 veces el sample rate, y el original no la limitaba. A
 * 32 kHz el límite es 14,4 kHz, así que la repisa alta de 12 kHz —que el MS2000
 * usa— se calcularía a 12 kHz de verdad. A 32 kHz el original calculaba el
 * biquad a 12 kHz también. El límite solo entra por debajo de 0,45·fs, y la
 * frecuencia más alta del MS2000 es 0,375·fs a 32 kHz, con margen.
 *
 * ESTA MIGRACIÓN ES LA SEGUNDA, Y TAMPOCO SE OYE NADA. Lo que se ha movido aquí
 * no es el biquad —que ya estaba en el módulo desde la anterior— sino la
 * composición de las dos repisas y sus tablas. `CascadeShelfEq` llama a los
 * mismos dos `ShelfFilter`, con los mismos números y en el mismo orden, así que
 * la salida tiene que ser la MISMA bits, no parecida. Eso es lo que comprueba
 * `Source/Tests/DSPCoreTests_EqualizerParity.cpp`, muestra a muestra y con
 * `bit_cast` a entero, que es más estricto que comparar con `==`: así también
 * cuenta como diferencia un -0,0 donde antes había un +0,0. No es un test de
 * tolerancia, y no debe convertirse en uno.
 */

#include "DspEffects/CascadeShelfEq.h"
#include "DspEffects/profiles/MS2000EqProfile.h"

namespace ABDMS2000 {

/**
 * @brief 2-Band Cascaded Shelving Equalizer based on reverse-engineered Korg MS2000 DSP specs.
 * Two `abd::dsp::ShelfFilter` in cascade, Q fixed at Butterworth (0.7071):
 * - Low Freq:  160Hz, 250Hz (default), 400Hz, 600Hz
 * - High Freq: 4.0kHz, 6.0kHz, 8.0kHz (default), 12.0kHz
 * - Gain Range: -12.0dB to +12.0dB (0..127 with 64 = 0dB)
 *
 * The biquad lives in the shared module, and so do the numbers: what is left
 * here is the shape of the API, which is the product's.
 */
class Equalizer {
public:
    Equalizer() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setLowFreqIndex(int index0to3) noexcept;
    void setLowGainDB(float gainDB) noexcept;
    void setHighFreqIndex(int index0to3) noexcept;
    void setHighGainDB(float gainDB) noexcept;

    void process(float& leftSample, float& rightSample) noexcept;

private:
    /** La última posición del selector, leída del perfil y no escrita aquí.

        El panel tiene cuatro posiciones porque las tiene el hardware, y el
        perfil del módulo tiene esas mismas cuatro. Escribir el 3 otra vez sería
        una tercera copia de un número que ya vive en dos sitios más, y la que se
        olvidaría de actualizarse al añadir una quinta posición. */
    static constexpr int kUltimaPosicion = abd::dsp::MS2000EqProfile::numPositions - 1;

    // La posición que tiene puesta el panel. NO es el estado del motor: es su
    // memoria, y existe por una sola razón, que está explicada en `prepare`.
    int lowFreqIndex_  = abd::dsp::MS2000EqProfile::defaultLowIndex;   // 250 Hz
    int highFreqIndex_ = abd::dsp::MS2000EqProfile::defaultHighIndex;  // 8.0 kHz

    // Las dos repisas en cascada, con los números del hardware dentro.
    abd::dsp::CascadeShelfEq<abd::dsp::MS2000EqProfile> eq_;
};

} // namespace ABDMS2000
