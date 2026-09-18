#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include "../State/ParameterRegistry.gen.h"
#include "../DSP/Sequencer/ModSequencer.h"
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
#include "MS2000ProgramData.h"   // el bloque nativo del plugin (conversión §6.4)
using abd::hw::SysExCodec;
#if ABD_HAS_JUCE
#include <juce_audio_processors/juce_audio_processors.h>
#endif

namespace ABDMS2000 {

/**
 * @brief Programa de fábrica del Korg MS2000 / MS2000R / microKORG — 254 bytes reales.
 *
 * Este es el **formato del equipo físico**, no el del plugin. El plugin guarda sus
 * presets propios en `MS2000ProgramData` (bloque nativo del motor); esto de
 * aquí es lo que sale y entra por MIDI de un MS2000 de verdad.
 *
 * Mapa verificado contra un dump real (patch "INIT Program" del panel ReMS2000,
 * `DOCS/ReMS2000-main/ReMS2000.panel` → `initPatchData`, contrastado con
 * `applyProgramData` / `applyTimbreData` / `applyVocoderData` del mismo panel):
 *
 *   0x00..0x0B   Nombre (12 B, ASCII)                    ← el nombre va AQUÍ, no en 0x1C
 *   0x0C..0x0F   Sin uso
 *   0x10         Timbre Voice (bits 6,7) + Voice Mode (bits 4,5: 0 Single, 1 Split, 2 Layer, 3 Vocoder)
 *   0x11         Scale Key (bits 4..7) + Scale Type (bits 0..3)
 *   0x12         Split Point
 *   0x13..0x16   Delay FX: flags (bit 7 tempo sync, bits 0..3 time base), time, feedback, type
 *   0x17..0x19   Mod FX: speed, depth, type
 *   0x1A..0x1D   EQ: high freq, high gain (+64), low freq, low gain (+64)
 *   0x1E..0x24   Arpegiador: tempo MSB/LSB, flags, type+range, gate, resolution, swing
 *   0x25         Sin uso
 *   0x26..0x91   TIMBRE 1 (108 B)   → ver `ti`
 *   0x92..0xFD   TIMBRE 2 (108 B)   → mismo layout que Timbre 1
 *
 * El bloque de un timbre (108 B) tiene un significado distinto según el Voice Mode:
 * con `Vocoder` (3) sus bytes 15..29 y 46..77 son las bandas del vocoder
 * (`knobSeq1Step`/`knobSeq2Step` en ReMS2000 = nivel y pan de banda), no los
 * osciladores/filtro del timbre. Por eso hay dos tablas de mapeo al motor.
 *
 * La trama en el cable es `F0 42 3n 58 40 [254 B empaquetados 7→8] F7`; el
 * empaquetado de Korg **no rellena** el último grupo parcial: 254 B → 291 B
 * (`abd::hw::SysExCodec` ya lo hace así, a diferencia del codec de ABDBankManager).
 *
 * Qué hace con el motor (`applyToAPVTS`) y qué no: ver `unmodelled()`.
 */
struct MS2000HardwareProgram
{
    // ─── Mapa global ─────────────────────────────────────────────────────────
    static constexpr size_t PROGRAM_SIZE = 254;
    static constexpr size_t NAME_OFFSET  = 0x00;
    static constexpr size_t NAME_LENGTH  = 12;
    static constexpr size_t GLOBAL_FLAGS = 0x10;
    static constexpr size_t SCALE_BYTE   = 0x11;
    static constexpr size_t SPLIT_POINT  = 0x12;
    static constexpr size_t DELAY_FLAGS  = 0x13;
    static constexpr size_t DELAY_TIME   = 0x14;
    static constexpr size_t DELAY_DEPTH  = 0x15;
    static constexpr size_t DELAY_TYPE   = 0x16;
    static constexpr size_t MODFX_SPEED  = 0x17;
    static constexpr size_t MODFX_DEPTH  = 0x18;
    static constexpr size_t MODFX_TYPE   = 0x19;
    static constexpr size_t EQ_HI_FREQ   = 0x1A;
    static constexpr size_t EQ_HI_GAIN   = 0x1B;
    static constexpr size_t EQ_LO_FREQ   = 0x1C;
    static constexpr size_t EQ_LO_GAIN   = 0x1D;
    static constexpr size_t ARP_TEMPO_MSB= 0x1E;
    static constexpr size_t ARP_TEMPO_LSB= 0x1F;
    static constexpr size_t ARP_FLAGS    = 0x20;
    static constexpr size_t ARP_TYPE_RNG = 0x21;
    static constexpr size_t ARP_GATE     = 0x22;
    static constexpr size_t ARP_RES      = 0x23;
    static constexpr size_t ARP_SWING    = 0x24;

    // Segmentos de timbre
    static constexpr size_t TIMBRE1_START = 0x26; // 38
    static constexpr size_t TIMBRE2_START = 0x92; // 146
    static constexpr size_t TIMBRE_SIZE   = 108;
    static constexpr size_t TIMBRES_START = TIMBRE1_START;
    static constexpr size_t TIMBRES_SIZE  = PROGRAM_SIZE - TIMBRE1_START; // 216 = 2 × 108

    /** Offsets dentro de un bloque de timbre de 108 B. */
    struct ti
    {
        enum : size_t
        {
            MIDI_CH = 0,
            FLAGS = 1,          // bits 6,7 assign | bit 5 EG2 reset | bit 4 EG1 reset | bit 3 trigger | bits 0,1 priority
            UNISON_DETUNE = 2,
            TUNE = 3,           // +64
            BEND_RANGE = 4,     // +64
            TRANSPOSE = 5,      // +64
            VIBRATO = 6,        // +64
            OSC1_WAVE = 7,
            OSC1_CTRL1 = 8,
            OSC1_CTRL2 = 9,
            OSC1_DWGS = 10,
            OSC2_FLAGS = 12,    // bits 4,5 mod select | bits 0,1 wave
            OSC2_SEMITONE = 13, // +64
            OSC2_TUNE = 14,     // +64
            PORTAMENTO = 15,    // bits 0..6
            MIX_OSC1 = 16,
            MIX_OSC2 = 17,
            // 18..20 = vocoder: HPF level / gate sense / threshold
            MIX_NOISE = 18,     // en modo synth; en vocoder = HPF level
            FILTER_TYPE = 19,
            CUTOFF = 20,
            RESONANCE = 21,
            FILTER_EG1_INT = 22,    // +64  (en vocoder = filter shift 0..4)
            FILTER_VELO = 23,      // +64
            FILTER_KEYTRACK = 24,  // +64
            AMP_LEVEL = 25,
            AMP_PAN = 26,          // +64 (64 = centro)
            AMP_FLAGS = 27,        // bit 6 amp SW (EG2/Gate) | bit 0 distorsión
            AMP_VELO = 28,         // +64
            AMP_KEYTRACK = 29,     // +64
            EG1_ATTACK = 30, EG1_DECAY = 31, EG1_SUSTAIN = 32, EG1_RELEASE = 33,
            EG2_ATTACK = 34, EG2_DECAY = 35, EG2_SUSTAIN = 36, EG2_RELEASE = 37,
            LFO1_FLAGS = 38, LFO1_FREQ = 39, LFO1_FLAGS2 = 40,
            LFO2_FLAGS = 41, LFO2_FREQ = 42, LFO2_FLAGS2 = 43,
            PATCH1 = 44, PATCH1_INT = 45, PATCH2 = 46, PATCH2_INT = 47,
            PATCH3 = 48, PATCH3_INT = 49, PATCH4 = 50, PATCH4_INT = 51,
            SEQ_FLAGS = 52, SEQ_FLAGS2 = 53,
            SEQ1_KNOB = 54, SEQ1_MOTION = 55, SEQ1_STEPS = 56,
            SEQ2_KNOB = 72, SEQ2_MOTION = 73, SEQ2_STEPS = 74,
            SEQ3_KNOB = 90, SEQ3_MOTION = 91, SEQ3_STEPS = 92,
            // En modo vocoder: 46..61 = nivel de las 16 bandas; 62..77 = pan (+64)
            VOCODER_BAND_LEVELS = 46,
            VOCODER_BAND_PANS  = 62
        };
    };

