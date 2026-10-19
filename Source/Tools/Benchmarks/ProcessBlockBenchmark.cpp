// ============================================================================
//  ProcessBlockBenchmark.cpp — Auditoría empírica Zero-Allocation y percentiles
//  de latencia de ABDMS2000::SynthEngine::processBlock() bajo carga real.
//
//  Uso:
//    ABDMS2000_Benchmarks.exe                  -> todos los escenarios (3 repeticiones)
//    ABDMS2000_Benchmarks.exe --ci             -> corrida rápida para CTest/CI (falla si allocs > 0)
//    ABDMS2000_Benchmarks.exe --scenario <str> -> filtra por nombre de escenario
//
//  Escenarios:
//    idle                : bloque en silencio con suspensión dinámica VST3 activa
//    single_poly4        : MS2000 Single (4 voces VA/DWGS + filtro + EG + LFO)
//    layer_bitimbral     : Bitímbrico Layer (Timbre 1 + Timbre 2 simultáneos + FX)
//    vocoder_16band      : Vocoder de 16 bandas con portadora + modulador activo
//    advanced_poly16_max : Modo Advanced (16 voces + VirtualPatch + ModSeq + FX completos)
// ============================================================================

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

#include "Core/SynthEngine.h"
#include "State/ParameterRegistry.gen.h"

// ============================================================================
//  Global allocation audit — override de operator new/delete
// ============================================================================
namespace
{
std::atomic<uint64_t> gAllocCount {0};
std::atomic<uint64_t> gAllocBytes {0};
} // namespace

void *operator new(size_t size)
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    if (void *p = std::malloc(size))
        return p;
    throw std::bad_alloc();
}

void *operator new[](size_t size)
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    if (void *p = std::malloc(size))
        return p;
    throw std::bad_alloc();
}

void *operator new(size_t size, const std::nothrow_t &) noexcept
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    return std::malloc(size);
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    return std::malloc(size);
}

#if defined(_MSC_VER)
void *operator new(size_t size, std::align_val_t align)
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    if (void *p = _aligned_malloc(size, static_cast<size_t>(align)))
        return p;
    throw std::bad_alloc();
}

void *operator new[](size_t size, std::align_val_t align)
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    if (void *p = _aligned_malloc(size, static_cast<size_t>(align)))
        return p;
    throw std::bad_alloc();
}

void *operator new(size_t size, std::align_val_t align, const std::nothrow_t &) noexcept
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    return _aligned_malloc(size, static_cast<size_t>(align));
}

void *operator new[](size_t size, std::align_val_t align, const std::nothrow_t &) noexcept
{
    gAllocCount.fetch_add(1, std::memory_order_relaxed);
    gAllocBytes.fetch_add(size, std::memory_order_relaxed);
    return _aligned_malloc(size, static_cast<size_t>(align));
}

void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void *p, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete(void *p, size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, size_t) noexcept
{
    std::free(p);
}
void operator delete(void *p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
void operator delete[](void *p, size_t, std::align_val_t) noexcept
{
    _aligned_free(p);
}
#else
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, size_t) noexcept
{
    std::free(p);
}
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr double kBlockBudgetUs = (kBlockSize / kSampleRate) * 1.0e6; // ~10667 µs @48kHz

class BenchmarkDummyProcessor : public juce::AudioProcessor
{
  public:
    BenchmarkDummyProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }
    const juce::String getName() const override
    {
        return "ABDMS2000_BenchmarkHost";
    }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
    double getTailLengthSeconds() const override
    {
        return 0.0;
    }
    bool acceptsMidi() const override
    {
        return true;
    }
    bool producesMidi() const override
    {
        return true;
    }
    juce::AudioProcessorEditor *createEditor() override
    {
        return nullptr;
    }
    bool hasEditor() const override
    {
        return false;
    }
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return "Init";
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override {}
    void setStateInformation(const void *, int) override {}
};

void setRawParam(juce::AudioProcessorValueTreeState &apvts, const char *id, float value)
{
    if (auto *raw = apvts.getRawParameterValue(id))
        raw->store(value, std::memory_order_relaxed);
}

struct ScenarioSpec
{
    std::string name;
    int numNotes = 0;
    int synthMode = 0;  // 0=MS2000, 1=microKORG, 2=Advanced
    int timbreMode = 0; // 0=Single, 1=Split, 2=Layer, 3=Vocoder
    bool enableModSeq = false;
    bool enableEffects = true;
    bool injectVocoderCarrier = false;
};

