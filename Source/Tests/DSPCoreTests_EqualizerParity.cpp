/**
 * @purpose Paridad BIT A BIT del ecualizador del MS2000 tras migrarlo a
 *          `CascadeShelfEq<MS2000EqProfile>`.
 * @classification Test
 *
 * QUE COMPRUEBA, Y QUE NO.
 * Que las dos implementaciones producen los MISMOS bits, muestra a muestra y en
 * los dos canales, para el mismo guion de mandos y la misma señal. No es un test de
 * tolerancia: si un coeficiente cambia en el ultimo bit, el filtro entero cambia
 * y este test se pone rojo. Un test con tolerancia de -74 dBFS habria pasado
 * siempre y no habria dicho nada.
 *
 * POR QUE HAY UNA COPIA DEL CODIGO ANTIGUO EN ESTE FICHERO, Y NO UN GOLDEN FILE.
 * Un fichero de audio golden solo vale si la referencia se regenero con la
 * version buena, y en una migracion de este tipo la tentacion es regenerarlo
 * con la nueva y dar el asunto por bueno. La referencia va aqui, escrita a mano
 * y CONGELADA: si alguien cambia la copia, el test sigue comparando, pero ya no
 * esta midiendo la migracion, asi que la copia no se toca. Es el mismo
 * criterio que usa `namespace frozen` en `DspEffectsTests.cpp` del modulo.
 *
 * LO QUE NO SE COMPRUEBA, Y POR QUE NO SE PUEDE.
 * Que el codigo nuevo suene IGUAL que el ecualizador de 1997, con la libm de
 * aquella maquina. Eso ya se perdio en la migracion anterior, que sustituyo
 * `std::pow`, `std::sqrt`, `std::cos` y `std::sin` por las aproximaciones de
 * `DspCore/DspMath.h`, y esta medido y escrito en la cabecera de
 * `Source/DSP/Effects/Equalizer.h`: hasta 509 ulps en un coeficiente, 0,012 dB
 * de respuesta. Esta migracion no toca ninguna formula, y por eso no vuelve a
 * abrir ese numero.
 */
#include "TestCompat.h"
#include "../DSP/Effects/Equalizer.h"
#include "../DSP/Common/DSPUtils.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace ABDMS2000 {
namespace Tests {

//==============================================================================
/** LA COPIA CONGELADA. `Equalizer.{h,cpp}` tal como estaban antes de la
    migración, con sus dos `ShelfFilter` y sus dos tablas. Sin tocar. */
namespace congelado {

class Equalizer {
public:
    void prepare(double sampleRate) noexcept
    {
        const double valido = DSPUtils::validateSampleRate(sampleRate);

        bajo_.prepare (valido);
        alto_.prepare (valido);

        bajo_.setMode (abd::dsp::ShelfMode::Low);
        alto_.setMode (abd::dsp::ShelfMode::High);

        frecuenciaBaja_ = kMS2000LowFreqs[lowFreqIndex_];
        frecuenciaAlta_ = kMS2000HighFreqs[highFreqIndex_];
        aplicaFrecuencias();

        bajo_.setGainDB (0.0f);
        alto_.setGainDB (0.0f);

        reset();
    }

    void reset() noexcept
    {
        bajo_.reset();
        alto_.reset();
    }

    void setLowFreqIndex(int index0to3) noexcept
    {
        const int recortado = index0to3 < 0 ? 0 : (index0to3 > 3 ? 3 : index0to3);
        if (recortado == lowFreqIndex_)
            return;

        lowFreqIndex_ = recortado;
        frecuenciaBaja_ = kMS2000LowFreqs[recortado];
        aplicaFrecuencias();
    }

    void setLowGainDB(float gainDB) noexcept          { bajo_.setGainDB (gainDB); }
    void setHighFreqIndex(int index0to3) noexcept
    {
        const int recortado = index0to3 < 0 ? 0 : (index0to3 > 3 ? 3 : index0to3);
        if (recortado == highFreqIndex_)
            return;

        highFreqIndex_ = recortado;
        frecuenciaAlta_ = kMS2000HighFreqs[recortado];
        aplicaFrecuencias();
    }

    void setHighGainDB(float gainDB) noexcept         { alto_.setGainDB (gainDB); }

    void process(float& leftSample, float& rightSample) noexcept
    {
        bajo_.processFrame (leftSample, rightSample);
        alto_.processFrame (leftSample, rightSample);
    }

private:
    static constexpr float kMS2000LowFreqs[4]  = { 160.0f, 250.0f, 400.0f, 600.0f };
    static constexpr float kMS2000HighFreqs[4] = { 4000.0f, 6000.0f, 8000.0f, 12000.0f };