    /** Voice Mode del programa (byte global 0x10, bits 4,5). */
    enum class VoiceMode : uint8_t { Single = 0, Split = 1, Layer = 2, Vocoder = 3 };

    std::array<uint8_t, PROGRAM_SIZE> raw{};

    MS2000HardwareProgram() { loadInitProgram(); }

    // ─── Plantilla "INIT Program" ────────────────────────────────────────────
    /** Valores del patch "INIT Program" del equipo real (ReMS2000 `initPatchData`). */
    void loadInitProgram()
    {
        static const uint8_t kInit[PROGRAM_SIZE] = {
            0x49, 0x4e, 0x49, 0x54, 0x20, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x3c, 0x05, 0x28, 0x00, 0x00, 0x14,
            0x00, 0x00, 0x14, 0x40, 0x0f, 0x40, 0x00, 0x78, 0x00, 0x00, 0x50, 0x01,
            0x00, 0x00, 0xff, 0x70, 0x0a, 0x40, 0x42, 0x40, 0x45, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x14,
            0x40, 0x40, 0x40, 0x7f, 0x40, 0x00, 0x40, 0x40, 0x00, 0x40, 0x7f, 0x00,
            0x00, 0x40, 0x7f, 0x00, 0x02, 0x0a, 0x03, 0x02, 0x46, 0x0c, 0x02, 0x40,
            0x03, 0x40, 0x42, 0x40, 0x43, 0x40, 0x43, 0xf1, 0x01, 0x01, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x40, 0xff, 0x70, 0x0a, 0x40, 0x42, 0x40, 0x45, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x7f, 0x00, 0x00, 0x01, 0x7f, 0x14,
            0x40, 0x40, 0x40, 0x7f, 0x40, 0x00, 0x40, 0x40, 0x00, 0x40, 0x7f, 0x00,
            0x00, 0x40, 0x7f, 0x00, 0x02, 0x0a, 0x03, 0x02, 0x46, 0x0c, 0x02, 0x40,
            0x03, 0x40, 0x42, 0x40, 0x43, 0x40, 0x43, 0xf1, 0x01, 0x01, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x40, 0x40,
        };
        std::memcpy(raw.data(), kInit, PROGRAM_SIZE);
    }

    void reset() { loadInitProgram(); }

    // ─── Acceso ──────────────────────────────────────────────────────────────
    uint8_t  get(size_t offset) const { return raw[offset]; }
    void     set(size_t offset, uint8_t value) { raw[offset] = value; }
    uint8_t* timbre(size_t index) { return raw.data() + TIMBRE1_START + (index * TIMBRE_SIZE); }
    const uint8_t* timbre(size_t index) const { return raw.data() + TIMBRE1_START + (index * TIMBRE_SIZE); }

    uint8_t field(size_t timbreIndex, size_t fieldOffset) const { return timbre(timbreIndex)[fieldOffset]; }
    void    setField(size_t timbreIndex, size_t fieldOffset, uint8_t value) { timbre(timbreIndex)[fieldOffset] = value; }

    VoiceMode getVoiceMode() const
    {
        return static_cast<VoiceMode>((raw[GLOBAL_FLAGS] >> 4) & 0x03);
    }

    void setVoiceMode(VoiceMode mode)
    {
        raw[GLOBAL_FLAGS] = static_cast<uint8_t>((raw[GLOBAL_FLAGS] & ~0x30)
            | ((static_cast<uint8_t>(mode) & 0x03) << 4));
    }

    bool isVocoderProgram() const { return getVoiceMode() == VoiceMode::Vocoder; }

    std::string getName() const
    {
        std::string res;
        res.reserve(NAME_LENGTH);
        for (size_t i = 0; i < NAME_LENGTH; ++i)
        {
            const char c = static_cast<char>(raw[NAME_OFFSET + i]);
            res += (c >= 32 && c <= 126) ? c : ' ';
        }
        while (!res.empty() && res.back() == ' ') res.pop_back();
        return res;
    }

    void setName(const std::string& newName)
    {
        std::string padded = newName.substr(0, NAME_LENGTH);
        while (padded.length() < NAME_LENGTH) padded += ' ';
        for (size_t i = 0; i < NAME_LENGTH; ++i)
            raw[NAME_OFFSET + i] = static_cast<uint8_t>(padded[i]);
    }

    // ─── Trama SysEx ─────────────────────────────────────────────────────────
    /** Desempaqueta 254 B de un payload 7→8 real (291 B; tolera el último grupo parcial). */
    bool unpackFromSysexPayload(const uint8_t* payload, size_t payloadLen)
    {
        std::vector<uint8_t> unpacked;
        if (!SysExCodec::unpack7to8(payload, payloadLen, unpacked) || unpacked.empty())
            return false;
        if (unpacked.size() < PROGRAM_SIZE) return false;
        std::copy_n(unpacked.begin(), PROGRAM_SIZE, raw.begin());
        return true;
    }

    /** Empaqueta los 254 B al payload 7→8 real (291 B con el último grupo parcial sin rellenar). */
    bool packToSysexPayload(std::vector<uint8_t>& outPayload) const
    {
        return SysExCodec::pack8to7(raw.data(), PROGRAM_SIZE, outPayload);
    }

    /**
     * Byte `3n` de la cabecera de Korg: `0x30 | (canal - 1)` (canal 1 -> `0x30`).
     *
     * Convención consolidada con el contrato del Bank Manager: canal 1 -> `0x30`
     * y canal 16 -> `0x3F`. La fórmula está compartida por las tramas C++ y TS;
     * cualquier validación frente a un MS2000 real queda documentada en ROADMAP §4.
     */
    static uint8_t korgChannelByte(int channel)
    {
        const int ch = std::max(1, std::min(16, channel));
        return static_cast<uint8_t>(0x30 | (ch - 1));
    }

