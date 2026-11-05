/**
 * @file tests/RenderCLI.cpp
 * @brief P0.1 — renderizador de un preset del motor ABDMS2000 a fichero.
 *
 * Contraparte de `Scripts/calibration/run_oracle.py`: aquél saca el mismo estímulo
 * del emulador ms2000r-emu (oráculo externo) y éste del `SynthEngine`, para poder
 * comparar cara a cara una sola variable (nivel, corte de filtro, envolvente...)
 * sin pasar por la suite de tests.
 *
 * Selección de programa: por índice del banco de fábrica (`FactoryPreload::kFactoryPreloadPrograms`,
 * los mismos 254 B que el firmware carga en la ranura 0), no por Program Change.
 * Los bytes se pueden alterar antes de aplicarlos con `--patch OFF=VAL`, que es la
 * vía de los estímulos de una sola variable de la fase P1.
 *
 * Salidas (misma base de nombre):
 *   <base>.bin : uint32 nFrames + float L[n] + float R[n]  (float sin recortar: es la
 *                verdad de medida, y el formato que ya lee `CalibrationTests`)
 *   <base>.wav : PCM entero 32 bits, 48 kHz estéreo, formato idéntico al del oráculo.
 *                Aquí sí hay recorte a ±1.0, así que si `clip` != 0 en la salida, el
 *                WAV miente en el pico: usar el .bin para niveles.
 *
 * Uso:
 *   ABDMS2000_Render --program 127 --note 60 --vel 100 --hold 1.5 --out scratch/p01/init
 *   ABDMS2000_Render --program 127 --patch 58=0x20 --out scratch/p01/cut32
 */

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Core/SynthEngine.h"
#include "MIDI/MS2000HardwareProgram.h"
#include "State/MS2000FactoryPreloadData.h"
#include "State/ParameterRegistry.gen.h"

using namespace ABDMS2000;

namespace
{

/// Host mínimo: el motor sólo necesita un APVTS al que agarrarse.
struct RenderHost : public juce::AudioProcessor
{
    RenderHost()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }
    const juce::String getName() const override { return "RenderHost"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor *createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override {}
    void setStateInformation(const void *, int) override {}
};

struct Options
{
    int program = 127;
    int note = 60;
    int vel = 100;
    double hold = 2.0;
    double tail = 1.0;
    double sr = 48000.0;
    std::string outBase;
    std::vector<std::pair<int, int>> patches;
};

void put16le(FILE *f, uint16_t v)
{
    const unsigned char b[2] = {static_cast<unsigned char>(v & 0xFF), static_cast<unsigned char>((v >> 8) & 0xFF)};
    std::fwrite(b, 1, 2, f);
}

void put32le(FILE *f, uint32_t v)
{
    const unsigned char b[4] = {static_cast<unsigned char>(v & 0xFF),
                                static_cast<unsigned char>((v >> 8) & 0xFF),
                                static_cast<unsigned char>((v >> 16) & 0xFF),
                                static_cast<unsigned char>((v >> 24) & 0xFF)};
    std::fwrite(b, 1, 4, f);
}

/// PCM entero 32 bits little-endian, estéreo entrelazado: el mismo que escribe el oráculo.
bool writeWav32(const std::string &path,
                const std::vector<float> &left,
                const std::vector<float> &right,
                int sampleRate,
                size_t &clipped)
{
    FILE *f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr)
        return false;
    const uint32_t n = static_cast<uint32_t>(left.size());
    const uint32_t dataBytes = n * 2u * 4u;
    std::fwrite("RIFF", 1, 4, f);
    put32le(f, 36u + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    put32le(f, 16u);
    put16le(f, 1u);                                            // PCM entero
    put16le(f, 2u);                                            // estéreo
    put32le(f, static_cast<uint32_t>(sampleRate));
    put32le(f, static_cast<uint32_t>(sampleRate) * 8u);        // byte rate
    put16le(f, 8u);                                            // block align
    put16le(f, 32u);                                           // bits
    std::fwrite("data", 1, 4, f);
    put32le(f, dataBytes);

    clipped = 0;
    for (size_t i = 0; i < n; ++i)
    {
        for (float s : {left[i], right[i]})
        {
            float c = s;
            if (c > 1.0f || c < -1.0f)
            {
                c = c > 1.0f ? 1.0f : -1.0f;
                ++clipped;
            }
            auto v = static_cast<int32_t>(std::llround(static_cast<double>(c) * 2147483647.0));
            put32le(f, static_cast<uint32_t>(v));
        }
    }
    std::fclose(f);
    return true;
}

/// Formato de `tests/emu_preset_*.bin`: uint32 nFrames + float L + float R.
bool writeBin(const std::string &path, const std::vector<float> &left, const std::vector<float> &right)
{
    FILE *f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr)
        return false;
    const uint32_t n = static_cast<uint32_t>(left.size());
    std::fwrite(&n, sizeof(uint32_t), 1, f);
    std::fwrite(left.data(), sizeof(float), n, f);
    std::fwrite(right.data(), sizeof(float), n, f);
    std::fclose(f);
    return true;
}