    int   lowFreqIndex_  = 1;
    int   highFreqIndex_ = 2;

    float frecuenciaBaja_ = 250.0f;
    float frecuenciaAlta_ = 8000.0f;

    void aplicaFrecuencias() noexcept
    {
        bajo_.setFrequencyHz (frecuenciaBaja_);
        alto_.setFrequencyHz (frecuenciaAlta_);
    }

    abd::dsp::ShelfFilter bajo_;
    abd::dsp::ShelfFilter alto_;
};

} // namespace congelado

//==============================================================================
/** Ruido reproducible. Un LCG con los parametros clasicos de Numerical Recipes:
    el mismo en las dos maquinas, y el mismo en cada ejecucion y en cada
    plataforma. Un `std::mt19937` sembrado con el reloj serviria para medir
    ruido, no para comparar dos motores muestra a muestra. */
class Generador {
public:
    explicit Generador (std::uint32_t semilla) : estado_ (semilla) {}

    float siguiente() noexcept
    {
        estado_ = estado_ * 1664525u + 1014535269u;
        return (float) ((estado_ >> 8) & 0xFFFFu) / 32768.0f - 1.0f;   // [-1, 1)
    }

private:
    std::uint32_t estado_;
};

//==============================================================================
enum TipoEvento
{
    eventoPrepare      = 0,
    eventoIndiceBajo   = 1,
    eventoGananciaBaja = 2,
    eventoIndiceAlto   = 3,
    eventoGananciaAlta = 4,
    eventoReset        = 5
};

struct Evento
{
    int   enMuestra = 0;     // 0 = antes de la primera muestra
    int   tipo      = eventoPrepare;
    int   valorInt  = 0;
    float valorFloat = 0.0f;
};

/** Los dos motores tienen los mismos nombres de metodo, asi que el guion se
    aplica con una sola plantilla. Si alguno cambiara de nombre, esto no
    compilaria, que es justo la aviso que se quiere antes que un test verde. */
template <typename Motor>
static inline void aplica (Motor& motor, const Evento& e) noexcept
{
    switch (e.tipo)
    {
        case eventoPrepare:      motor.prepare      (e.valorFloat); break;
        case eventoIndiceBajo:   motor.setLowFreqIndex (e.valorInt); break;
        case eventoGananciaBaja: motor.setLowGainDB    (e.valorFloat); break;
        case eventoIndiceAlto:   motor.setHighFreqIndex (e.valorInt); break;
        case eventoGananciaAlta: motor.setHighGainDB   (e.valorFloat); break;
        case eventoReset:        motor.reset(); break;
    }
}

//==============================================================================
struct Resultado
{
    long comparadas = 0;
    int  primeraDiferencia = -1;
    int  canalDiferente = -1;
    float antes = 0.0f;
    float ahora = 0.0f;
    int  eventos = 0;
};

/** Corre el mismo guion y la misma señal por los dos motores, y mira el
    resultado BIT A BIT. `bit_cast` a entero y no `==` a proposito: con `==`, un
    -0,0 donde antes habia un +0,0 pasaria por igual, y en un estado de un
    biquad eso ya es una diferencia de bits. */
template <typename Antes, typename Ahora>
static Resultado compara (Antes& antes, Ahora& ahora,
                          const std::vector<Evento>& guion,
                          int numMuestras,
                          std::uint32_t semillaL,
                          std::uint32_t semillaR)
{
    Resultado r;
    r.eventos = (int) guion.size();

    Generador genL (semillaL);
    Generador genR (semillaR);

    std::size_t siguienteEvento = 0;

    for (int i = 0; i < numMuestras; ++i)
    {
        // Los eventos de esta muestra se aplican a LOS DOS motores, y en el
        // mismo orden: el guion es el contrato, no una guia.
        while (siguienteEvento < guion.size() && guion[siguienteEvento].enMuestra == i)
        {
            aplica (antes, guion[siguienteEvento]);
            aplica (ahora,  guion[siguienteEvento]);
            ++siguienteEvento;
        }

        // La misma muestra entra en los dos motores. Se genera una vez y se
        // reparte: si se generara dos veces con el mismo generador, el segundo
        // motor recibiria la segunda mitad de la secuencia.
        float l = genL.siguiente();
        float b = genR.siguiente();

        float la = l, lb = b;
        float ha = l, hb = b;

        antes.process (la, lb);
        ahora.process  (ha, hb);

        r.comparadas += 2;

        if (r.primeraDiferencia < 0)
        {
            if (std::bit_cast<std::uint32_t> (la) != std::bit_cast<std::uint32_t> (ha))
            {
                r.primeraDiferencia = i;
                r.canalDiferente = 0;
                r.antes = la;
                r.ahora = ha;
            }
            else if (std::bit_cast<std::uint32_t> (lb) != std::bit_cast<std::uint32_t> (hb))
            {
                r.primeraDiferencia = i;
                r.canalDiferente = 1;
                r.antes = lb;
                r.ahora = hb;
            }
        }
    }

    return r;
}

/** Traduce el resultado a un `check` con un mensaje que dice DONDE ha fallado,
    no solo que ha fallado. Un test de paridad que dice "FAIL" sin decir en que
    muestra obliga a repetir el trabajo a mano. */
static void informa (bool ok, const Resultado& r, const char* que, long& total)
{
    total += r.comparadas;

    if (ok)
    {
        char msg[256];
        snprintf (msg, sizeof (msg), "%s: %ld muestras identicas bit a bit", que, r.comparadas);
        check (true, msg);
        return;
    }

    char msg[320];
    snprintf (msg, sizeof (msg),
              "%s: DIFIERE en la muestra %d (canal %d): antes %.9g, ahora %.9g  [0x%08X vs 0x%08X]",
              que, r.primeraDiferencia, r.canalDiferente, r.antes, r.ahora,
              std::bit_cast<std::uint32_t> (r.antes),
              std::bit_cast<std::uint32_t> (r.ahora));
    check (false, msg);
}

//==============================================================================
/**
    El caso que mas facilidad tiene de romperse, y el que mas veces se ha roto en
    la vida real de este shim: NO son las dos repisas, son los dos selectores.
    Con dieciseis combinaciones por sample rate, cualquier desacuerdo entre
    "que indice cree tener" y "que frecuencia tiene puesta" sale aqui. */
static void testParidadDePosiciones()
{
    long total = 0;
    const double sampleRates[3] = { 32000.0, 44100.0, 48000.0 };

    for (int sr = 0; sr < 3; ++sr)
    {
        for (int lo = 0; lo < 4; ++lo)
        {
            for (int hi = 0; hi < 4; ++hi)
            {
                congelado::Equalizer antes;
                Equalizer ahora;

                std::vector<Evento> guion;
                guion.push_back ({ 0, eventoPrepare, 0, (float) sampleRates[sr] });
                guion.push_back ({ 0, eventoIndiceBajo, lo, 0.0f });
                guion.push_back ({ 0, eventoIndiceAlto, hi, 0.0f });
                // La ganancia VARIAN con la posicion, para que las dieciseis
                // combinaciones no sean la misma prueba repetida dieciseis
                // veces con distinto nombre.
                guion.push_back ({ 0, eventoGananciaBaja, 0, -3.0f * (float) lo - 1.5f });
                guion.push_back ({ 0, eventoGananciaAlta, 0,  2.0f * (float) hi + 0.5f });

                char nombre[96];
                snprintf (nombre, sizeof (nombre),
                          "posiciones %d/%d a %.0f Hz", lo, hi, sampleRates[sr]);

                const Resultado r = compara (antes, ahora, guion, 1024, 0x13579BDFu, 0x2468ACE0u);
                informa (r.primeraDiferencia < 0, r, nombre, total);
            }
        }
    }

    char msg[128];
    snprintf (msg, sizeof (msg), "posiciones: %ld muestras comparadas en total", total);
    check (total == 16 * 3 * 2 * 1024, msg);
}

/**
    La ganancia con su BANDA MUERTA de 0,05 dB, que es una regla del motor y no
    del shim, y que por eso tiene que seguir cayendo en el mismo sitio. El
    barrido pasa por los dos lados del umbral, y a proposito se queda a +/-0,04
    dB del valor anterior (no se aplica) y a +/-0,06 dB (se aplica). */
static void testParidadDeGanancia()
{
    long total = 0;
    const double sampleRates[2] = { 44100.0, 32000.0 };

    for (int sr = 0; sr < 2; ++sr)
    {
        congelado::Equalizer antes;
        Equalizer ahora;

        std::vector<Evento> guion;
        guion.push_back ({ 0, eventoPrepare, 0, (float) sampleRates[sr] });
        guion.push_back ({ 0, eventoIndiceBajo, 1, 0.0f });
        guion.push_back ({ 0, eventoIndiceAlto, 2, 0.0f });

        // 0,02 dB por muestra durante 1201 muestras: pasa por los -12, por el
        // 0, por los +12, y va metiendo la banda muerta en cada paso.
        for (int i = 0; i < 1201; ++i)
        {
            const float db = -12.0f + 0.02f * (float) i;
            guion.push_back ({ i, eventoGananciaBaja, 0, db });
            guion.push_back ({ i, eventoGananciaAlta, 0, -db });
        }

        char nombre[96];
        snprintf (nombre, sizeof (nombre), "barrido de ganancia a %.0f Hz", sampleRates[sr]);

        const Resultado r = compara (antes, ahora, guion, 1201, 0x0BADC0DEu, 0x0BADC0DFu);
        informa (r.primeraDiferencia < 0, r, nombre, total);
    }

    // Los valores que NO tienen que mover nada: dentro de la banda muerta.
    {
        congelado::Equalizer antes;
        Equalizer ahora;

        std::vector<Evento> guion;
        guion.push_back ({ 0, eventoPrepare, 0, 44100.0f });
        guion.push_back ({ 0, eventoGananciaBaja, 0, 6.0f });
        guion.push_back ({ 0, eventoGananciaBaja, 0, 6.04f });   // dentro: no se aplica
        guion.push_back ({ 0, eventoGananciaBaja, 0, 6.06f });   // fuera: se aplica
        guion.push_back ({ 0, eventoGananciaBaja, 0, 6.02f });   // dentro otra vez

        const Resultado r = compara (antes, ahora, guion, 512, 0x5EED0001u, 0x5EED0002u);
        informa (r.primeraDiferencia < 0, r, "banda muerta de 0,05 dB en el borde", total);
    }
}

/**
    Cambios de indice CON AUDIO CORRIENDO, que es como pasa de verdad:
    `SynthEngine` escribe los cuatro mandos en cada actualizacion de parametro
    (ver `SynthEngine.cpp`), y la mayoria de las veces no ha cambiado nada.

    Aqui van tambien los valores FUERA DE RANGO, que es donde dos recortes
    distintos darian ids distintos: -1 y 4 tienen que acabar en la misma
    posicion en los dos motores. Y se repite un indice que ya estaba, porque la
    guarda de "no ha cambiado" es la linea mas fácil de equivocar de todo el
    shim. */
static void testParidadDeIndicesEnCaliente()
{
    long total = 0;

    const int secuencia[] = { 0, 3, 3, -1, 4, 1, 1, 2, 0, 99, -99, 3 };

    congelado::Equalizer antes;
    Equalizer ahora;

    std::vector<Evento> guion;
    guion.push_back ({ 0, eventoPrepare, 0, 48000.0f });

    for (int i = 0; i < (int) (sizeof (secuencia) / sizeof (secuencia[0])); ++i)
    {
        const int enMuestra = 128 * i;
        guion.push_back ({ enMuestra, eventoIndiceBajo, secuencia[i], 0.0f });
        guion.push_back ({ enMuestra, eventoIndiceAlto, secuencia[11 - i], 0.0f });
        guion.push_back ({ enMuestra, eventoGananciaBaja, 0, 4.0f * (float) i - 6.0f });
        guion.push_back ({ enMuestra, eventoGananciaAlta, 0, -3.0f * (float) i + 5.0f });
    }

    const Resultado r = compara (antes, ahora, guion, 2048, 0xC0FFEE01u, 0xC0FFEE02u);
    informa (r.primeraDiferencia < 0, r, "indices movidos en caliente, con recorte", total);
}

/**
    `reset` con audio dentro, y un impulso justo despues: si el estado de los
    cuatro interpoladores no se limpiara igual, el impulso sale distinto. */
static void testParidadDeReset()
{
    long total = 0;

    congelado::Equalizer antes;
    Equalizer ahora;

    std::vector<Evento> guion;
    guion.push_back ({ 0, eventoPrepare, 0, 44100.0f });
    guion.push_back ({ 0, eventoIndiceBajo, 0, 0.0f });
    guion.push_back ({ 0, eventoIndiceAlto, 3, 0.0f });
    guion.push_back ({ 0, eventoGananciaBaja, 0,  9.0f });
    guion.push_back ({ 0, eventoGananciaAlta, 0, -9.0f });
    guion.push_back ({ 500, eventoReset, 0, 0.0f });

    const Resultado r = compara (antes, ahora, guion, 1024, 0xABCD1234u, 0xABCD5678u);
    informa (r.primeraDiferencia < 0, r, "reset con audio dentro", total);
}

/**
    EL CASO POR EL QUE ESTE SHIM SIGUE EXISTIENDO, y el unico en el que el codigo
    nuevo hace algo mas que delegar.

    `CascadeShelfEq::prepare` pone los indices de FABRICA del perfil. El shim
    anterior conservaba la posicion que el panel tenia puesta. Son cosas
    distintas, y con la delegacion a secas el ecualizador volveria a 250 Hz y
    8 kHz por debajo del usuario en cuanto el host reiniciase el audio a mitad
    de sesion. Mismo sonido, distinta frecuencia, y por eso va con su propio
    caso en vez de darlo por supuesto. */
static void testPrepareConservaLaPosicionDelPanel()
{
    long total = 0;

    congelado::Equalizer antes;
    Equalizer ahora;

    std::vector<Evento> guion;
    guion.push_back ({ 0, eventoPrepare, 0, 44100.0f });
    guion.push_back ({ 0, eventoIndiceBajo, 3, 0.0f });     // 600 Hz
    guion.push_back ({ 0, eventoIndiceAlto, 0, 0.0f });     // 4 kHz
    guion.push_back ({ 0, eventoGananciaBaja, 0, 7.0f });
    guion.push_back ({ 0, eventoGananciaAlta, 0, -7.0f });
    guion.push_back ({ 300, eventoPrepare, 0, 44100.0f });   // el host reinicia el audio

    // Y AQUI, DELIBERADAMENTE, NO SE REESCRIBEN LOS INDICES. La primera version
    // de este caso los reescribia justo despues del `prepare`, y hacia que
    // pasara siempre: el guion tapaba justo lo que tenia que medir. Un test de
    // paridad que no puede fallar no es un test.
    //
    // Y LA GANANCIA, POR EL MOTIVO CONTRARIO, TAMPOCO SE PUEDE DEJAR A 0 dB.
    // `prepare` pone la ganancia a 0, y a 0 dB la repisa es la IDENTIDAD bit a
    // bit: con A = 1, `b0 = (2 + 2·alfa) / (2 + 2·alfa) = 1` y `b1 = a1`,
    // `b2 = a2` con la MISMA expresion a los dos lados, asi que la forma II
    // transpuesta devuelve `y = x` y los dos acumuladores se quedan en cero
    // muestra a muestra. O sea: con la ganancia a cero, perder la frecuencia
    // NO SE VE, porque da igual estar a 250 Hz o a 600 Hz. Por eso despues del
    // `prepare` se vuelve a poner la ganancia —que es lo que hace
    // `SynthEngine` en su siguiente actualizacion de parametro— y NO el indice.
    // Asi la frecuencia que hay que conservar es la que sostiene el sonido.
    guion.push_back ({ 300, eventoGananciaBaja, 0, 7.0f });
    guion.push_back ({ 300, eventoGananciaAlta, 0, -7.0f });

    const Resultado r = compara (antes, ahora, guion, 1024, 0xFEEDFACEu, 0xFEEDFACDu);
    informa (r.primeraDiferencia < 0, r, "prepare a mitad de sesion conserva la posicion", total);
}

/** El caso vacio, que tambien tiene que ser bit a bit: sin eventos, la salida
    tiene que ser identica desde el primer prepare. */
static void testParidadSinManearNada()
{
    long total = 0;

    for (int sr = 0; sr < 3; ++sr)
    {
        const double sampleRates[3] = { 32000.0, 44100.0, 48000.0 };
        congelado::Equalizer antes;
        Equalizer ahora;

        std::vector<Evento> guion;
        guion.push_back ({ 0, eventoPrepare, 0, (float) sampleRates[sr] });

        char nombre[96];
        snprintf (nombre, sizeof (nombre), "sin manear nada a %.0f Hz", sampleRates[sr]);

        const Resultado r = compara (antes, ahora, guion, 2048, 0x11111111u, 0x22222222u);
        informa (r.primeraDiferencia < 0, r, nombre, total);
    }
}

void testEqualizerParity()
{
    printf ("\n--- Equalizer: paridad bit a bit con la implementacion anterior ---\n");

    testParidadSinManearNada();
    testParidadDePosiciones();
    testParidadDeGanancia();
    testParidadDeIndicesEnCaliente();
    testParidadDeReset();
    testPrepareConservaLaPosicionDelPanel();
}

} // namespace Tests
} // namespace ABDMS2000