    /** Trama completa `F0 42 3n 58 40 [payload] F7`. */
    std::vector<uint8_t> buildProgramDump(int channel = 1, uint8_t function = 0x40) const
    {
        std::vector<uint8_t> packed;
        if (!packToSysexPayload(packed)) return {};

        std::vector<uint8_t> sysex;
        sysex.reserve(6 + packed.size());
        sysex.push_back(0xF0);
        sysex.push_back(0x42);                                       // Korg
        sysex.push_back(korgChannelByte(channel));
        sysex.push_back(0x58);                                       // MS2000 / MS2000R
        sysex.push_back(function);                                   // 0x40 = Program Data Dump
        sysex.insert(sysex.end(), packed.begin(), packed.end());
        sysex.push_back(0xF7);
        return sysex;
    }

    /** Longitud del payload empaquetado del programa real (254 B → 291 B). */
    static size_t packedPayloadSize() { return ((PROGRAM_SIZE + 6) / 7) * 8 - (7 - (PROGRAM_SIZE % 7)); }

    // ─── All Data Dump (`0x4C`): la memoria completa del equipo ──────────────

    /**
     * Comando del volcado completo de la memoria (`MS2000_SysEx_Spec.md` §2).
     * Se llama `CMD_` por el mismo motivo que los acuses: `0x4C` no es un offset.
     */
    static constexpr uint8_t CMD_ALL_DATA_DUMP = 0x4C;

    /**
     * Longitud del payload empaquetado de un All Data Dump de `count` programas.
     *
     * El empaquetado de Korg se aplica al **flujo completo** (los `count × 254` B
     * concatenados), no a cada programa por separado. La diferencia importa: 254 no es
     * múltiplo de 7, así que empaquetar cada programa y concatenar los trozos deja los
     * bytes de MSBs en otro sitio y el receptor descodifica otra cosa. Con 128 programas
     * el flujo son 32 512 B → 37 157 B de payload (y 37 163 B de trama), mientras que
     * 128 trozos independientes darían 37 248 B.
     *
     * ⚠️ Hipótesis **pendiente de verificar contra hardware real**: el receptor de este
     * repo (`SysExManager`, comando `0x4C`) deshace el volcado como flujo único, que es
     * lo coherente con esto, pero nadie ha grabado todavía un All Data Dump del MS2000
     * para confirmarlo (ver el roadmap).
     */
    static size_t allDataDumpPayloadSize(size_t count)
    {
        const size_t total = count * PROGRAM_SIZE;
        const size_t remainder = total % 7;
        return ((total / 7) * 8) + (remainder == 0 ? 0 : remainder + 1);
    }

    /**
     * Trama de All Data Dump: `F0 42 3n 58 4C [count × 254 B empaquetados] F7`.
     *
     * Es la memoria completa del equipo (los 128 programas de la máquina) con la
     * cabecera de Korg. Devuelve vacío si no hay programas: un volcado sin memoria no
     * significa nada y no se manda.
     */
    static std::vector<uint8_t> buildAllDataDump(int channel,
                                                 const std::vector<MS2000HardwareProgram>& programs)
    {
        if (programs.empty()) return {};

        std::vector<uint8_t> unpacked;
        unpacked.reserve(programs.size() * PROGRAM_SIZE);
        for (const auto& program : programs)
            unpacked.insert(unpacked.end(), program.raw.begin(), program.raw.end());

        std::vector<uint8_t> packed;
        if (!SysExCodec::pack8to7(unpacked.data(), unpacked.size(), packed)) return {};

        std::vector<uint8_t> sysex;
        sysex.reserve(6 + packed.size());
        sysex.push_back(0xF0);
        sysex.push_back(0x42);                                       // Korg
        sysex.push_back(korgChannelByte(channel));
        sysex.push_back(0x58);                                       // MS2000 / MS2000R
        sysex.push_back(CMD_ALL_DATA_DUMP);
        sysex.insert(sysex.end(), packed.begin(), packed.end());
        sysex.push_back(0xF7);
        return sysex;
    }

    // Acuse de escritura del equipo (`MS2000_SysEx_Spec.md` §2): el MS2000 real manda
    // `0x23` Write Completed tras guardar un programa recibido y `0x24` Write Error si
    // no pudo. Ojo: `ARP_RES`/`ARP_SWING` de arriba son **offsets** del programa, no
    // comandos; de ahí el prefijo `CMD_`.
    static constexpr uint8_t CMD_WRITE_COMPLETED = 0x23;
    static constexpr uint8_t CMD_WRITE_ERROR     = 0x24;

    /** `F0 42 3n 58 [23|24] F7` — acuse de 6 B, sin payload, en el canal del emisor. */
    static std::vector<uint8_t> buildWriteAcknowledgement(int channel, bool ok)
    {
        return { 0xF0, 0x42, korgChannelByte(channel), 0x58,
                 ok ? CMD_WRITE_COMPLETED : CMD_WRITE_ERROR, 0xF7 };
    }

    // ─── Mapeo al motor ──────────────────────────────────────────────────────

    /**
     * Cómo se convierte un byte/bit del dump al valor del parámetro del motor.
     *
     * El Timbre 2 usa **la misma tabla** que el Timbre 1: el programa real guarda dos
     * bloques de 108 B idénticos en estructura (0x26 y 0x92), y el id del parámetro se
     * resuelve cambiando el prefijo (`osc1Wave` → `t2Osc1Wave`).
     */
    enum class Xform : uint8_t
    {
        Raw,          // 0..127 igual
        Signed,       // byte - 64
        Bit,          // bit suelto
        EqFreq,       // 0..29 real → 0..3 del motor (aproximación declarada)
        EqGain,       // +64 igual que el motor
        ArpRes,       // 0..5 real → índice del enum del motor (1/24 … 1/4)
        ArpRange,     // 0..3 real (1..4 octavas) → 1..4 del motor
        FormantShift, // 0..4 real (0,+1,+2,-1,-2) → 0..4 del motor (-2,-1,0,+1,+2)
        Duration127,  // bits 0..6 (portamento)
        SeqLastStep,  // bits 4..7 almacenan último paso − 1 → 1..16 del motor
        SeqMotionInv  // bit 0 con el sentido invertido (real 0 = Smooth, motor 0 = Step)
    };

    struct Field
    {
        size_t   offset;    // offset absoluto en el programa (ya sumado el timbre)
        uint8_t  bitLo;
        uint8_t  bitHi;     // inclusivo; bitHi < bitLo = byte completo
        const char* paramId;
        Xform    xform;
    };

    static uint8_t extractBits(uint8_t byte, uint8_t lo, uint8_t hi)
    {
        if (hi < lo) return byte;
        const uint8_t mask = static_cast<uint8_t>(((1u << (hi - lo + 1)) - 1u) << lo);
        return static_cast<uint8_t>((byte & mask) >> lo);
    }

    static void insertBits(uint8_t& byte, uint8_t lo, uint8_t hi, uint8_t value)
    {
        if (hi < lo) { byte = value; return; }
        const uint8_t mask = static_cast<uint8_t>(((1u << (hi - lo + 1)) - 1u) << lo);
        byte = static_cast<uint8_t>((byte & ~mask) | ((value << lo) & mask));
    }

