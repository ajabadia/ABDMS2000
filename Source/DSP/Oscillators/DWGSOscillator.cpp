#include "DWGSOscillator.h"
#include "../Common/DSPUtils.h"
#include <algorithm>

namespace ABDMS2000 {

void DWGSOscillator::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    tableData_ = DWGSTables::getTableData();
    setFrequency(frequency_);
    reset();
}

void DWGSOscillator::reset(float initialPhase) noexcept
{
    phase_ = static_cast<double>(initialPhase);
    while (phase_ >= 1.0) phase_ -= 1.0;
    while (phase_ < 0.0)  phase_ += 1.0;
}

void DWGSOscillator::setFrequency(float frequencyHz) noexcept
{
    frequency_ = DSPUtils::clamp(frequencyHz, 0.1f, static_cast<float>(sampleRate_ * 0.499));
    phaseIncrement_ = static_cast<double>(frequency_ / sampleRate_);
}

void DWGSOscillator::setWaveIndex(int index) noexcept
{
    currentWaveIndex_ = std::max(0, std::min(static_cast<int>(DWGSTables::MAX_EXPANDED_TABLES - 1), index));
}

float DWGSOscillator::getNextSample() noexcept
{
    if (tableData_ == nullptr) return 0.0f;

    const size_t tableOffset = currentWaveIndex_ * DWGSTables::SAMPLES_PER_TABLE;
    const double scaledIndex = phase_ * static_cast<double>(DWGSTables::SAMPLES_PER_TABLE);
    const size_t index0 = static_cast<size_t>(scaledIndex) % DWGSTables::SAMPLES_PER_TABLE;
    const size_t index1 = (index0 + 1) % DWGSTables::SAMPLES_PER_TABLE;
    const float frac = static_cast<float>(scaledIndex - static_cast<double>(index0));

    const float s0 = tableData_[tableOffset + index0];
    const float s1 = tableData_[tableOffset + index1];

    // Linear interpolation
    const float out = s0 + frac * (s1 - s0);

    phase_ += phaseIncrement_;
    if (phase_ >= 1.0)
    {
        phase_ -= 1.0;
    }

    return out;
}

} // namespace ABDMS2000
