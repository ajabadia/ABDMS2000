#include "Equalizer.h"
#include "../Common/DSPUtils.h"

namespace ABDMS2000
{

/*
 * El motor —el biquad, los coeficientes, la banda muerta de 0,05 dB, la forma II
 * transpuesta con estado separado por canal, las dos repisas en cascada y las
 * tablas de frecuencia— ya no esta aqui. Todo eso vive en
 * `ABDSharedCode/DspEffects/CascadeShelfEq.h` con el perfil
 * `DspEffects/profiles/MS2000EqProfile.h`.
 *
 * LO QUE QUEDA EN ESTE FICHERO, Y POR QUE NO SE BORRA:
 *   - la FORMA de la API que llama el panel (`setLowFreqIndex`, `setHighGainDB`),
 *     que usan `SynthEngine`, `WasmBridge` y el WebUI y no tiene por que cambiar;
 *   - la memoria del selector, y el unico caso en que este shim hace mas que
 *     delegar. Ver `prepare`.
 *
 * LO QUE SE MANTIENE EXACTO DEL ANTERIOR, por contrato con el panel y con
 * `WasmBridge`:
 *   - los indices 0..3 se recortan a 0..3;
 *   - la ganancia se recorta a -12..+12 dB y solo se aplica si se ha movido mas
 *     de 0,05 dB (esa es la banda muerta, y la pone el motor, no esta clase);
 *   - la salida va en `leftSample` y `rightSample`, in situ.
 *
 * Y EL SONIDO, BIT A BIT. Todo lo de arriba sale de la misma maquina con los
 * mismos numeros en el mismo orden, asi que la salida no puede cambiar. No es
 * una promesa: lo comprueba `Source/Tests/DSPCoreTests_EqualizerParity.cpp`,
 * que corre esta clase contra una copia CONGELADA de la anterior y exige
 * igualdad de bits en cada muestra de los dos canales.
 */

void Equalizer::prepare(double sampleRate) noexcept
{
    // `validateSampleRate` sigue mandando: es la validacion del PRODUCTO y el
    // modulo no la puede sustituir por la suya, que solo mira que el sample rate
    // no sea absurdo. Se pasa el valor ya validado, no el que dio el host.
    const double valido = DSPUtils::validateSampleRate(sampleRate);

    eq_.prepare(valido);

    // ESTA ES LA UNICA LINEA DE ESTE FICHERO QUE NO ES UNA DELEGACION, Y ESTA
    // PORQUE. `CascadeShelfEq::prepare` pone los indices de FABRICA del perfil
    // (250 Hz y 8 kHz), porque una maquina arranca donde dice su perfil. El
    // shim anterior hacia otra cosa: guardaba la posicion que el panel tenia
    // puesta y se la ponia a los dos motores, de modo que un `prepare` a mitad
    // de sesion —que es cuando lo llama un host que reinicia el audio— no
    // devolvia el ecualizador a 250 Hz y 8 kHz por debajo del usuario.
    //
    // Sin estas dos lineas, la migracion habria cambiado el comportamiento en
    // silencio: mismo sonido, distinta frecuencia. El test de paridad mete
    // precisamente este caso (mover el selector, llamar a `prepare` y poner la
    // ganancia) y se puso rojo al quitarlas, en la muestra en la que ocurre el
    // `prepare`. Ese es el motivo de que el caso exista.
    eq_.setLowIndex(lowFreqIndex_);
    eq_.setHighIndex(highFreqIndex_);

    // La ganancia la reinicia la maquina a los 0 dB del perfil, que es lo que
    // hacia el anterior. `reset` es el estado de audio; el anterior lo llamaba
    // aqui, y se sigue llamando.
    reset();
}

void Equalizer::reset() noexcept
{
    eq_.reset();
}

void Equalizer::setLowFreqIndex(int index0to3) noexcept
{
    const int recortado = index0to3 < 0 ? 0 : (index0to3 > kUltimaPosicion ? kUltimaPosicion : index0to3);
    if (recortado == lowFreqIndex_)
        return;

    lowFreqIndex_ = recortado;
    eq_.setLowIndex(recortado);
}

void Equalizer::setLowGainDB(float gainDB) noexcept
{
    // El recorte y la banda muerta los hace el motor. Aqui solo se le pasa el
    // valor: duplicar el recorte seria tener la regla en dos sitios, y el sitio
    // que importa es el que esta en el modulo, que es el que tienen los demas.
    eq_.setLowGainDB(gainDB);
}

void Equalizer::setHighFreqIndex(int index0to3) noexcept
{
    const int recortado = index0to3 < 0 ? 0 : (index0to3 > kUltimaPosicion ? kUltimaPosicion : index0to3);
    if (recortado == highFreqIndex_)
        return;

    highFreqIndex_ = recortado;
    eq_.setHighIndex(recortado);
}

void Equalizer::setHighGainDB(float gainDB) noexcept
{
    eq_.setHighGainDB(gainDB);
}

void Equalizer::process(float &leftSample, float &rightSample) noexcept
{
    // Las dos repisas en cascada, con estado separado por canal en cada una.
    // El motor ya trae los cuatro estados y el bucle de audio del original, asi
    // que esto es una llamada y nada mas.
    eq_.processFrame(leftSample, rightSample);
}

}  // namespace ABDMS2000
