#include "../Core/HardwareConstants.h"
#include "../Core/MS2000FirmwareTables.h"
#include "../DSP/Modulation/LFO.h"
#include "DWGSBinary.h"
#include "LfoFirmwareParityTests.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ABDMS2000::Tests
{

template <size_t NumEntries>
static bool lfoRomTableMatches(const uint8_t *rom,
                               size_t romOffset,
                               const uint32_t (&table)[NumEntries]) noexcept
{
    for (size_t index = 0; index < NumEntries; ++index)
    {
        const uint32_t expectedWord = table[index] << 8;
        const size_t entryOffset = romOffset + index * 4u;
        for (unsigned int byte = 0; byte < 4; ++byte)
        {
            const unsigned int shift = 24u - (byte * 8u);
            if (rom[entryOffset + byte] != static_cast<uint8_t>((expectedWord >> shift) & 0xFFu))
                return false;
        }
    }
    return true;
}

static bool lfoRomTablesMatchEmbeddedFirmware() noexcept
{
    constexpr size_t kLfo1RomOffset = 0x295E8u;
    constexpr size_t kLfo2RomOffset = 0x29BE8u;
    constexpr size_t kRomLfoBytes = 128u * 4u;
    if (DWGSBinary::x811v107_sys == nullptr
        || DWGSBinary::x811v107_sysSize < static_cast<int>(kLfo2RomOffset + kRomLfoBytes))
        return false;

    const auto *rom = reinterpret_cast<const uint8_t *>(DWGSBinary::x811v107_sys);
    return lfoRomTableMatches(rom, kLfo1RomOffset, FirmwareTables::kRomLfo1IncrementQ23)
           && lfoRomTableMatches(rom, kLfo2RomOffset, FirmwareTables::kRomLfo2IncrementQ23);
}

static void selectWaveform(LFO &lfo, bool useLfo2, LFOWaveform wave = LFOWaveform::Sawtooth) noexcept
{
    if (useLfo2)
        lfo.setWaveformLFO2(static_cast<LFOWaveformLFO2>(wave));
    else
        lfo.setWaveformLFO1(wave);
}

static void selectWaveform(LFO &lfo, bool useLfo2, LFOWaveformLFO2 wave) noexcept
{
    (void)useLfo2;
    lfo.setWaveformLFO2(wave);
}

static float measureFrequencyHz(LFO &lfo, int samples, float modulationOctaves = 0.0f) noexcept
{
    lfo.reset();
    int risingCrossings = 0;
    float previous = lfo.getNextSample(modulationOctaves);
    for (int i = 1; i < samples; ++i)
    {
        const float current = lfo.getNextSample(modulationOctaves);
        if (previous <= 0.0f && current > 0.0f)
            ++risingCrossings;
        previous = current;
    }
    return static_cast<float>(risingCrossings) * 48000.0f / static_cast<float>(samples);
}

bool runLfoFirmwareParityTests() noexcept
{
    if (!lfoRomTablesMatchEmbeddedFirmware())
        return false;

    constexpr int kSamplesPerSecond = 48000;
    constexpr float kToleranceHz = 1.1f;
    for (int oscillatorIndex = 0; oscillatorIndex < 2; ++oscillatorIndex)
    {
        const bool useLfo2 = oscillatorIndex != 0;
        LFO oscillator;
        oscillator.prepare(48000.0);
        selectWaveform(oscillator, useLfo2);

        oscillator.setFrequencyHz(600.0f);
        const float legacyBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(legacyBaseHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        oscillator.setFrequencyHz(600.0f);
        const float hardwareBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(hardwareBaseHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setFrequencyHz(200.0f);
        const float hardwareModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(hardwareModulatedHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(false);
        oscillator.setFrequencyHz(Constants::kLfoDefaultMaxFrequencyHz);
        const float legacyModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(legacyModulatedHz - Constants::kLfoNonHardwareModulationMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setBpm(399.0);
        oscillator.setTempoSync(true, 14);
        const float legacySyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(legacySyncedHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        const float hardwareSyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(hardwareSyncedHz - (399.0f / 60.0f * 8.0f)) >= kToleranceHz)
            return false;

        oscillator.setBpm(300.0);
        const float retunedHardwareSyncHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(retunedHardwareSyncHz - 40.0f) >= kToleranceHz)
            return false;
    }

    // Test LFO1 Square waveform
    {
        LFO oscillator;
        oscillator.prepare(48000.0);
        selectWaveform(oscillator, false, LFOWaveform::Square);

        oscillator.setFrequencyHz(600.0f);
        const float squareBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(squareBaseHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        oscillator.setFrequencyHz(600.0f);
        const float squareHardwareBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(squareHardwareBaseHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setFrequencyHz(200.0f);
        const float squareHardwareModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(squareHardwareModulatedHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(false);
        oscillator.setFrequencyHz(Constants::kLfoDefaultMaxFrequencyHz);
        const float squareLegacyModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(squareLegacyModulatedHz - Constants::kLfoNonHardwareModulationMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setBpm(399.0);
        oscillator.setTempoSync(true, 14);
        const float squareLegacySyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(squareLegacySyncedHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        const float squareHardwareSyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(squareHardwareSyncedHz - (399.0f / 60.0f * 8.0f)) >= kToleranceHz)
            return false;

        oscillator.setBpm(300.0);
        const float squareRetunedHardwareSyncHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(squareRetunedHardwareSyncHz - 40.0f) >= kToleranceHz)
            return false;
    }

    // Test LFO1 Triangle waveform
    {
        LFO oscillator;
        oscillator.prepare(48000.0);
        selectWaveform(oscillator, false, LFOWaveform::Triangle);

        oscillator.setFrequencyHz(600.0f);
        const float triangleBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(triangleBaseHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        oscillator.setFrequencyHz(600.0f);
        const float triangleHardwareBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(triangleHardwareBaseHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setFrequencyHz(200.0f);
        const float triangleHardwareModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(triangleHardwareModulatedHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(false);
        oscillator.setFrequencyHz(Constants::kLfoDefaultMaxFrequencyHz);
        const float triangleLegacyModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(triangleLegacyModulatedHz - Constants::kLfoNonHardwareModulationMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setBpm(399.0);
        oscillator.setTempoSync(true, 14);
        const float triangleLegacySyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(triangleLegacySyncedHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        const float triangleHardwareSyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(triangleHardwareSyncedHz - (399.0f / 60.0f * 8.0f)) >= kToleranceHz)
            return false;

        oscillator.setBpm(300.0);
        const float triangleRetunedHardwareSyncHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(triangleRetunedHardwareSyncHz - 40.0f) >= kToleranceHz)
            return false;
    }

    // Test LFO2 Square+ waveform
    {
        LFO oscillator;
        oscillator.prepare(48000.0);
        selectWaveform(oscillator, true, LFOWaveformLFO2::SquarePlus);

        oscillator.setFrequencyHz(600.0f);
        const float lfo2SquarePlusBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SquarePlusBaseHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        oscillator.setFrequencyHz(600.0f);
        const float lfo2SquarePlusHardwareBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SquarePlusHardwareBaseHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setFrequencyHz(200.0f);
        const float lfo2SquarePlusHardwareModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(lfo2SquarePlusHardwareModulatedHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(false);
        oscillator.setFrequencyHz(Constants::kLfoDefaultMaxFrequencyHz);
        const float lfo2SquarePlusLegacyModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(lfo2SquarePlusLegacyModulatedHz - Constants::kLfoNonHardwareModulationMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setBpm(399.0);
        oscillator.setTempoSync(true, 14);
        const float lfo2SquarePlusLegacySyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SquarePlusLegacySyncedHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        const float lfo2SquarePlusHardwareSyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SquarePlusHardwareSyncedHz - (399.0f / 60.0f * 8.0f)) >= kToleranceHz)
            return false;

        oscillator.setBpm(300.0);
        const float lfo2SquarePlusRetunedHardwareSyncHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SquarePlusRetunedHardwareSyncHz - 40.0f) >= kToleranceHz)
            return false;
    }

    // Test LFO2 Sine waveform
    {
        LFO oscillator;
        oscillator.prepare(48000.0);
        selectWaveform(oscillator, true, LFOWaveformLFO2::Sine);

        oscillator.setFrequencyHz(600.0f);
        const float lfo2SineBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SineBaseHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        oscillator.setFrequencyHz(600.0f);
        const float lfo2SineHardwareBaseHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SineHardwareBaseHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setFrequencyHz(200.0f);
        const float lfo2SineHardwareModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(lfo2SineHardwareModulatedHz - Constants::kLfoHardwareMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(false);
        oscillator.setFrequencyHz(Constants::kLfoDefaultMaxFrequencyHz);
        const float lfo2SineLegacyModulatedHz = measureFrequencyHz(oscillator, kSamplesPerSecond, 4.0f);
        if (std::abs(lfo2SineLegacyModulatedHz - Constants::kLfoNonHardwareModulationMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setBpm(399.0);
        oscillator.setTempoSync(true, 14);
        const float lfo2SineLegacySyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SineLegacySyncedHz - Constants::kLfoDefaultMaxFrequencyHz) >= kToleranceHz)
            return false;

        oscillator.setHardwareMS2000Mode(true);
        const float lfo2SineHardwareSyncedHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SineHardwareSyncedHz - (399.0f / 60.0f * 8.0f)) >= kToleranceHz)
            return false;

        oscillator.setBpm(300.0);
        const float lfo2SineRetunedHardwareSyncHz = measureFrequencyHz(oscillator, kSamplesPerSecond);
        if (std::abs(lfo2SineRetunedHardwareSyncHz - 40.0f) >= kToleranceHz)
            return false;
    }

    return true;
}

}  // namespace ABDMS2000::Tests