void printUsage()
{
    std::printf(
        "ABDMS2000_Render (P0.1) — render de un preset del motor a .bin float + .wav int32\n"
        "\n"
        "  --program N        preset de fábrica 0..127 (defecto 127 = INIT Program)\n"
        "  --patch OFF=VAL    modifica un byte del programa de 254 B (repetible;\n"
        "                     acepta decimal o 0xNN). Cutoff = 58, resonance = 59\n"
        "  --note N           nota MIDI (defecto 60 = C4)\n"
        "  --vel V            velocity (defecto 100)\n"
        "  --hold S           segundos con la nota pulsada (defecto 2.0)\n"
        "  --tail S           segundos tras el note-off (defecto 1.0)\n"
        "  --sr HZ            frecuencia de muestreo (defecto 48000)\n"
        "  --out BASE         base de salida, sin extensión (defecto scratch/p01/render_<N>)\n"
        "\n"
        "Los cambios de una sola variable comparables con\n"
        "Scripts/calibration/run_oracle.py usan la misma tabla de offsets\n"
        "(Source/MIDI/MS2000HardwareProgram.h: TIMBRE1_START 0x26 + ti::CAMPO).\n");
}

}  // namespace

int main(int argc, char **argv)
{
    Options opt;
    for (int i = 1; i < argc; ++i)
    {
        const char *a = argv[i];
        const auto next = [&](const char *what) -> const char * {
            if (i + 1 >= argc)
            {
                std::printf("[ERROR] %s necesita un valor\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0)
        {
            printUsage();
            return 0;
        }
        else if (std::strcmp(a, "--program") == 0)
            opt.program = std::atoi(next(a));
        else if (std::strcmp(a, "--note") == 0)
            opt.note = std::atoi(next(a));
        else if (std::strcmp(a, "--vel") == 0)
            opt.vel = std::atoi(next(a));
        else if (std::strcmp(a, "--hold") == 0)
            opt.hold = std::atof(next(a));
        else if (std::strcmp(a, "--tail") == 0)
            opt.tail = std::atof(next(a));
        else if (std::strcmp(a, "--sr") == 0)
            opt.sr = std::atof(next(a));
        else if (std::strcmp(a, "--out") == 0)
            opt.outBase = next(a);
        else if (std::strcmp(a, "--patch") == 0)
        {
            const char *spec = next(a);
            const char *eq = std::strchr(spec, '=');
            if (eq == nullptr)
            {
                std::printf("[ERROR] --patch espera OFF=VAL, recibido \"%s\"\n", spec);
                return 2;
            }
            const int off = static_cast<int>(std::strtol(std::string(spec, eq).c_str(), nullptr, 0));
            const int val = static_cast<int>(std::strtol(eq + 1, nullptr, 0));
            if (off < 0 || off >= MS2000HardwareProgram::PROGRAM_SIZE || val < 0 || val > 0xFF)
            {
                std::printf("[ERROR] --patch fuera de rango: off=%d val=%d\n", off, val);
                return 2;
            }
            opt.patches.emplace_back(off, val);
        }
        else
        {
            std::printf("[ERROR] argumento desconocido: %s\n", a);
            printUsage();
            return 2;
        }
    }

    if (opt.program < 0 || opt.program >= FactoryPreload::kNumPrograms)
    {
        std::printf("[ERROR] --program fuera de 0..%d\n", FactoryPreload::kNumPrograms - 1);
        return 2;
    }

    MS2000HardwareProgram hw;
    std::memcpy(hw.raw.data(), FactoryPreload::kFactoryPreloadPrograms[opt.program], FactoryPreload::kProgramSize);
    for (const auto &[off, val] : opt.patches)
        hw.raw[static_cast<size_t>(off)] = static_cast<uint8_t>(val);

    RenderHost host;
    juce::AudioProcessorValueTreeState apvts(host, nullptr, "Parameters", ParameterRegistry::createParameterLayout());
    SynthEngine engine(apvts);
    const int applied = hw.applyToAPVTS(apvts);
    const int blockSize = 256;
    engine.prepare(opt.sr, blockSize);
    engine.reset();

    const int nFrames = static_cast<int>(std::llround((opt.hold + opt.tail) * opt.sr));
    const int noteOffFrame = static_cast<int>(std::llround(opt.hold * opt.sr));

    std::vector<float> left(static_cast<size_t>(nFrames), 0.0f);
    std::vector<float> right(static_cast<size_t>(nFrames), 0.0f);
    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;

    int done = 0;
    bool noteOnSent = false;
    bool noteOffSent = false;
    while (done < nFrames)
    {
        const int chunk = std::min(blockSize, nFrames - done);
        if (!noteOnSent)
        {
            midi.addEvent(juce::MidiMessage::noteOn(1, opt.note, static_cast<juce::uint8>(opt.vel)), 0);
            noteOnSent = true;
        }
        if (!noteOffSent && done <= noteOffFrame && noteOffFrame < done + chunk)
        {
            midi.addEvent(juce::MidiMessage::noteOff(1, opt.note, static_cast<juce::uint8>(0)), noteOffFrame - done);
            noteOffSent = true;
        }
        block.setSize(2, chunk, false, true, true);
        block.clear();
        engine.processBlock(block, midi, nullptr);
        midi.clear();
        std::memcpy(left.data() + done, block.getReadPointer(0), sizeof(float) * static_cast<size_t>(chunk));
        std::memcpy(right.data() + done, block.getReadPointer(1), sizeof(float) * static_cast<size_t>(chunk));
        done += chunk;
    }

    double peak = 0.0;
    double sumSq = 0.0;
    size_t firstNonZero = 0;
    bool foundOnset = false;
    for (int i = 0; i < nFrames; ++i)
    {
        const double mono = 0.5 * (static_cast<double>(left[static_cast<size_t>(i)]) + right[static_cast<size_t>(i)]);
        peak = std::max(peak, std::abs(mono));
        sumSq += mono * mono;
        if (!foundOnset && std::abs(mono) > 1e-3)
        {
            firstNonZero = static_cast<size_t>(i);
            foundOnset = true;
        }
    }
    const double rms = std::sqrt(sumSq / std::max(1, nFrames));
    const auto db = [](double x) { return 20.0 * std::log10(x > 1e-12 ? x : 1e-12); };

    std::string base = opt.outBase;
    if (base.empty())
        base = "scratch/p01/render_" + std::to_string(opt.program);
    juce::File outFile(juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(base)));
    outFile.getParentDirectory().createDirectory();
    const std::string basePath = outFile.getFullPathName().toStdString();

    size_t clipped = 0;
    const bool okWav = writeWav32(basePath + ".wav", left, right, static_cast<int>(opt.sr), clipped);
    const bool okBin = writeBin(basePath + ".bin", left, right);

    std::printf("[P0.1] #%03d \"%s\"%s\n", opt.program, hw.getName().c_str(),
                opt.patches.empty() ? "" : " (parcheado)");
    for (const auto &[off, val] : opt.patches)
        std::printf("        patch [%d] = 0x%02X\n", off, val);
    std::printf("  params aplicados=%d  sr=%.0f  frames=%d (%.2fs)  noteOn=%d noteOff=%d\n", applied, opt.sr, nFrames,
                nFrames / opt.sr, opt.note, noteOffFrame);
    std::printf("  peak=%.2f dBFS  rms=%.2f dBFS  onset=%zu  clip=%zu muestras\n", db(peak), db(rms),
                foundOnset ? firstNonZero : 0, clipped);
    if (clipped > 0)
        std::printf("  [WARN] %zu muestras recortadas en el WAV: el pico real está en el .bin\n", clipped);
    std::printf("  -> %s.wav %s\n  -> %s.bin %s\n", basePath.c_str(), okWav ? "OK" : "FALLO", basePath.c_str(),
                okBin ? "OK" : "FALLO");
    return (okWav && okBin) ? 0 : 1;
}
