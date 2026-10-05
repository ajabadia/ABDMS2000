#pragma once

/*
 * Compatibility shim — canonical implementation lives in ABDSharedCode::SynthCore
 * (namespace abd::synth, sub-namespace DSPUtils). Thin re-export keeping
 * historical include paths while consuming the shared module (Phase 2 DRY).
 * NOTE: sync artifact, not hand-maintained — edit ABDSharedCode/SynthCore instead.
 */

#include "SynthCore/DSPUtils.h"

namespace ABDMS2000
{
    // Historical code calls DSPUtils::clamp(...) / DSPUtils::TWO_PI inside
    // namespace ABDMS2000 — re-export as a nested namespace, not a flat using.
    namespace DSPUtils
    {
        using namespace abd::synth::DSPUtils;

        // === Local constants for sample rate validation & DSP tuning ===
        // Used by VAOscillator, MultiModeFilter, ModFX, DelayFX, Equalizer, Vocoder
        static constexpr double kMinSampleRate = 1000.0;
        static constexpr double kDefaultSampleRate = 44100.0;

        // Centralized sample rate validation
        inline double validateSampleRate(double sr) noexcept
        {
            return (sr > kMinSampleRate) ? sr : kDefaultSampleRate;
        }

        // Resonance curve constants (MS2000 hardware modeling)
        static constexpr float kResonanceFeedbackScale = 0.245f;  // Feedback scaling for resonance curve
        static constexpr float kResonanceMinFeedback = 0.01f;      // Minimum feedback to prevent denormal

        // ModFX constants
        static constexpr float kSpeedLogScaleMax = 750.0f;         // Log scale max for speed (0.02Hz to 15Hz)
        static constexpr float kChorusBaseDelaySec = 0.0075f;      // 7.5ms base chorus delay
        static constexpr float kFlangerDelayRangeSec = 0.0057f;    // 5.7ms delay range for flanger
        static constexpr float kChorusExcursionSec = 0.0035f;      // 3.5ms chorus excursion
        static constexpr float kFlangerExcursionRangeSec = 0.0020f;// 2ms flanger excursion range
    }
}