struct ScenarioResult
{
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double p999 = 0.0;
    double max = 0.0;
    double mean = 0.0;
    uint64_t allocs = 0;
    uint64_t allocBytes = 0;
    int overruns = 0;
};

double percentile(const std::vector<double> &sorted, double p)
{
    if (sorted.empty())
        return 0.0;
    const size_t idx = static_cast<size_t>(std::round(p * static_cast<double>(sorted.size() - 1)));
    return sorted[idx];
}

ScenarioResult
runScenario(const ScenarioSpec &spec, const char *label, int warmupBlocks, int measuredBlocks)
{
    ScenarioResult result;
    std::vector<double> times;
    times.reserve(static_cast<size_t>(measuredBlocks));

    BenchmarkDummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts(
        proc, nullptr, "Parameters", ABDMS2000::ParameterRegistry::createParameterLayout());
    ABDMS2000::SynthEngine engine(apvts);
    engine.prepare(kSampleRate, kBlockSize);

    setRawParam(apvts, "synthMode", static_cast<float>(spec.synthMode));
    setRawParam(apvts, "timbreMode", static_cast<float>(spec.timbreMode));
    setRawParam(apvts, "synthVocoderMode", (spec.timbreMode == 3) ? 1.0f : 0.0f);
    setRawParam(apvts, "ampSustain", 1.0f);
    setRawParam(apvts, "t2AmpSustain", 1.0f);

    if (spec.enableModSeq)
    {
        setRawParam(apvts, "seqRunMode", 1.0f);
        setRawParam(apvts, "seq1Knob", 5.0f); // Filter Cutoff
        setRawParam(apvts, "seq2Knob", 6.0f); // Filter Resonance
        setRawParam(apvts, "seq3Knob", 3.0f); // Osc1 Pitch
    }

    if (spec.enableEffects)
    {
        setRawParam(apvts, "modFxType", 0.0f);
        setRawParam(apvts, "modFxDepth", 0.65f);
        setRawParam(apvts, "delayType", 1.0f);
        setRawParam(apvts, "delayDepth", 0.45f);
        setRawParam(apvts, "eqLowGain", 3.0f);
        setRawParam(apvts, "eqHighGain", 2.5f);
    }
    else
    {
        setRawParam(apvts, "modFxDepth", 0.0f);
        setRawParam(apvts, "delayDepth", 0.0f);
    }

    // Virtual patch routes active in max scenario
    setRawParam(apvts, "patch1Src", 2.0f); // LFO1
    setRawParam(apvts, "patch1Dst", 3.0f); // Cutoff
    setRawParam(apvts, "patch1Int", 0.5f);
    setRawParam(apvts, "patch2Src", 3.0f); // LFO2
    setRawParam(apvts, "patch2Dst", 1.0f); // Pitch
    setRawParam(apvts, "patch2Int", 0.35f);

    engine.updateParametersFromAPVTS();

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;

    if (spec.numNotes > 0)
    {
        for (int v = 0; v < spec.numNotes; ++v)
            midi.addEvent(juce::MidiMessage::noteOn(1, 48 + v * 2, 0.85f), 0);
    }

    // Para el escenario idle, dejamos que venza la cola de silencio dinámico (2.5s ~ 240 bloques)
    const int effectiveWarmup = (spec.numNotes == 0) ? std::max(warmupBlocks, 260) : warmupBlocks;
    for (int i = 0; i < effectiveWarmup; ++i)
    {
        buffer.clear();
        engine.processBlock(buffer, midi);
        midi.clear();
    }

    // ── Medición con auditoría estricta de operator new ──
    gAllocCount.store(0, std::memory_order_relaxed);
    gAllocBytes.store(0, std::memory_order_relaxed);

    using clock = std::chrono::steady_clock;
    for (int i = 0; i < measuredBlocks; ++i)
    {
        buffer.clear();
        if (spec.injectVocoderCarrier)
        {
            // Simula señal moduladora entrante en el bus para el vocoder
            float *ch0 = buffer.getWritePointer(0);
            float *ch1 = buffer.getWritePointer(1);
            for (int s = 0; s < kBlockSize; ++s)
            {
                const float modSig = 0.25f * ((s & 31) < 16 ? 1.0f : -1.0f);
                ch0[s] = modSig;
                ch1[s] = modSig;
            }
        }

        const auto t0 = clock::now();
        engine.processBlock(buffer, midi);
        const auto t1 = clock::now();

        const double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        times.push_back(us);
        if (us > kBlockBudgetUs)
            ++result.overruns;
    }

    result.allocs = gAllocCount.load(std::memory_order_relaxed);
    result.allocBytes = gAllocBytes.load(std::memory_order_relaxed);

    std::sort(times.begin(), times.end());
    result.p50 = percentile(times, 0.50);
    result.p95 = percentile(times, 0.95);
    result.p99 = percentile(times, 0.99);
    result.p999 = percentile(times, 0.999);
    result.max = times.back();

    double sum = 0.0;
    for (double t : times)
        sum += t;
    result.mean = sum / static_cast<double>(times.size());

    std::printf("RESULT %-20s p50_us=%.1f p95_us=%.1f p99_us=%.1f p999_us=%.1f "
                "max_us=%.1f mean_us=%.2f allocs=%llu allocBytes=%llu overruns=%d "
                "(budget_us=%.0f)\n",
                label,
                result.p50,
                result.p95,
                result.p99,
                result.p999,
                result.max,
                result.mean,
                static_cast<unsigned long long>(result.allocs),
                static_cast<unsigned long long>(result.allocBytes),
                result.overruns,
                kBlockBudgetUs);
    std::fflush(stdout);
    return result;
}
} // namespace