    /**
     * Solo los campos que comparten byte con otros usan rango de bits; el resto
     * ocupa el byte entero (`bitLo`/`bitHi` quedan ahí como documentación).
     */
    static bool usesBitRange(Xform xform)
    {
        // ArpRange comparte byte con el tipo de arpegio (bits 4..7), así que
        // siempre se lee por rango; los demás ocupan el byte entero.
        return xform == Xform::Bit || xform == Xform::Duration127 || xform == Xform::ArpRange
            || xform == Xform::SeqLastStep || xform == Xform::SeqMotionInv;
    }

    /** `base` = 0 para el Timbre 1, `TIMBRE_SIZE` para el Timbre 2. */
    uint8_t readField(const Field& f, size_t base = 0) const
    {
        const uint8_t byte = raw[f.offset + base];
        return usesBitRange(f.xform) ? extractBits(byte, f.bitLo, f.bitHi) : byte;
    }

    void writeField(const Field& f, uint8_t value, size_t base = 0)
    {
        uint8_t& byte = raw[f.offset + base];
        if (usesBitRange(f.xform)) insertBits(byte, f.bitLo, f.bitHi, value);
        else byte = value;
    }

    /** ¿El campo vive dentro de un bloque de timbre (y por tanto existe dos veces)? */
    static bool isTimbreField(const Field& f)
    {
        return f.offset >= TIMBRE1_START && f.offset < TIMBRE1_START + TIMBRES_SIZE;
    }

    /** Tabla real → motor del Timbre 1 interpretado como **synth** (+ globales). */
    static const std::vector<Field>& synthFields()
    {
        static const std::vector<Field> fields = {
            // globales
            { GLOBAL_FLAGS, 4, 5, nullptr, Xform::Bit },                      // voice mode (tratado aparte)
            { GLOBAL_FLAGS, 6, 7, ParamIDs::timbreVoices,  Xform::Bit },      // reparto de las 4 voces
            { SCALE_BYTE,   4, 7, ParamIDs::scaleKey,      Xform::Bit },
            { SCALE_BYTE,   0, 3, ParamIDs::scaleType,     Xform::Bit },
            { SPLIT_POINT,  0, 0, ParamIDs::splitPoint,    Xform::Raw },
            { DELAY_TIME,   0, 0, ParamIDs::delayTime,     Xform::Raw },
            { DELAY_DEPTH,  0, 0, ParamIDs::delayFeedback, Xform::Raw },
            { DELAY_TYPE,   0, 0, ParamIDs::delayType,     Xform::Raw },
            { MODFX_SPEED,  0, 0, ParamIDs::modFxSpeed,    Xform::Raw },
            { MODFX_DEPTH,  0, 0, ParamIDs::modFxDepth,    Xform::Raw },
            { MODFX_TYPE,   0, 0, ParamIDs::modFxType,     Xform::Raw },
            { EQ_HI_FREQ,   0, 0, ParamIDs::eqHighFreq,    Xform::EqFreq },
            { EQ_HI_GAIN,   0, 0, ParamIDs::eqHighGain,    Xform::EqGain },
            { EQ_LO_FREQ,   0, 0, ParamIDs::eqLowFreq,     Xform::EqFreq },
            { EQ_LO_GAIN,   0, 0, ParamIDs::eqLowGain,     Xform::EqGain },
            { ARP_FLAGS,    7, 7, ParamIDs::arpOn,         Xform::Bit },
            { ARP_FLAGS,    6, 6, ParamIDs::arpLatch,      Xform::Bit },
            { ARP_TYPE_RNG, 0, 3, ParamIDs::arpType,       Xform::Bit },
            { ARP_TYPE_RNG, 4, 7, ParamIDs::arpRange,      Xform::ArpRange },
            { ARP_GATE,     0, 0, ParamIDs::arpGate,       Xform::Raw },
            { ARP_RES,      0, 0, ParamIDs::arpResolution, Xform::ArpRes },
            // timbre 1 — voz y osciladores
            { TIMBRE1_START + ti::FLAGS,        6, 7, ParamIDs::voiceMode,     Xform::Bit },
            { TIMBRE1_START + ti::UNISON_DETUNE,0, 0, ParamIDs::unisonDetune,  Xform::Raw },
            { TIMBRE1_START + ti::PORTAMENTO,   0, 6, ParamIDs::portamentoTime,Xform::Duration127 },
            { TIMBRE1_START + ti::OSC1_WAVE,    0, 0, ParamIDs::osc1Wave,      Xform::Raw },
            { TIMBRE1_START + ti::OSC1_CTRL1,   0, 0, ParamIDs::osc1Ctrl1,     Xform::Raw },
            { TIMBRE1_START + ti::OSC1_CTRL2,   0, 0, ParamIDs::osc1Ctrl2,     Xform::Raw },
            { TIMBRE1_START + ti::OSC1_DWGS,    0, 0, ParamIDs::osc1DwgsWave,  Xform::Raw },
            { TIMBRE1_START + ti::OSC2_FLAGS,   4, 5, ParamIDs::osc2ModType,   Xform::Bit },
            { TIMBRE1_START + ti::OSC2_FLAGS,   0, 1, ParamIDs::osc2Wave,      Xform::Bit },
            { TIMBRE1_START + ti::OSC2_SEMITONE,0, 0, ParamIDs::osc2Semitone,  Xform::Signed },
            { TIMBRE1_START + ti::OSC2_TUNE,    0, 0, ParamIDs::osc2Tune,      Xform::Signed },
            { TIMBRE1_START + ti::MIX_OSC1,     0, 0, ParamIDs::mixOsc1Level,  Xform::Raw },
            { TIMBRE1_START + ti::MIX_OSC2,     0, 0, ParamIDs::mixOsc2Level,  Xform::Raw },
            { TIMBRE1_START + ti::MIX_NOISE,    0, 0, ParamIDs::mixNoiseLevel, Xform::Raw },
            // timbre 1 — filtro, amp, EG, LFO
            { TIMBRE1_START + ti::FILTER_TYPE,  0, 0, ParamIDs::filterType,      Xform::Raw },
            { TIMBRE1_START + ti::CUTOFF,       0, 0, ParamIDs::filterCutoff,    Xform::Raw },
            { TIMBRE1_START + ti::RESONANCE,    0, 0, ParamIDs::filterResonance, Xform::Raw },
            { TIMBRE1_START + ti::FILTER_EG1_INT,0, 0, ParamIDs::filterEg1Int,   Xform::Signed },
            { TIMBRE1_START + ti::FILTER_KEYTRACK,0, 0, ParamIDs::filterKeyTrack,Xform::Signed },
            { TIMBRE1_START + ti::FILTER_VELO,  0, 0, ParamIDs::filterVelo,    Xform::Signed },
            { TIMBRE1_START + ti::AMP_LEVEL,    0, 0, ParamIDs::ampLevel,        Xform::Raw },
            { TIMBRE1_START + ti::AMP_PAN,      0, 0, ParamIDs::ampPan,          Xform::Signed },
            { TIMBRE1_START + ti::AMP_FLAGS,    0, 0, ParamIDs::ampDistortion,   Xform::Bit },
            { TIMBRE1_START + ti::AMP_KEYTRACK, 0, 0, ParamIDs::ampKeyTrack,     Xform::Signed },
            { TIMBRE1_START + ti::AMP_VELO,     0, 0, ParamIDs::ampVelo,         Xform::Signed },
            { TIMBRE1_START + ti::EG1_ATTACK,   0, 0, ParamIDs::eg1Attack,  Xform::Raw },
            { TIMBRE1_START + ti::EG1_DECAY,    0, 0, ParamIDs::eg1Decay,   Xform::Raw },
            { TIMBRE1_START + ti::EG1_SUSTAIN,  0, 0, ParamIDs::eg1Sustain, Xform::Raw },
            { TIMBRE1_START + ti::EG1_RELEASE,  0, 0, ParamIDs::eg1Release, Xform::Raw },
            { TIMBRE1_START + ti::EG2_ATTACK,   0, 0, ParamIDs::eg2Attack,  Xform::Raw },
            { TIMBRE1_START + ti::EG2_DECAY,    0, 0, ParamIDs::eg2Decay,   Xform::Raw },
            { TIMBRE1_START + ti::EG2_SUSTAIN,  0, 0, ParamIDs::eg2Sustain, Xform::Raw },
            { TIMBRE1_START + ti::EG2_RELEASE,  0, 0, ParamIDs::eg2Release, Xform::Raw },
            { TIMBRE1_START + ti::LFO1_FLAGS,   0, 1, ParamIDs::lfo1Wave,     Xform::Bit },
            { TIMBRE1_START + ti::LFO1_FLAGS,   4, 5, ParamIDs::lfo1KeySync,  Xform::Bit },
            { TIMBRE1_START + ti::LFO1_FREQ,    0, 0, ParamIDs::lfo1Freq,     Xform::Raw },
            { TIMBRE1_START + ti::LFO1_FLAGS2,  7, 7, ParamIDs::lfo1TempoSync,Xform::Bit },
            { TIMBRE1_START + ti::LFO1_FLAGS2,  0, 4, ParamIDs::lfo1SyncNote, Xform::Bit },
            { TIMBRE1_START + ti::LFO2_FLAGS,   0, 1, ParamIDs::lfo2Wave,     Xform::Bit },
            { TIMBRE1_START + ti::LFO2_FLAGS,   4, 5, ParamIDs::lfo2KeySync,  Xform::Bit },
            { TIMBRE1_START + ti::LFO2_FREQ,    0, 0, ParamIDs::lfo2Freq,     Xform::Raw },
            { TIMBRE1_START + ti::LFO2_FLAGS2,  7, 7, ParamIDs::lfo2TempoSync,Xform::Bit },
            { TIMBRE1_START + ti::LFO2_FLAGS2,  0, 4, ParamIDs::lfo2SyncNote, Xform::Bit },
            // timbre 1 — virtual patch
            { TIMBRE1_START + ti::PATCH1,       0, 3, ParamIDs::patch1Source,      Xform::Bit },
            { TIMBRE1_START + ti::PATCH1,       4, 7, ParamIDs::patch1Destination, Xform::Bit },
            { TIMBRE1_START + ti::PATCH1_INT,   0, 0, ParamIDs::patch1Intensity,   Xform::Signed },
            { TIMBRE1_START + ti::PATCH2,       0, 3, ParamIDs::patch2Source,      Xform::Bit },
            { TIMBRE1_START + ti::PATCH2,       4, 7, ParamIDs::patch2Destination, Xform::Bit },
            { TIMBRE1_START + ti::PATCH2_INT,   0, 0, ParamIDs::patch2Intensity,   Xform::Signed },
            { TIMBRE1_START + ti::PATCH3,       0, 3, ParamIDs::patch3Source,      Xform::Bit },
            { TIMBRE1_START + ti::PATCH3,       4, 7, ParamIDs::patch3Destination, Xform::Bit },
            { TIMBRE1_START + ti::PATCH3_INT,   0, 0, ParamIDs::patch3Intensity,   Xform::Signed },
            { TIMBRE1_START + ti::PATCH4,       0, 3, ParamIDs::patch4Source,      Xform::Bit },
            { TIMBRE1_START + ti::PATCH4,       4, 7, ParamIDs::patch4Destination, Xform::Bit },
            { TIMBRE1_START + ti::PATCH4_INT,   0, 0, ParamIDs::patch4Intensity,   Xform::Signed },
            // timbre 1 — mod sequencer (cabecera; las 3 filas van en `seqRow()`)
            { TIMBRE1_START + ti::SEQ_FLAGS,    7, 7, ParamIDs::modSeqOn,         Xform::Bit },
            { TIMBRE1_START + ti::SEQ_FLAGS,    0, 4, ParamIDs::modSeqResolution, Xform::Bit },
            { TIMBRE1_START + ti::SEQ_FLAGS2,   4, 7, ParamIDs::seqLastStep,      Xform::SeqLastStep },
            { TIMBRE1_START + ti::SEQ_FLAGS2,   2, 3, ParamIDs::modSeqType,       Xform::Bit },
            { TIMBRE1_START + ti::SEQ_FLAGS2,   0, 1, ParamIDs::seqKeySync,       Xform::Bit }
        };
        return fields;
    }

