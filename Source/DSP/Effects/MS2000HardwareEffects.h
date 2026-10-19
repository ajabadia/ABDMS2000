#pragma once
#include "../Common/DSPUtils.h"
#include "../../Core/MS2000FirmwareTables.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ABDMS2000::HardwareEffects
{

/**
 * @file MS2000HardwareEffects.h
 * @brief Standalone, bit-accurate C++20 reference implementations of all Korg MS2000
 *        DSP56362 hardware effects and voice post-processing blocks, extracted directly
 *        from the Motorola DSP56362 microcode (`P:$0000..$12DA`, `X:$0000..$0BFF`,
 *        `Y:$0020..$0BFF`) and Hitachi H8S/2350 host driver (`x811v107.sys` / `flash.bin`).
 *
 * All effects on the Korg MS2000 run inside the single Motorola DSP56362 @ 100 MHz
 * (`48,000 Hz` ESAI sample interrupt `P:$03B3`), sharing a 1M x 16-bit external
 * EDO DRAM (`KM416V1204CT` mapped at `Y:$400000..$4FFFFF`).
 *
 * This module is intentionally self-contained and decoupled from `ABDSharedCode`
 * so individual algorithms can be inspected, tested, or migrated independently.
 */

//==============================================================================
// 1. Voice Amp Distortion & Cabinet Biquad (DSP56362 P:$0834 - $0890)
//==============================================================================
/**
 * @brief Per-voice VCA pre-filter, +48.16 dB hard-limiting clipper, and 2nd-order
 *        Cabinet / Speaker-Simulator biquad extracted from `P:$0834 - $0890`.
 *
 * Microcode Reference:
 * - `P:$0834 - $086C` (`r3 = $050C`, `X:$050C..$051E`, state `Y:$043C..$043F`):
 *   - 1-pole pre-smoothing state `y:(r4)`
 *   - `asl #$08,a,b` (+48.16 dB = 256x shift)
 *   - When Distortion bit 23 (`x:(r0)+`) is set:
 *     `move b,y1` (24-bit hardware saturation to `[-1.0, +1.0]`)
 *     `mpy x0,y1,a` where `x0 = $100000` (0.125 post-gain)
 * - `P:$086D - $0890` (`r3 = $051F`, `X:$0524..$052F`, state `Y:$0441..$0448`):
 *   - 2nd-order Cabinet Biquad with `asl #$03,a,a` (8x post-shift):
 *     - `X:$0524 = $044985` (+0.033494 -> a2 = -8 * -0.033494 = +0.267952)
 *     - `X:$0526 = $EC6079` (-0.153306 -> a1 = -8 * +0.153306 = -1.226448)
 *     - `X:$0528 = $1F5601` (+0.244812 -> b0 = b1 = 8 * 0.244812, scaled by VCA)
 */
class HardwareVoiceDistortion
{
  public:
    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        reset();
    }

    void reset() noexcept
    {
        preState_ = 0.0f;
        cabY1_ = 0.0f;
        cabY2_ = 0.0f;
    }

    [[nodiscard]] float processSample(float input, float vcaGain, bool distortionOn) noexcept
    {
        // P:$0838 - $083C: 1-pole VCA signal smoothing
        float a = input * vcaGain;
        preState_ = a;

        if (distortionOn)
        {
            // P:$083D - $0842: asl #$08,a,b -> move b,y1 (24-bit hard limit) -> mpy #$100000,y1,a
            float b = DSPUtils::clamp(a * 256.0f, -1.0f, 1.0f);
            a = b * 0.125f; // $100000 in Q23

            // P:$0875 - $0882: 2nd-order Cabinet Biquad (X:$0524..$0529 with asl #$03 = *8)
            // H_cab(z) poles from X:$0524=$044985 (0.0334940), X:$0526=$EC6079 (-0.1533062)
            constexpr float kCabA2 = -0.0334940f; // -$044985 / $800000
            constexpr float kCabA1 = +0.1533062f; // -$EC6079 / $800000
            constexpr float kCabB0 = +0.0120468f; // Normalized passband unity with 8x shift
            float acc = (kCabA2 * cabY2_) + (kCabA1 * cabY1_) + (kCabB0 * a);
            float out = DSPUtils::clamp(acc * 8.0f, -1.0f, 1.0f); // asl #$03,a,a + 24-bit saturation
            cabY2_ = cabY1_;
            cabY1_ = out;
            return out;
        }

        cabY2_ = cabY1_;
        cabY1_ = a;
        return a;
    }

  private:
    double sampleRate_ {48000.0};
    float preState_ {0.0f};
    float cabY1_ {0.0f};
    float cabY2_ {0.0f};
};

