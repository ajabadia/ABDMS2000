#include "MultiModeFilter.h"
#include "../../Core/MS2000FirmwareTables.h"
#include <algorithm>
#include <cmath>

namespace ABDMS2000
{

void MultiModeFilter::prepare(double sampleRate) noexcept
{
    sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
    reset();
    updateCoefficients();
}

void MultiModeFilter::reset() noexcept
{
    s1_1_ = s1_2_ = 0.0f;
    s2_1_ = s2_2_ = 0.0f;
}

void MultiModeFilter::setType(FilterType type) noexcept
{
    if (type_ == type)
        return;
    type_ = type;
    updateCoefficients();
}

void MultiModeFilter::setCutoff(float cutoffHz) noexcept
{
    const float clamped = DSPUtils::clamp(cutoffHz, 5.0f, static_cast<float>(sampleRate_ * 0.49));
    if (std::abs(clamped - cutoffHz_) <= 1e-3f)
        return;
    cutoffHz_ = clamped;
    updateCoefficients();
}

void MultiModeFilter::setResonance(float resonance0to1) noexcept
{
    const float clamped = DSPUtils::clamp(resonance0to1, 0.0f, 1.0f);
    if (std::abs(clamped - resonance_) <= 1e-5f)
        return;
    resonance_ = clamped;
    updateCoefficients();
}

void MultiModeFilter::updateCoefficients() noexcept
{
    const bool is24LPF = (type_ == FilterType::LPF24);
    const auto resRom = FirmwareTables::getHardwareFilterResonanceCoeffs(resonance_, is24LPF);

    // Pre-warped analog frequency mapping (Bilinear transform)
    float wd = cutoffHz_ * DSPUtils::TWO_PI;
    float T = 1.0f / static_cast<float>(sampleRate_);
    float wa = (2.0f / T) * std::tan(wd * T * 0.5f);
    g_ = wa * T * 0.5f;

    // Damping / Feedback exacto desde w2 de la ROM (0x26554 / 0x26D54):
    // 2*R = 1 - w2  =>  R = (1 - w2) / 2,  Q_etapa = 1 / (1 - w2).
    // Para mantener auto-oscilación sostenida tras impulso en el integrador TPT trapezoidal
    // cuando el knob supera 105 (~0.827, donde w2 > 0.962), llevamos R suavemente bajo cero.
    if (resonance_ > 0.82f)
    {
        const float selfOscT = (resonance_ - 0.82f) / 0.18f;
        R_ = -0.008f * selfOscT;
    }
    else
    {
        R_ = std::max(resRom.dampingR, 1e-5f);
    }
    w0_ = resRom.w0;
    w1_ = resRom.w1;

    // Ganancia post-SVF por modo desde los descriptores ROM (0x25DF4 / 0x25ECC / 0x25FA4 / 0x2607C):
    // En P:$077B (X:$04E8 = X:$04E6 * w0) y P:$07B4..$07B5 (`mpy y0,x0,a` / `asl a` -> `2 * X:$04E6 * w0`),
    // seguida de la ganancia post-VCA `x8` (`asl #$03,a,a` en P:$087F con X:$0524..$0528):
    //   - 24LPF: 2 * 0.250000 * 8 * (b0+b1)/(1-a1) = 4.000
    //   - 12LPF: 2 * 0.500000 * 4                  = 4.000
    //   - 12BPF: 2 * 0.750000 * 1.0 (X:$0528=$100000*8=1) = 1.500
    //   - 12HPF: 2 * 0.560059 * 1.0 (X:$0528=$100000*8=1) = 1.120117
    // Esto elimina el exceso de +15..+21.6 dB en resonancia que introducía la normalización artificial
    // 1/(w1[0]^2 * w0[0]) = 12.011 sin comprometer el suelo de auto-oscilación tras impulso (Cal 8.3).
    const float modePostGain = FirmwareTables::getHardwareFilterModePostGain(static_cast<int>(type_));
    switch (type_)
    {
    case FilterType::LPF24:
        passbandNorm_ = 8.0f * modePostGain;  // 8 * 0.50 = 4.0f (DC gain = 0.333 = -9.54 dB @ reso=0)
        break;
    case FilterType::LPF12:
        passbandNorm_ = 4.0f * modePostGain;  // 4 * 1.00 = 4.0f (DC gain = 0.514 = -5.78 dB @ reso=0)
        break;
    case FilterType::BPF12:
    case FilterType::HPF12:
    default:
        passbandNorm_ = 2.0f * modePostGain;  // 3.0f (BPF12) / 2.2402f (HPF12)
        break;
    }

    h_ = 1.0f / (1.0f + 2.0f * R_ * g_ + g_ * g_);
}

float MultiModeFilter::process(float inputSample) noexcept
{
    // Headroom-preserving input scaling: the VCF only expects mixer sums already within the
    // [-1.0f, 1.0f] band, and the firmwares keep the SVF integrator blocks open wider than
    // a naive unit-norm model. Clipping or folding the inputs here (e.g. *0.25f/4.0f) adds
    // a nonlinear pre-stage that does not reproduce the hardware headroom on resonant patches,
    // so this stage must stay linear for the resonance/level comparison to be meaningful.
    float in = inputSample;

    // Planned future ladder compensation for vocal stages that exceed +/-1.0f after the ROM
    // w1-scale (X:$04DD / X:$04E0) is applied at the Voice level, not in this filter;
    // the Voice already owns the only long-term scaling knob in the voice pipeline:
    // effectiveAmpLevel / voiceDistortion_. Compensate that excess there, never by folding
    // the filter input, so that VCF comparisons stay tied to the reference criterion.
    float u1 = in * w1_;

    // --- First 2-pole SVF stage (with DSP56362 internal resonator state saturation) ---
    float hp1 = (u1 - (2.0f * R_ + g_) * s1_1_ - s1_2_) * h_;
    float bp1 = g_ * hp1 + s1_1_;
    const float nextS1_1 = g_ * hp1 + bp1;
    // Preserve exact linear trapezoidal integration for HPF12/BPF12 so low-frequency zeros do not leak
    s1_1_ = (type_ == FilterType::HPF12 || type_ == FilterType::BPF12) ? nextS1_1
                                                                       : (DSPUtils::softClip(nextS1_1 * 0.5f) * 2.0f);
    float lp1 = g_ * bp1 + s1_2_;
    s1_2_ = g_ * bp1 + lp1;  // Integrator 2 state update

    switch (type_)
    {
    case FilterType::LPF12:
        return lp1 * w0_ * passbandNorm_;

    case FilterType::BPF12:
        // ROM 0x25FA4: X:$04B2 = $800001 (< 0) puentea el saturador post-SVF (P:$083F..$0842)
        // y el biquad de salida es unidad plana (X:$0528 = $100000 -> 0.125 * 8 = 1.0).
        return bp1 * w0_ * passbandNorm_;

    case FilterType::HPF12:
        // ROM 0x2607C: X:$04B2 = $800001 (< 0) puentea el saturador post-SVF (P:$083F..$0842)
        // y el biquad de salida es unidad plana (X:$0528 = $100000 -> 0.125 * 8 = 1.0).
        return hp1 * w0_ * passbandNorm_;

    case FilterType::LPF24:
    default:
    {
        // Cascade into Second 2-pole SVF stage for -24dB/oct LPF (scaled by w1, X:$04E0)
        float u2 = lp1 * w1_;
        float hp2 = (u2 - (2.0f * R_ + g_) * s2_1_ - s2_2_) * h_;
        float bp2 = g_ * hp2 + s2_1_;
        s2_1_ = DSPUtils::softClip((g_ * hp2 + bp2) * 0.5f) * 2.0f;
        float lp2 = g_ * bp2 + s2_2_;
        s2_2_ = g_ * bp2 + lp2;

        return lp2 * w0_ * passbandNorm_;
    }
    }
}

}  // namespace ABDMS2000