    /** Tabla real → motor del Timbre 1 **en modo vocoder** (bandas y vocoder). */
    static const std::vector<Field>& vocoderFields()
    {
        static const std::vector<Field> fields = {
            { TIMBRE1_START + ti::MIX_OSC1, 0, 0, ParamIDs::mixOsc1Level, Xform::Raw },
            { TIMBRE1_START + ti::MIX_OSC2, 0, 0, ParamIDs::mixOsc2Level, Xform::Raw },
            { TIMBRE1_START + ti::MIX_NOISE,0, 0, ParamIDs::vocoderHpfLevel, Xform::Raw },
            { TIMBRE1_START + ti::FILTER_TYPE, 0, 0, ParamIDs::filterType,   Xform::Raw },
            { TIMBRE1_START + ti::CUTOFF,      0, 0, ParamIDs::filterCutoff, Xform::Raw },
            { TIMBRE1_START + ti::RESONANCE,   0, 0, ParamIDs::filterResonance, Xform::Raw },
            { TIMBRE1_START + ti::FILTER_EG1_INT, 0, 0, ParamIDs::vocoderFormantShift, Xform::FormantShift },
            { TIMBRE1_START + ti::FILTER_VELO,    0, 0, ParamIDs::vocoderGateSense,    Xform::Raw },
            { TIMBRE1_START + ti::AMP_LEVEL,   0, 0, ParamIDs::ampLevel, Xform::Raw },
            { TIMBRE1_START + ti::AMP_PAN,     0, 0, ParamIDs::ampPan,   Xform::Signed },
            { TIMBRE1_START + ti::AMP_FLAGS,   0, 0, ParamIDs::ampDistortion, Xform::Bit },
            { TIMBRE1_START + ti::AMP_VELO,    0, 0, ParamIDs::vocoderDirectLevel, Xform::Raw },
            { TIMBRE1_START + ti::EG1_ATTACK,  0, 0, ParamIDs::eg1Attack,  Xform::Raw },
            { TIMBRE1_START + ti::EG1_DECAY,   0, 0, ParamIDs::eg1Decay,   Xform::Raw },
            { TIMBRE1_START + ti::EG1_SUSTAIN, 0, 0, ParamIDs::eg1Sustain, Xform::Raw },
            { TIMBRE1_START + ti::EG1_RELEASE, 0, 0, ParamIDs::eg1Release, Xform::Raw },
            { TIMBRE1_START + ti::EG2_ATTACK,  0, 0, ParamIDs::eg2Attack,  Xform::Raw },
            { TIMBRE1_START + ti::EG2_DECAY,   0, 0, ParamIDs::eg2Decay,   Xform::Raw },
            { TIMBRE1_START + ti::EG2_SUSTAIN, 0, 0, ParamIDs::eg2Sustain, Xform::Raw },
            { TIMBRE1_START + ti::EG2_RELEASE, 0, 0, ParamIDs::eg2Release, Xform::Raw },
            { TIMBRE1_START + ti::LFO1_FLAGS,  0, 1, ParamIDs::lfo1Wave,    Xform::Bit },
            { TIMBRE1_START + ti::LFO1_FLAGS,  4, 5, ParamIDs::lfo1KeySync, Xform::Bit },
            { TIMBRE1_START + ti::LFO1_FREQ,   0, 0, ParamIDs::lfo1Freq,    Xform::Raw },
            { TIMBRE1_START + ti::LFO2_FLAGS,  0, 1, ParamIDs::lfo2Wave,    Xform::Bit },
            { TIMBRE1_START + ti::LFO2_FLAGS,  4, 5, ParamIDs::lfo2KeySync, Xform::Bit },
            { TIMBRE1_START + ti::LFO2_FREQ,   0, 0, ParamIDs::lfo2Freq,    Xform::Raw },
            // Delays / FX globales, con la misma lectura que en modo synth
            { DELAY_TIME,  0, 0, ParamIDs::delayTime,  Xform::Raw },
            { DELAY_DEPTH, 0, 0, ParamIDs::delayFeedback, Xform::Raw },
            { DELAY_TYPE,  0, 0, ParamIDs::delayType,  Xform::Raw },
            { MODFX_SPEED, 0, 0, ParamIDs::modFxSpeed, Xform::Raw },
            { MODFX_DEPTH, 0, 0, ParamIDs::modFxDepth, Xform::Raw },
            { MODFX_TYPE,  0, 0, ParamIDs::modFxType,  Xform::Raw }
        };
        return fields;
    }