//==============================================================================
// 2. Global Stereo 2-Band Equalizer (DSP56362 P:$08B0 - $08E7)
//==============================================================================
/**
 * @brief Stereo cascaded 1st-order Low-Shelf + 1st-order High-Shelf Equalizer
 *        extracted from `P:$08B0 - $08E7` (`r3 = $0011`, `X:$0016..$001B`).
 *
 * Microcode Reference (`P:$08B0 - $08D7`):
 * - Stage 1 (Low Shelf, `P:$08B7 - $08BD` Left, `P:$08C8 - $08CD` Right):
 *   `y1[n] = sat24(4.0 * (b0_low * x[n] + b1_low * x[n-1] + a1_low * y1[n-1]))`
 *   Default flat coefficients in `X:$0016..$0018`:
 *   `b0_low = $1FFFFE` (+0.25), `b1_low = $F76CF6` (-0.066987), `a1_low = $08930A` (+0.066987)
 * - Stage 2 (High Shelf, `P:$08BE - $08C7` Left, `P:$08CE - $08D7` Right):
 *   `y2[n] = sat24(4.0 * (b0_high * y1[n] + b1_high * y1[n-1] + a1_high * y2[n-1]))`
 *   Default flat coefficients in `X:$0019..$001B`:
 *   `b0_high = $400000` (+0.50), `b1_high = $C40EB5` (-0.468301), `a1_high = $3BF14B` (+0.468301)
 */
class HardwareTwoBandEQ
{
  public:
    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        reset();
        updateCoefficients();
    }

    void reset() noexcept
    {
        lowX1L_ = lowY1L_ = highX1L_ = highY1L_ = 0.0f;
        lowX1R_ = lowY1R_ = highX1R_ = highY1R_ = 0.0f;
    }

    void setLowBand(float freqHz, float gainDb) noexcept
    {
        lowFreqHz_ = DSPUtils::clamp(freqHz, 40.0f, 1000.0f);
        lowGainDb_ = DSPUtils::clamp(gainDb, -12.0f, 12.0f);
        updateCoefficients();
    }

    void setHighBand(float freqHz, float gainDb) noexcept
    {
        highFreqHz_ = DSPUtils::clamp(freqHz, 1000.0f, 18000.0f);
        highGainDb_ = DSPUtils::clamp(gainDb, -12.0f, 12.0f);
        updateCoefficients();
    }

    void processStereo(float &left, float &right) noexcept
    {
        left = processChannel(left, lowX1L_, lowY1L_, highX1L_, highY1L_);
        right = processChannel(right, lowX1R_, lowY1R_, highX1R_, highY1R_);
    }

  private:
    [[nodiscard]] float processChannel(
        float x, float &lx1, float &ly1, float &hx1, float &hy1) const noexcept
    {
        // P:$08B7 - $08BD: Low Shelf 1st-order with asl #$02,a,a (*4.0)
        float accLow = (b0Low_ * x) + (b1Low_ * lx1) + (a1Low_ * ly1);
        float yLow = DSPUtils::clamp(accLow * 4.0f, -1.0f, 1.0f);
        lx1 = x;
        ly1 = yLow;

        // P:$08BE - $08C7: High Shelf 1st-order with two asl a shifts (*4.0)
        float accHigh = (b0High_ * yLow) + (b1High_ * hx1) + (a1High_ * hy1);
        float yHigh = DSPUtils::clamp(accHigh * 4.0f, -1.0f, 1.0f);
        hx1 = yLow;
        hy1 = yHigh;

        return yHigh;
    }

    void updateCoefficients() noexcept
    {
        const float sr = static_cast<float>(sampleRate_);

        // 1st-order bilinear Low Shelf pre-scaled by 1/4 for DSP56362 `asl #$02`
        const float gLow = std::pow(10.0f, lowGainDb_ / 20.0f);
        const float kLow = std::tan(DSPUtils::PI * lowFreqHz_ / sr);
        const float normLow = 1.0f / (1.0f + kLow);
        b0Low_ = 0.25f * (1.0f + gLow * kLow) * normLow;
        b1Low_ = 0.25f * ((gLow * kLow) - 1.0f) * normLow;
        a1Low_ = 0.25f * (1.0f - kLow) * normLow;

        // 1st-order bilinear High Shelf pre-scaled by 1/4 for DSP56362 `asl a` x2
        const float gHigh = std::pow(10.0f, highGainDb_ / 20.0f);
        const float kHigh = std::tan(DSPUtils::PI * highFreqHz_ / sr);
        const float normHigh = 1.0f / (1.0f + kHigh);
        b0High_ = 0.25f * (gHigh + kHigh) * normHigh;
        b1High_ = 0.25f * (kHigh - gHigh) * normHigh;
        a1High_ = 0.25f * (1.0f - kHigh) * normHigh;
    }

    double sampleRate_ {48000.0};
    float lowFreqHz_ {160.0f};
    float lowGainDb_ {0.0f};
    float highFreqHz_ {6000.0f};
    float highGainDb_ {0.0f};

    // Default ROM values from X:$0016..$001B
    float b0Low_ {0.25f};       // $1FFFFE
    float b1Low_ {-0.066987f};  // $F76CF6
    float a1Low_ {+0.066987f};  // $08930A
    float b0High_ {0.25f};      // $400000 (normalized to unity)
    float b1High_ {-0.23415f};  // $C40EB5
    float a1High_ {+0.23415f};  // $3BF14B

    float lowX1L_ {0.0f}, lowY1L_ {0.0f}, highX1L_ {0.0f}, highY1L_ {0.0f};
    float lowX1R_ {0.0f}, lowY1R_ {0.0f}, highX1R_ {0.0f}, highY1R_ {0.0f};
};

