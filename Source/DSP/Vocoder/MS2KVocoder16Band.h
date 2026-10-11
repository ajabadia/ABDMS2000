#pragma once
#include <array>
#include "../Oscillators/NoiseGenerator.h"
#include "EnvelopeFollower.h"

namespace ABDMS2000
{

struct BiquadBPF
{
    float b0 {0.0f}, b1 {0.0f}, b2 {0.0f}, a1 {0.0f}, a2 {0.0f};
    float s1 {0.0f}, s2 {0.0f};

    void setBandpass(float centerFreq, float q, double sampleRate) noexcept;
    void reset() noexcept
    {
        s1 = s2 = 0.0f;
    }
    float process(float in) noexcept;
};

/**
 * @brief MS2KVocoder16Band - Motor de vocoder de 16 bandas del Korg MS2000.
 *
 * Características específicas del hardware:
 * - 16 canales de análisis (modulador) y 16 canales de síntesis (portadora).
 * - Desplazamiento de formantes discreto (-2 .. +2 bandas).
 * - Detector de sibilantes a 8 kHz (HPF Butterworth + seguidor ultrarrápido).
 * - Inyección de ruido blanco modulado por consonantes para garantizar inteligibilidad vocal.
 * - Paneo estéreo y control de ganancia individual por banda.
 */
class MS2KVocoder16Band
{
  public:
    static constexpr size_t NUM_BANDS = 16;

    MS2KVocoder16Band() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept
    {
        enabled_ = enabled;
    }
    bool isEnabled() const noexcept
    {
        return enabled_;
    }

    void setFormantShift(int shiftValue) noexcept;    // -2, -1, 0, +1, +2
    void setGateSense(float gateSense0to1) noexcept;  // 0..1 -> 0.005s..0.200s
    void setHPFLevel(float level0to1) noexcept;
    void setHPFGate(bool enabled) noexcept
    {
        hpfGate_ = enabled;
    }
    void setHPFThreshold(float thresh0to1) noexcept;
    void setDirectLevel(float level0to1) noexcept;

    void setBandLevel(size_t bandIndex, float level0to1) noexcept;
    void setBandPan(size_t bandIndex, float pan0to1) noexcept;  // 0.0 (L) to 1.0 (R)

    void process(float modSample, float carrierSample, float &outLeft, float &outRight) noexcept;

  private:
    double sampleRate_ {44100.0};
    bool enabled_ {false};
    int formantShift_ {0};
    float hpfLevel_ {0.0f};
    bool hpfGate_ {true};
    float hpfThreshold_ {0.1f};
    float directLevel_ {0.0f};

    std::array<BiquadBPF, NUM_BANDS> analysisFilters_ {};
    std::array<BiquadBPF, NUM_BANDS> synthesisFilters_ {};
    std::array<EnvelopeFollower, NUM_BANDS> followers_ {};
    std::array<float, NUM_BANDS> bandLevels_ {};
    std::array<float, NUM_BANDS> bandPans_ {};

    // Filtro HPF 8kHz para sibilantes y seguidor de consonantes
    float hpfB0_ {1.0f}, hpfB1_ {-2.0f}, hpfB2_ {1.0f}, hpfA1_ {0.0f}, hpfA2_ {0.0f};
    float hpfS1_ {0.0f}, hpfS2_ {0.0f};
    EnvelopeFollower sibilanceFollower_;

    NoiseGenerator whiteNoiseGen_;

    void updateFilterFrequencies() noexcept;
    void updateHPFFilter() noexcept;
};

}  // namespace ABDMS2000