    // ─── Mod Sequence: 3 filas por timbre ────────────────────────────────────
    //
    // Bytes 52/53 del bloque = cabecera (on/off, resolución, último paso, tipo y key
    // sync, ya en `synthFields()`). Cada fila ocupa: byte de destino (0..30 = los 31
    // destinos reales), byte de movimiento (bit 0) y 16 bytes de paso (64±63).
    struct SeqRow
    {
        size_t destOffset;
        size_t motionOffset;
        size_t stepsOffset;
        const char* destId;
        const char* motionId;
    };

    static const SeqRow& seqRow(size_t row)
    {
        static const SeqRow rows[3] = {
            { ti::SEQ1_KNOB, ti::SEQ1_MOTION, ti::SEQ1_STEPS, ParamIDs::seq1Dest, ParamIDs::seq1Motion },
            { ti::SEQ2_KNOB, ti::SEQ2_MOTION, ti::SEQ2_STEPS, ParamIDs::seq2Dest, ParamIDs::seq2Motion },
            { ti::SEQ3_KNOB, ti::SEQ3_MOTION, ti::SEQ3_STEPS, ParamIDs::seq3Dest, ParamIDs::seq3Motion }
        };
        return rows[row % 3];
    }

    /** Id del paso `step` (0..15) de la fila `row` (0..2): `seq1Step1` … `seq3Step16`. */
    static std::string seqStepParamId(size_t row, size_t step)
    {
        return "seq" + std::to_string(row + 1) + "Step" + std::to_string(step + 1);
    }

    /** Bandas del vocoder: 16 niveles (bytes 46..61) y 16 pans (62..77) del timbre 1. */
    template <typename Fn>
    static void forEachVocoderBand(Fn&& fn)
    {
        static const char* const kLevels[16] = {
            ParamIDs::vocoderBandLevel1,  ParamIDs::vocoderBandLevel2,  ParamIDs::vocoderBandLevel3,
            ParamIDs::vocoderBandLevel4,  ParamIDs::vocoderBandLevel5,  ParamIDs::vocoderBandLevel6,
            ParamIDs::vocoderBandLevel7,  ParamIDs::vocoderBandLevel8,  ParamIDs::vocoderBandLevel9,
            ParamIDs::vocoderBandLevel10, ParamIDs::vocoderBandLevel11, ParamIDs::vocoderBandLevel12,
            ParamIDs::vocoderBandLevel13, ParamIDs::vocoderBandLevel14, ParamIDs::vocoderBandLevel15,
            ParamIDs::vocoderBandLevel16
        };
        for (int i = 0; i < 16; ++i)
            fn(kLevels[i], TIMBRE1_START + ti::VOCODER_BAND_LEVELS + static_cast<size_t>(i));
    }

    static float toEngineValue(uint8_t fieldValue, Xform xform)
    {
        switch (xform)
        {
            case Xform::Signed:  return static_cast<float>(static_cast<int>(fieldValue) - 64);
            case Xform::EqFreq:  return std::min(3.0f, std::floor(static_cast<float>(fieldValue) * 4.0f / 30.0f));
            case Xform::EqGain:  return static_cast<float>(fieldValue);
            case Xform::ArpRes:
            {
                // 0..5 real = 1/24, 1/16, 1/12, 1/8, 1/6, 1/4 → índices del motor
                static const float kTable[6] = { 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f };
                return kTable[std::min<uint8_t>(fieldValue, 5)];
            }
            case Xform::FormantShift:
            {
                // real 0..4 = 0, +1, +2, -1, -2 → motor -2..+2
                static const float kTable[5] = { 2.0f, 3.0f, 4.0f, 1.0f, 0.0f };
                return kTable[std::min<uint8_t>(fieldValue, 4)];
            }
            case Xform::ArpRange: return static_cast<float>(fieldValue) + 1.0f;
            case Xform::SeqLastStep: return static_cast<float>(fieldValue) + 1.0f;
            case Xform::SeqMotionInv:
                // El byte real guarda 0 = Smooth, 1 = Step; el motor numera al revés.
                return (fieldValue == 0) ? static_cast<float>(ModSeqMotion::Smooth)
                                         : static_cast<float>(ModSeqMotion::Step);
            case Xform::Duration127:
            case Xform::Raw:
            case Xform::Bit:
            default:
                return static_cast<float>(fieldValue);
        }
    }