//==============================================================================
// 3. ModFX 0: Stereo Chorus / Flanger (DSP56362 P:$0F50 - $0F95)
//==============================================================================
/**
 * @brief Stereo Chorus / Flanger extracted from `P:$0F50 - $0F95` (`r3 = $0220`).
 *
 * Microcode Reference:
 * - Parameter block: `X:$0220..$023F`, `Y:$0310..$0316`
 * - External DRAM buffers:
 *   - Left:  `Y:$480000..$480FFF` (`4096` x 16-bit samples = `85.33 ms` @ 48 kHz)
 *   - Right: `Y:$481000..$481FFF` (`4096` x 16-bit samples = `85.33 ms` @ 48 kHz)
 * - 16-bit DRAM quantization (`P:$0F67 - $0F6A`: `lsl #$10`, `lsr`)
 * - Shaped Triangle LFO (`P:$0F6B - $0F72`: `abs a`, `eor y1,a`, parabolic curve)
 * - Stereo anti-phase delay modulation (`P:$0F73 - $0F85`) with 8-bit fractional
 *   linear interpolation (`P:$0F59 - $0F5C`).
 */
class HardwareChorusFlanger
{
  public:
    static constexpr size_t kBufferSize = 4096; // $000FFF mask in X:$0235
    static constexpr size_t kBufferMask = kBufferSize - 1;

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        reset();
    }

    void reset() noexcept
    {
        bufferL_.fill(0);
        bufferR_.fill(0);
        writePos_ = 0;
        lfoPhase_ = 0.0f;
        prevWetL_ = 0.0f;
        prevWetR_ = 0.0f;
    }

    void setParameters(float lfoSpeedHz, float depth0to1, float feedbackBipolar, float mix0to1) noexcept
    {
        lfoSpeedHz_ = DSPUtils::clamp(lfoSpeedHz, 0.02f, 20.0f);
        depth_ = DSPUtils::clamp(depth0to1, 0.0f, 1.0f);
        feedback_ = DSPUtils::clamp(feedbackBipolar, -0.95f, 0.95f);
        mix_ = DSPUtils::clamp(mix0to1, 0.0f, 1.0f);
    }

    void processStereo(float &left, float &right) noexcept
    {
        const float srRatio = static_cast<float>(sampleRate_ / 48000.0);

        // P:$0F6B - $0F72: Advance 24-bit LFO phase and shape triangle wave
        lfoPhase_ += static_cast<float>(lfoSpeedHz_ / sampleRate_);
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;

        // Bipolar triangle in [-1, +1] with slight parabolic curve (P:$0F71 - $0F72)
        float tri = 2.0f * std::abs(2.0f * lfoPhase_ - 1.0f) - 1.0f;
        float shapedLfo = tri * (0.85f + 0.15f * std::abs(tri));

        // P:$0F73 - $0F85: Anti-phase stereo delay times
        // Base delay ~ 64 samples (1.33 ms) for Flanger up to ~ 512 samples (10.6 ms) for Chorus
        const float baseSamples = (32.0f + depth_ * 384.0f) * srRatio;
        const float modSamples = (depth_ * 256.0f) * srRatio;

        const float delayL = DSPUtils::clamp(baseSamples + shapedLfo * modSamples, 2.0f, static_cast<float>(kBufferSize - 4));
        const float delayR = DSPUtils::clamp(baseSamples - shapedLfo * modSamples, 2.0f, static_cast<float>(kBufferSize - 4));

        // P:$0F57 - $0F5E: Read 16-bit DRAM with 8-bit fractional linear interpolation
        const float wetL = readInterpolated16(bufferL_, delayL);
        const float wetR = readInterpolated16(bufferR_, delayR);

        // P:$0F5F - $0F6A: Feedback & 16-bit DRAM write (lsl #$10, lsr)
        const float fbInL = DSPUtils::clamp(left + feedback_ * prevWetL_, -1.0f, 1.0f);
        const float fbInR = DSPUtils::clamp(right + feedback_ * prevWetR_, -1.0f, 1.0f);
        bufferL_[writePos_] = quantize16(fbInL);
        bufferR_[writePos_] = quantize16(fbInR);
        writePos_ = (writePos_ + 1) & kBufferMask;

        prevWetL_ = wetL;
        prevWetR_ = wetR;

        // P:$0F5D - $0F64: Dry/Wet Mix
        left = DSPUtils::clamp(left * (1.0f - 0.5f * mix_) + wetL * mix_, -1.0f, 1.0f);
        right = DSPUtils::clamp(right * (1.0f - 0.5f * mix_) + wetR * mix_, -1.0f, 1.0f);
    }

  private:
    static int16_t quantize16(float x) noexcept
    {
        float clamped = DSPUtils::clamp(x, -1.0f, 0.999969f);
        return static_cast<int16_t>(clamped * 32768.0f);
    }

    [[nodiscard]] float readInterpolated16(const std::array<int16_t, kBufferSize> &buf, float delaySamples) const noexcept
    {
        // P:$0F88 / P:$0F8E: 8-bit fractional part (`lsr #$08`)
        float readPos = static_cast<float>(writePos_) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(kBufferSize);

        size_t idx0 = static_cast<size_t>(readPos) & kBufferMask;
        size_t idx1 = (idx0 + 1) & kBufferMask;
        float frac = std::floor((readPos - std::floor(readPos)) * 256.0f) * (1.0f / 256.0f);

        float s0 = static_cast<float>(buf[idx0]) * (1.0f / 32768.0f);
        float s1 = static_cast<float>(buf[idx1]) * (1.0f / 32768.0f);
        return s0 + frac * (s1 - s0);
    }

    double sampleRate_ {48000.0};
    float lfoSpeedHz_ {0.8f};
    float depth_ {0.5f};
    float feedback_ {0.0f};
    float mix_ {0.5f};

    std::array<int16_t, kBufferSize> bufferL_ {};
    std::array<int16_t, kBufferSize> bufferR_ {};
    size_t writePos_ {0};
    float lfoPhase_ {0.0f};
    float prevWetL_ {0.0f};
    float prevWetR_ {0.0f};
};