int main(int argc, char *argv[])
{
    juce::String filter;
    bool ciMode = false;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg(argv[i]);
        if (arg == "--ci")
            ciMode = true;
        else if (arg == "--scenario" && i + 1 < argc)
            filter = juce::String(argv[++i]);
    }

    const int warmupBlocks = ciMode ? 60 : 150;
    const int measuredBlocks = ciMode ? 500 : 1500;
    const int repeats = ciMode ? 1 : 3;

    std::printf("ABDMS2000 processBlock benchmark & zero-allocation audit\n");
    std::printf("config: sr=%.0fHz block=%d warmup=%d measured=%d repeats=%d budget_us=%.0f\n",
                kSampleRate,
                kBlockSize,
                warmupBlocks,
                measuredBlocks,
                repeats,
                kBlockBudgetUs);

    std::vector<ScenarioSpec> specs = {
        {"idle", 0, 0, 0, false, false, false},
        {"single_poly4", 4, 0, 0, false, true, false},
        {"layer_bitimbral", 4, 0, 2, true, true, false},
        {"vocoder_16band", 4, 0, 3, false, true, true},
        {"advanced_poly16_max", 16, 2, 2, true, true, false},
    };

    uint64_t totalAllocsAcrossAllScenarios = 0;

    for (const auto &spec : specs)
    {
        if (filter.isNotEmpty() && !juce::String(spec.name).contains(filter))
            continue;

        ScenarioResult best;
        bool haveBest = false;
        for (int r = 0; r < repeats; ++r)
        {
            char label[48];
            std::snprintf(label, sizeof(label), "%s#%d", spec.name.c_str(), r + 1);
            const auto res = runScenario(spec, label, warmupBlocks, measuredBlocks);
            totalAllocsAcrossAllScenarios += res.allocs;
            if (!haveBest || res.p95 < best.p95)
            {
                best = res;
                haveBest = true;
            }
        }

        if (repeats > 1 && haveBest)
        {
            std::printf("BEST   %-20s p50_us=%.1f p95_us=%.1f p99_us=%.1f p999_us=%.1f "
                        "max_us=%.1f mean_us=%.2f allocs=%llu allocBytes=%llu overruns=%d\n",
                        spec.name.c_str(),
                        best.p50,
                        best.p95,
                        best.p99,
                        best.p999,
                        best.max,
                        best.mean,
                        static_cast<unsigned long long>(best.allocs),
                        static_cast<unsigned long long>(best.allocBytes),
                        best.overruns);
        }
    }

    if (totalAllocsAcrossAllScenarios > 0)
    {
        std::printf("BENCH_FAIL: detected %llu heap allocations inside processBlock!\n",
                    static_cast<unsigned long long>(totalAllocsAcrossAllScenarios));
        return 1;
    }

    std::printf("BENCH_DONE (0 allocations verified across all scenarios)\n");
    return 0;
}