    static uint8_t fromEngineValue(float value, Xform xform)
    {
        const int iv = static_cast<int>(std::lround(value));
        switch (xform)
        {
            case Xform::Signed:  return static_cast<uint8_t>(std::clamp(iv + 64, 0, 255));
            case Xform::EqFreq:
            {
                // inversa: 4 pasos del motor → centro de cada tercio de los 30 pasos reales
                static const int kTable[4] = { 3, 11, 18, 26 };
                return static_cast<uint8_t>(kTable[std::clamp(iv, 0, 3)]);
            }
            case Xform::EqGain:  return static_cast<uint8_t>(std::clamp(iv, 0, 127));
            case Xform::ArpRes:
            {
                static const int kTable[16] = { -1, -1, 0, 1, 2, 3, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1 };
                const int mapped = kTable[std::clamp(iv, 0, 15)];
                return static_cast<uint8_t>(mapped < 0 ? 3 : mapped); // fuera de la tabla: 1/16
            }
            case Xform::FormantShift:
            {
                static const int kTable[5] = { 3, 4, 0, 1, 2 };       // -2,-1,0,+1,+2 → real
                return static_cast<uint8_t>(kTable[std::clamp(iv, 0, 4)]);
            }
            case Xform::ArpRange: return static_cast<uint8_t>(std::clamp(iv - 1, 0, 3));
            case Xform::SeqLastStep: return static_cast<uint8_t>(std::clamp(iv - 1, 0, 15));
            case Xform::SeqMotionInv:
                return static_cast<uint8_t>(iv == static_cast<int>(ModSeqMotion::Smooth) ? 0 : 1);
            case Xform::Duration127:
            case Xform::Raw:
            case Xform::Bit:
            default:
                return static_cast<uint8_t>(std::clamp(iv, 0, 255));
        }
    }

#if ABD_HAS_JUCE
    /** `osc1Wave` → `t2Osc1Wave` cuando el campo pertenece al Timbre 2. */
    static juce::String timbreParamId(const char* baseId, bool second)
    {
        juce::String id(baseId);
        if (second && id.isNotEmpty())
            id = "t2" + id.substring(0, 1).toUpperCase() + id.substring(1);
        return id;
    }

    /**
     * Aplica el programa real al motor: globales, **los dos timbres** y sus dos mod
     * sequences. Devuelve el nº de parámetros escritos.
     */
    int applyToAPVTS(juce::AudioProcessorValueTreeState& apvts) const
    {
        int applied = 0;
        auto setValueById = [&](const juce::String& id, float value)
        {
            if (auto* p = apvts.getParameter(id))
            {
                p->setValueNotifyingHost(p->convertTo0to1(value));
                ++applied;
            }
        };
        auto setValue = [&](const char* id, float value, bool second)
        {
            if (id == nullptr) return;
            setValueById(timbreParamId(id, second), value);
        };

        const bool vocoder = isVocoderProgram();
        setValueById(ParamIDs::synthVocoderMode, vocoder ? 1.0f : 0.0f);

        // Modo de programa (0x10 bits 4,5) y reparto de las 4 voces (bits 6,7).
        const uint8_t voiceModeBits = extractBits(raw[GLOBAL_FLAGS], 4, 5);
        setValueById(ParamIDs::timbreMode, vocoder ? 0.0f : static_cast<float>(std::min<uint8_t>(voiceModeBits, 2)));
        setValueById(ParamIDs::timbreVoices, static_cast<float>(extractBits(raw[GLOBAL_FLAGS], 6, 7)));

        if (vocoder)
        {
            for (const auto& f : vocoderFields())
                setValue(f.paramId, toEngineValue(readField(f), f.xform), false);
            forEachVocoderBand([&](const char* id, size_t offset) { setValue(id, static_cast<float>(raw[offset]), false); });
        }
        else
        {
            // Los campos globales (FX, EQ, arpegiador, escala, split) una vez; los del
            // bloque de timbre, dos veces: Timbre 1 con base 0 y Timbre 2 con base 108.
            for (const auto& f : synthFields())
            {
                if (!isTimbreField(f))
                {
                    setValue(f.paramId, toEngineValue(readField(f), f.xform), false);
                    continue;
                }
                for (size_t t = 0; t < 2; ++t)
                    setValue(f.paramId, toEngineValue(readField(f, t * TIMBRE_SIZE), f.xform), t == 1);
            }
            for (size_t t = 0; t < 2; ++t)
                forEachSeqValue([&](const juce::String& id, float value) { setValueById(id, value); },
                                t == 1, t * TIMBRE_SIZE);
        }

        // El portamento del MS2000 no tiene interruptor: está activo cuando el tiempo > 0.
        const uint8_t porta = extractBits(raw[TIMBRE1_START + ti::PORTAMENTO], 0, 6); // bit 7 sin usar
        setValueById(ParamIDs::portamentoOn, porta > 0 ? 1.0f : 0.0f);

        return applied;
    }