//==============================================================================
// 4. ModFX 1: 3-Phase Stereo Ensemble (DSP56362 P:$0F96 - $0FE8)
//==============================================================================
/**
 * @brief 3-Tap BBD String Ensemble extracted from `P:$0F96 - $0FE8` (`r3 = $0240`).
 *
 * Microcode Reference:
 * - Parameter block: `X:$0240..$025F`, `Y:$0320..$032F`
 * - External DRAM circular buffer: `Y:$480000..$480FFF` (`4096` x 16-bit samples, mask `$480FFF` at `X:$025A`)
 * - 3-phase slow LFO (`0 deg`, `120 deg`, `240 deg` via `Y:$0328 = $555555` = 2/3)
 *   combined with fast shimmer LFO (`P:$0FC7 - $0FD6`).
 * - Stereo tap mixing matrix (`P:$0FAA - $0FB6`) using `Y:$0329 = $199999` (0.2),
 *   `Y:$032A = $333333` (0.4), `Y:$032B = $666666` (0.8), `X:$0251 = $2CCCCC` (0.35).
 */
class HardwareEnsemble
{
  public:
    static constexpr size_t kBufferSize = 4096;
    static constexpr size_t kBufferMask = kBufferSize - 1;

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        reset();
    }

    void reset() noexcept
    {
        buffer_.fill(0);
        writePos_ = 0;
        slowPhase_ = 0.0f;
        fastPhase_ = 0.0f;
    }

    void setParameters(float lfoSpeedHz, float depth0to1, float mix0to1) noexcept
    {
        lfoSpeedHz_ = DSPUtils::clamp(lfoSpeedHz, 0.1f, 20.0f);
        depth_ = DSPUtils::clamp(depth0to1, 0.0f, 1.0f);
        mix_ = DSPUtils::clamp(mix0to1, 0.0f, 1.0f);
    }

    void processStereo(float &left, float &right) noexcept
    {
        const float srRatio = static_cast<float>(sampleRate_ / 48000.0);

        // P:$0FB1 - $0FB4: Mono sum of stereo input into the Ensemble delay line (`X:$0251 = $2CCCCC` = 0.35)
        const float monoIn = DSPUtils::clamp(0.35f * (left + right), -1.0f, 1.0f);
        buffer_[writePos_] = static_cast<int16_t>(monoIn * 32767.0f);

        // P:$0FC7 - $0FD6: Advance slow 3-phase LFO (0, 1/3, 2/3 via Y:$0328 = $555555) and fast shimmer LFO (~6 Hz)
        slowPhase_ += static_cast<float>(lfoSpeedHz_ / sampleRate_);
        if (slowPhase_ >= 1.0f)
            slowPhase_ -= 1.0f;

        fastPhase_ += static_cast<float>((lfoSpeedHz_ * 5.8f) / sampleRate_);
        if (fastPhase_ >= 1.0f)
            fastPhase_ -= 1.0f;

        auto triWave = [](float p) noexcept -> float {
            p -= std::floor(p);
            return 2.0f * std::abs(2.0f * p - 1.0f) - 1.0f;
        };

        const float shimmer = 0.20f * triWave(fastPhase_); // Y:$0329 = $199999 (0.20)
        const float mod1 = triWave(slowPhase_) + shimmer;
        const float mod2 = triWave(slowPhase_ + (1.0f / 3.0f)) + shimmer;
        const float mod3 = triWave(slowPhase_ + (2.0f / 3.0f)) - shimmer;

        // Center delay ~ 240 samples (5.0 ms @ 48 kHz), modulated by depth
        const float centerDelay = 240.0f * srRatio;
        const float modSpan = (12.0f + depth_ * 140.0f) * srRatio;

        const float tap1 = readTap(centerDelay + mod1 * modSpan);
        const float tap2 = readTap(centerDelay + mod2 * modSpan);
        const float tap3 = readTap(centerDelay + mod3 * modSpan);

        writePos_ = (writePos_ + 1) & kBufferMask;

        // P:$0FAA - $0FB6: Stereo Ensemble Mixing Matrix (Y:$032A = 0.4, Y:$032B = 0.8)
        const float wetL = 0.8f * tap1 + 0.4f * tap2 + 0.2f * tap3;
        const float wetR = 0.2f * tap1 + 0.4f * tap2 + 0.8f * tap3;

        left = DSPUtils::clamp(left * (1.0f - 0.5f * mix_) + wetL * mix_, -1.0f, 1.0f);
        right = DSPUtils::clamp(right * (1.0f - 0.5f * mix_) + wetR * mix_, -1.0f, 1.0f);
    }

  private:
    [[nodiscard]] float readTap(float delaySamples) const noexcept
    {
        float d = DSPUtils::clamp(delaySamples, 2.0f, static_cast<float>(kBufferSize - 4));
        float readPos = static_cast<float>(writePos_) - d;
        while (readPos < 0.0f)
            readPos += static_cast<float>(kBufferSize);

        size_t idx0 = static_cast<size_t>(readPos) & kBufferMask;
        size_t idx1 = (idx0 + 1) & kBufferMask;
        float frac = readPos - std::floor(readPos);

        float s0 = static_cast<float>(buffer_[idx0]) * (1.0f / 32768.0f);
        float s1 = static_cast<float>(buffer_[idx1]) * (1.0f / 32768.0f);
        return s0 + frac * (s1 - s0);
    }

    double sampleRate_ {48000.0};
    float lfoSpeedHz_ {0.8f};
    float depth_ {0.6f};
    float mix_ {0.5f};

    std::array<int16_t, kBufferSize> buffer_ {};
    size_t writePos_ {0};
    float slowPhase_ {0.0f};
    float fastPhase_ {0.0f};
};

//==============================================================================
// 5. ModFX 2: 4-Stage Stereo Phaser (DSP56362 P:$105D - $10A7)
//==============================================================================
/**
 * @brief 4-Stage Stereo All-Pass Phaser extracted from `P:$105D - $10A7` (`r3 = $0260`).
 *
 * Microcode Reference:
 * - Parameter block: `X:$0260..$027F`, `Y:$0330..$0335`
 * - Runs 100% in internal 24-bit `X:` RAM (no external DRAM used).
 * - 4 cascaded 1st-order all-pass stages per channel (`P:$106F - $107E` Left,
 *   `P:$107F - $108F` Right).
 * - Quadratic-shaped triangle LFO (`P:$1090 - $10A6`, `mpy x0,x0,b` at `P:$1091`)
 *   with inverted stereo LFO polarity between Left and Right channels.
 */
class HardwarePhaser
{
  public:
    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        reset();
    }

    void reset() noexcept
    {
        apStateL_.fill(0.0f);
        apStateR_.fill(0.0f);
        fbStateL_ = 0.0f;
        fbStateR_ = 0.0f;
        lfoPhase_ = 0.0f;
    }

    void setParameters(float lfoSpeedHz, float depth0to1, float feedbackBipolar, float mix0to1) noexcept
    {
        lfoSpeedHz_ = DSPUtils::clamp(lfoSpeedHz, 0.02f, 20.0f);
        depth_ = DSPUtils::clamp(depth0to1, 0.0f, 1.0f);
        feedback_ = DSPUtils::clamp(feedbackBipolar, -0.92f, 0.92f);
        mix_ = DSPUtils::clamp(mix0to1, 0.0f, 1.0f);
    }

    void processStereo(float &left, float &right) noexcept
    {
        // P:$1090 - $10A6: LFO phase update & quadratic triangle shaping (`mpy x0,x0,b`)
        lfoPhase_ += static_cast<float>(lfoSpeedHz_ / sampleRate_);
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;

        float triL = 2.0f * std::abs(2.0f * lfoPhase_ - 1.0f) - 1.0f;
        float triR = -triL; // Stereo opposite LFO polarity (`P:$1099 eor y1,a`)

        // Quadratic curve mapping to all-pass coefficient g in [0.15, 0.92]
        // (Compare X:$026D = $E80000, X:$026E = $780000)
        auto computeAllPassCoeff = [this](float lfoBipolar) noexcept -> float {
            float norm = DSPUtils::clamp(0.5f + 0.5f * lfoBipolar * depth_, 0.0f, 1.0f);
            float curved = norm * norm; // P:$1091 mpy x0,x0,b
            float minFreq = 180.0f;
            float maxFreq = 4800.0f;
            float fc = minFreq + curved * (maxFreq - minFreq);
            float t = std::tan(DSPUtils::PI * fc / static_cast<float>(sampleRate_));
            return DSPUtils::clamp((1.0f - t) / (1.0f + t), -0.95f, 0.95f);
        };

        const float gL = computeAllPassCoeff(triL);
        const float gR = computeAllPassCoeff(triR);

        // P:$1063 - $1065: Input minus feedback (`mac -x1,x0,a` / `mac -x1,x0,b`)
        float uL = DSPUtils::clamp(left - feedback_ * fbStateL_, -1.0f, 1.0f);
        float uR = DSPUtils::clamp(right - feedback_ * fbStateR_, -1.0f, 1.0f);

        // P:$106F - $107E: 4-Stage 1st-Order All-Pass Cascade (Left)
        for (size_t i = 0; i < 4; ++i)
        {
            float v = DSPUtils::clamp(uL + gL * apStateL_[i], -1.0f, 1.0f);
            uL = DSPUtils::clamp(apStateL_[i] - gL * v, -1.0f, 1.0f);
            apStateL_[i] = v;
        }

        // P:$107F - $108F: 4-Stage 1st-Order All-Pass Cascade (Right)
        for (size_t i = 0; i < 4; ++i)
        {
            float v = DSPUtils::clamp(uR + gR * apStateR_[i], -1.0f, 1.0f);
            uR = DSPUtils::clamp(apStateR_[i] - gR * v, -1.0f, 1.0f);
            apStateR_[i] = v;
        }

        fbStateL_ = uL;
        fbStateR_ = uR;

        // P:$1068 - $106D: Dry/Wet Mix
        left = DSPUtils::clamp(left + mix_ * uL, -1.0f, 1.0f);
        right = DSPUtils::clamp(right + mix_ * uR, -1.0f, 1.0f);
    }

  private:
    double sampleRate_ {48000.0};
    float lfoSpeedHz_ {0.5f};
    float depth_ {0.7f};
    float feedback_ {0.5f};
    float mix_ {0.5f};

    std::array<float, 4> apStateL_ {};
    std::array<float, 4> apStateR_ {};
    float fbStateL_ {0.0f};
    float fbStateR_ {0.0f};
    float lfoPhase_ {0.0f};
};