    /**
     * Vuelca al programa (`raw`) todo lo que el motor modela: globales, **los dos timbres**
     * y sus mod sequences. Lo que no modela (MIDI ch por timbre, transpose, bend, vibrato,
     * resets de EG, prioridad… ) se queda como estaba en `raw`, así que un parche que vino
     * del equipo vuelve al equipo sin perderlo.
     *
     * Es el **único** camino de escritura, y `getValueById` decide de dónde sale cada valor:
     * del motor (`captureFromAPVTS`) o del bloque nativo del plugin (`fromNativeProgram`,
     * §6.4 de `DOCS/ABDSynths_SysEx_Spec.md`). Así los dos sentidos mapean exactamente igual
     * y no pueden desincronizarse.
     */
    template <typename GetValue>
    int captureModeledFields(GetValue&& getValueById, const std::string& programName)
    {
        int captured = 0;
        auto getValue = [&](const char* id, float fallback, bool second) -> float
        {
            if (id == nullptr) return fallback;
            return getValueById(timbreParamId(id, second), fallback);
        };
        auto store = [&](const Field& f, float value, size_t base)
        {
            writeField(f, fromEngineValue(value, f.xform), base);
            ++captured;
        };

        if (!programName.empty()) setName(programName);

        const bool vocoder = getValueById(ParamIDs::synthVocoderMode, 0.0f) > 0.5f;
        if (vocoder)
            setVoiceMode(VoiceMode::Vocoder);
        else
            setVoiceMode(static_cast<VoiceMode>(std::max(0.0f, std::min(2.0f,
                        getValueById(ParamIDs::timbreMode, 0.0f)))));

        if (vocoder)
        {
            for (const auto& f : vocoderFields())
            {
                if (f.paramId == nullptr) continue;
                store(f, getValue(f.paramId, 0.0f, false), 0);
            }
            forEachVocoderBand([&](const char* id, size_t offset)
            {
                raw[offset] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(getValueById(id, 0.0f))), 0, 127));
            });
        }
        else
        {
            for (const auto& f : synthFields())
            {
                if (f.paramId == nullptr) continue; // el voice mode global se trata aparte
                if (!isTimbreField(f))
                {
                    store(f, getValue(f.paramId, 0.0f, false), 0);
                    continue;
                }
                for (size_t t = 0; t < 2; ++t)
                {
                    const bool second = (t == 1);
                    store(f, getValue(f.paramId, 0.0f, second), t * TIMBRE_SIZE);
                }
            }
            for (size_t t = 0; t < 2; ++t)
                captureSeqFromAPVTS(getValueById, t == 1, t * TIMBRE_SIZE);
        }

        const uint8_t porta = static_cast<uint8_t>(std::clamp(
            static_cast<int>(std::lround(getValueById(ParamIDs::portamentoTime, 0.0f))), 0, 127));
        insertBits(raw[TIMBRE1_START + ti::PORTAMENTO], 0, 6, porta);

        return captured;
    }

    /** Vuelca el **motor** (el estado vivo del plugin) al programa real. */
    int captureFromAPVTS(const juce::AudioProcessorValueTreeState& apvts, const std::string& programName = {})
    {
        auto getValueById = [&apvts](const juce::String& id, float fallback) -> float
        {
            if (auto* p = apvts.getRawParameterValue(id)) return p->load();
            return fallback;
        };
        return captureModeledFields(getValueById, programName);
    }

    /**
     * Valor del motor tal como está guardado en el **bloque nativo** del plugin
     * (`MS2000ProgramData`, 384 B): un byte por parámetro en su `sysexOffset`, con los
     * mismos sesgos que `MS2000ProgramData::applyToAPVTS` (booleano 64/0, con signo +64).
     *
     * Los cuatro parámetros del byte de voz viven empaquetados en `VOICE_BYTE`, no en su
     * propio offset, así que se leen aparte (igual que hace el propio bloque nativo).
     */
    static float nativeEngineValue(const MS2000ProgramData& native, const juce::String& id, float fallback)
    {
        const uint8_t voiceByte = native.rawData[MS2000ProgramData::VOICE_BYTE];
        const char* raw = id.toRawUTF8();

        if (std::strcmp(raw, ParamIDs::voiceMode) == 0)
            return static_cast<float>(voiceByte & 0x03);
        if (std::strcmp(raw, ParamIDs::unisonDetune) == 0)
            return static_cast<float>((voiceByte >> 2) & 0x0F);
        if (std::strcmp(raw, ParamIDs::portamentoOn) == 0)
            return static_cast<float>((voiceByte >> 6) & 0x01);
        if (std::strcmp(raw, ParamIDs::portamentoTime) == 0)
            return static_cast<float>(native.rawData[MS2000ProgramData::VOICE_BYTE + 2]);

        const auto* meta = ParameterRegistry::getParameter(raw);
        if (meta == nullptr || meta->sysexOffset < 0) return fallback;

        const size_t off = MS2000ProgramData::TIMBRE_START + static_cast<size_t>(meta->sysexOffset);
        if (off >= MS2000ProgramData::UNPACKED_PROGRAM_SIZE) return fallback;

        const uint8_t byte = native.rawData[off];
        if (meta->type == ParamType::Boolean) return byte >= 64 ? 1.0f : 0.0f;
        if (meta->min < 0.0f) return static_cast<float>(static_cast<int>(byte) - 64);
        return static_cast<float>(byte);
    }

    /**
     * Programa **real** del equipo construido desde un preset **nativo** del plugin
     * (`MS2000ProgramData`): el paso que permite que el plugin suelto haga de equipo y
     * conteste un `0x0E` de Korg con su memoria completa (§6.4).
     *
     * ⚠️ Conversión **declarada como aproximada**: parte de la plantilla "INIT Program" del
     * equipo y escribe encima todo lo que el motor modela, así que los campos que el motor
     * no modela (`unmodelled()`: MIDI ch por timbre, Tune, Bend Range, Transpose, Vibrato,
     * resets de EG…) salen con los valores del INIT — no se inventa nada, pero tampoco se
     * puede recuperar lo que el bloque nativo nunca guardó. **Pendiente de verificar contra
     * un MS2000 físico** (ver el roadmap de ABDMS2000).
     */
    static MS2000HardwareProgram fromNativeProgram(const MS2000ProgramData& native,
                                                   const std::string& programName = {})
    {
        MS2000HardwareProgram program; // plantilla "INIT Program" del equipo
        auto getValueById = [&native](const juce::String& id, float fallback) -> float
        {
            return nativeEngineValue(native, id, fallback);
        };
        program.captureModeledFields(getValueById, programName.empty() ? native.getName() : programName);
        return program;
    }

    /**
     * Vuelca las 3 filas del mod sequence de un timbre en el motor: destino (byte
     * completo, 0..30 reales), movimiento (bit 0) y los 16 pasos (byte − 64).
     */
    template <typename SetFn>
    void forEachSeqValue(SetFn&& set, bool second, size_t base) const
    {
        for (size_t row = 0; row < 3; ++row)
        {
            const SeqRow& r = seqRow(row);
            set(timbreParamId(r.destId, second),
                static_cast<float>(raw[TIMBRE1_START + r.destOffset + base]));
            set(timbreParamId(r.motionId, second),
                toEngineValue(extractBits(raw[TIMBRE1_START + r.motionOffset + base], 0, 0), Xform::SeqMotionInv));
            for (size_t s = 0; s < ModSequencer::NUM_STEPS; ++s)
                set(timbreParamId(seqStepParamId(row, s).c_str(), second),
                    toEngineValue(raw[TIMBRE1_START + r.stepsOffset + base + s], Xform::Signed));
        }
    }

    /** Inverso de `forEachSeqValue`: el motor manda sobre los 54 bytes del secuenciador. */
    template <typename GetFn>
    void captureSeqFromAPVTS(GetFn&& get, bool second, size_t base)
    {
        for (size_t row = 0; row < 3; ++row)
        {
            const SeqRow& r = seqRow(row);
            raw[TIMBRE1_START + r.destOffset + base] = static_cast<uint8_t>(std::clamp(
                static_cast<int>(std::lround(get(timbreParamId(r.destId, second), 0.0f))), 0, 30));
            insertBits(raw[TIMBRE1_START + r.motionOffset + base], 0, 0, fromEngineValue(
                get(timbreParamId(r.motionId, second), 0.0f), Xform::SeqMotionInv));
            for (size_t s = 0; s < ModSequencer::NUM_STEPS; ++s)
                raw[TIMBRE1_START + r.stepsOffset + base + s] = static_cast<uint8_t>(std::clamp(
                    static_cast<int>(std::lround(get(timbreParamId(seqStepParamId(row, s).c_str(), second), 0.0f))) + 64,
                    0, 127));
        }
    }
#endif

    /**
     * Lo que el motor **no** modela de este programa. Se conserva byte a byte y
     * vuelve al hardware intacto; se informa para que nadie crea que se aplicó.
     */
    static std::vector<const char*> unmodelled()
    {
        return {
            "MIDI channel por timbre, Tune, Bend Range, Transpose y Vibrato",
            "EG1/EG2 Reset, Trigger Mode y Key Priority",
            "Amp SW (EG2/Gate): el VCA sigue a EG2, no a la puerta de tecla",
            "Swing del arpegiador, su Target (Both/T1/T2) y su Key Sync",
            "Delay Tempo Sync / Time Base, Mod FX Feedback y los On/Off de Mod FX y Delay",
            "Mod Seq: el modo de disparo del reloj (1Shot/Loop) y el destino OSC1 CTRL2",
            "EQ: la frecuencia real tiene 30 pasos y el motor 4 (aproximado)",
            "Escala: se conserva y vuelve al equipo, pero el motor no reafina (Equal Temp)",
            "OSC2 Mod: el 4º modo del hardware (Ring+Sync) no es el CrossMod del motor"
        };
    }
};

} // namespace ABDMS2000