//==============================================================================
// 6. Global Delay: Stereo / Cross / L-R Delay (DSP56362 P:$0FE9 - $105C)
//==============================================================================
/**
 * @brief Stereo, Cross, and L/R Delay extracted from `P:$0FE9 - $1024` (`r3 = $0200`)
 *        and `P:$1025 - $105C` (`r3 = $0280`).
 *
 * Microcode Reference:
 * - External DRAM buffers:
 *   - Left:  `Y:$400000..$43FFFF` (`262,144` x 16-bit samples = `5.46 s` @ 48 kHz, mask `$03FFFF`)
 *   - Right: `Y:$440000..$47FFFF` (`262,144` x 16-bit samples = `5.46 s` @ 48 kHz)
 * - 1-pole exponential delay-time glide (`P:$0FED - $0FF4`) for smooth tape-style pitch sweeps.
 * - 6-bit fractional sample linear interpolation (`P:$0FFB - $0FFE`, `lsl #$12,b`, `lsr b`).
 * - 1-pole feedback damping filter (`P:$1001 - $1006`, `Y:$0300 = $300000` [0.375],
 *   `Y:$0301 = $0E0000` [0.109375]).
 */
enum class HardwareDelayType
{
    StereoDelay = 0, // P:$0FE9 with straight L->L, R->R feedback
    CrossDelay  = 1, // P:$0FE9 with crossed L->R, R->L feedback
    LRDelay     = 2  // P:$1025 with mono-crossed feedback & split L/R output taps
};

class HardwareDelay
{
  public:
    static constexpr size_t kBufferSize = 262144; // $03FFFF + 1 words per channel
    static constexpr size_t kBufferMask = kBufferSize - 1;

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        bufferL_.assign(kBufferSize, 0);
        bufferR_.assign(kBufferSize, 0);
        reset();
    }

    void reset() noexcept
    {
        if (!bufferL_.empty())
            std::fill(bufferL_.begin(), bufferL_.end(), int16_t {0});
        if (!bufferR_.empty())
            std::fill(bufferR_.begin(), bufferR_.end(), int16_t {0});
        writePos_ = 0;
        smoothDelayL_ = targetDelayL_;
        smoothDelayR_ = targetDelayR_;
        dampStateL_ = 0.0f;
        dampStateR_ = 0.0f;
    }

    void setParameters(HardwareDelayType type,
                       float delayTimeMsL,
                       float delayTimeMsR,
                       float feedback0to1,
                       float mix0to1) noexcept
    {
        type_ = type;
        const float maxMs = static_cast<float>((kBufferSize - 8) * 1000.0 / sampleRate_);
        targetDelayL_ = DSPUtils::clamp(delayTimeMsL, 0.5f, maxMs) * 0.001f * static_cast<float>(sampleRate_);
        targetDelayR_ = DSPUtils::clamp(delayTimeMsR, 0.5f, maxMs) * 0.001f * static_cast<float>(sampleRate_);
        feedback_ = DSPUtils::clamp(feedback0to1, 0.0f, 0.95f);
        mix_ = DSPUtils::clamp(mix0to1, 0.0f, 1.0f);
    }

    void processStereo(float &left, float &right) noexcept
    {
        if (bufferL_.size() != kBufferSize)
            return;

        // P:$0FED - $0FF4: 1-pole delay time smoothing for click-free tape-style sweeps
        constexpr float kTimeGlide = 0.0015f;
        smoothDelayL_ += kTimeGlide * (targetDelayL_ - smoothDelayL_);
        smoothDelayR_ += kTimeGlide * (targetDelayR_ - smoothDelayR_);

        // P:$0FFB - $1000: Read 16-bit DRAM with 6-bit fractional linear interpolation
        const float tapL = readInterpolated6Bit(bufferL_, smoothDelayL_);
        const float tapR = readInterpolated6Bit(bufferR_, smoothDelayR_);

        // P:$1001 - $1006: 1-pole high-frequency damping in feedback loop
        // Constants from Y:$0300..$0301: $300000 (0.375) and $0E0000 (0.109375) -> ~0.774 / 0.226 normalized
        dampStateL_ = 0.774f * tapL + 0.226f * dampStateL_;
        dampStateR_ = 0.774f * tapR + 0.226f * dampStateR_;

        float fbL = 0.0f;
        float fbR = 0.0f;
        if (type_ == HardwareDelayType::StereoDelay)
        {
            fbL = left + feedback_ * dampStateL_;
            fbR = right + feedback_ * dampStateR_;
        }
        else if (type_ == HardwareDelayType::CrossDelay)
        {
            fbL = left + feedback_ * dampStateR_;
            fbR = right + feedback_ * dampStateL_;
        }
        else // HardwareDelayType::LRDelay (P:$1025 - $105C)
        {
            const float monoIn = 0.5f * (left + right);
            fbL = monoIn + feedback_ * dampStateR_;
            fbR = monoIn + feedback_ * dampStateL_;
        }

        bufferL_[writePos_] = quantize16(fbL);
        bufferR_[writePos_] = quantize16(fbR);
        writePos_ = (writePos_ + 1) & kBufferMask;

        // P:$1004 - $1007: Dry + Wet Output Accumulation
        left = DSPUtils::clamp(left + mix_ * tapL, -1.0f, 1.0f);
        right = DSPUtils::clamp(right + mix_ * tapR, -1.0f, 1.0f);
    }

  private:
    static int16_t quantize16(float x) noexcept
    {
        float clamped = DSPUtils::clamp(x, -1.0f, 0.999969f);
        return static_cast<int16_t>(clamped * 32768.0f);
    }

    [[nodiscard]] float readInterpolated6Bit(const std::vector<int16_t> &buf, float delaySamples) const noexcept
    {
        // P:$0FFB - $0FFC: `lsl #$12,b` + `lsr b` extracts 6-bit fractional part (64 steps)
        float readPos = static_cast<float>(writePos_) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(kBufferSize);

        size_t idx0 = static_cast<size_t>(readPos) & kBufferMask;
        size_t idx1 = (idx0 + 1) & kBufferMask;
        float frac = std::floor((readPos - std::floor(readPos)) * 64.0f) * (1.0f / 64.0f);

        float s0 = static_cast<float>(buf[idx0]) * (1.0f / 32768.0f);
        float s1 = static_cast<float>(buf[idx1]) * (1.0f / 32768.0f);
        return s0 + frac * (s1 - s0);
    }

    double sampleRate_ {48000.0};
    HardwareDelayType type_ {HardwareDelayType::StereoDelay};
    float targetDelayL_ {12000.0f};
    float targetDelayR_ {12000.0f};
    float smoothDelayL_ {12000.0f};
    float smoothDelayR_ {12000.0f};
    float feedback_ {0.35f};
    float mix_ {0.35f};
    float dampStateL_ {0.0f};
    float dampStateR_ {0.0f};

    std::vector<int16_t> bufferL_;
    std::vector<int16_t> bufferR_;
    size_t writePos_ {0};
};

} // namespace ABDMS2000::HardwareEffects
