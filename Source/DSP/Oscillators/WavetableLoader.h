#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace ABDMS2000 {

/**
 * @brief Utility functions for loading wavetables from raw binary formats.
 *
 * Supports:
 * - Korg .m1 format: raw 16-bit big-endian signed PCM, single-cycle waves
 * - Standard .wav format: 16-bit PCM mono single-cycle waves
 * - Automatic resampling to 2048-sample target size
 */
namespace WavetableLoader {

constexpr size_t TARGET_SIZE = 2048;

/**
 * @brief Load raw 16-bit big-endian signed PCM samples into a float vector.
 * @param data   Pointer to raw byte buffer
 * @param size   Buffer size in bytes
 * @param out    Output vector of normalized float samples [-1.0, +1.0]
 * @return true on success
 */
inline bool loadRaw16BitBE(const uint8_t* data, size_t size, std::vector<float>& out) noexcept
{
    if (data == nullptr || size < 2) return false;

    const size_t numSamples = size / 2;
    out.resize(numSamples);

    float maxVal = 0.0001f;
    for (size_t i = 0; i < numSamples; ++i)
    {
        // Big-endian: high byte first
        int16_t sample = (static_cast<int16_t>(data[i * 2]) << 8)
                       |  static_cast<int16_t>(data[i * 2 + 1]);
        out[i] = static_cast<float>(sample) / 32768.0f;
        maxVal = std::max(maxVal, std::abs(out[i]));
    }

    // Normalize
    float invMax = 1.0f / maxVal;
    for (auto& s : out) s *= invMax;

    return true;
}

/**
 * @brief Load 16-bit signed little-endian PCM samples into a float vector.
 */
inline bool loadRaw16BitLE(const uint8_t* data, size_t size, std::vector<float>& out) noexcept
{
    if (data == nullptr || size < 2) return false;

    const size_t numSamples = size / 2;
    out.resize(numSamples);

    float maxVal = 0.0001f;
    for (size_t i = 0; i < numSamples; ++i)
    {
        int16_t sample = (static_cast<int16_t>(data[i * 2 + 1]) << 8)
                       |  static_cast<int16_t>(data[i * 2]);
        out[i] = static_cast<float>(sample) / 32768.0f;
        maxVal = std::max(maxVal, std::abs(out[i]));
    }

    float invMax = 1.0f / maxVal;
    for (auto& s : out) s *= invMax;

    return true;
}

/**
 * @brief Load a standard RIFF WAV file (16-bit PCM mono) into float samples.
 * Returns raw samples — caller should resample to target size.
 */
inline bool loadWav(const uint8_t* data, size_t size, std::vector<float>& out) noexcept
{
    if (data == nullptr || size < 44) return false;

    // Minimal RIFF WAV parser
    if (std::memcmp(data, "RIFF", 4) != 0) return false;
    if (std::memcmp(data + 8, "WAVE", 4) != 0) return false;

    // Find 'fmt ' and 'data' chunks
    size_t pos = 12;
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t sampleRate = 44100;
    const uint8_t* audioData = nullptr;
    uint32_t audioDataSize = 0;

    while (pos + 8 <= size)
    {
        uint32_t chunkSize = (static_cast<uint32_t>(data[pos + 4]))
                           | (static_cast<uint32_t>(data[pos + 5]) << 8)
                           | (static_cast<uint32_t>(data[pos + 6]) << 16)
                           | (static_cast<uint32_t>(data[pos + 7]) << 24);

        if (std::memcmp(data + pos, "fmt ", 4) == 0)
        {
            if (pos + 8 + chunkSize > size) return false;
            numChannels  = static_cast<uint16_t>(data[pos + 10]) | (static_cast<uint16_t>(data[pos + 11]) << 8);
            bitsPerSample = static_cast<uint16_t>(data[pos + 22]) | (static_cast<uint16_t>(data[pos + 23]) << 8);
            sampleRate   = (static_cast<uint32_t>(data[pos + 12]))
                         | (static_cast<uint32_t>(data[pos + 13]) << 8)
                         | (static_cast<uint32_t>(data[pos + 14]) << 16)
                         | (static_cast<uint32_t>(data[pos + 15]) << 24);
        }
        else if (std::memcmp(data + pos, "data", 4) == 0)
        {
            audioData = data + pos + 8;
            audioDataSize = chunkSize;
            if (static_cast<size_t>(pos + 8 + chunkSize) > size)
                audioDataSize = static_cast<uint32_t>(size - (pos + 8));
            break;
        }

        pos += 8 + chunkSize;
    }

    if (audioData == nullptr || audioDataSize == 0) return false;
    if (bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 8) return false;

    // Load 16-bit PCM
    if (bitsPerSample == 16)
    {
        const size_t numSamples = audioDataSize / 2;
        out.resize(numSamples);

        float maxVal = 0.0001f;
        for (size_t i = 0; i < numSamples; ++i)
        {
            int16_t s = (static_cast<int16_t>(audioData[i * 2 + 1]) << 8)
                      |  static_cast<int16_t>(audioData[i * 2]);
            out[i] = static_cast<float>(s) / 32768.0f;
            maxVal = std::max(maxVal, std::abs(out[i]));
        }

        float invMax = 1.0f / maxVal;
        for (auto& s : out) s *= invMax;
    }
    else if (bitsPerSample == 8)
    {
        const size_t numSamples = audioDataSize;
        out.resize(numSamples);
        float maxVal = 0.0001f;
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = (static_cast<float>(audioData[i]) - 128.0f) / 128.0f;
            maxVal = std::max(maxVal, std::abs(out[i]));
        }
        float invMax = 1.0f / maxVal;
        for (auto& s : out) s *= invMax;
    }

    return !out.empty();
}

/**
 * @brief Resample a source wavetable to TARGET_SIZE (2048) samples using linear interpolation.
 */
inline void resample(const std::vector<float>& src, float* dest) noexcept
{
    if (src.empty() || dest == nullptr) return;

    float maxVal = 0.0001f;
    for (size_t i = 0; i < TARGET_SIZE; ++i)
    {
        double pos = (static_cast<double>(i) / static_cast<double>(TARGET_SIZE))
                   * static_cast<double>(src.size());
        size_t idx0 = static_cast<size_t>(pos) % src.size();
        size_t idx1 = (idx0 + 1) % src.size();
        float frac = static_cast<float>(pos - static_cast<double>(idx0));

        dest[i] = src[idx0] + frac * (src[idx1] - src[idx0]);
        maxVal = std::max(maxVal, std::abs(dest[i]));
    }

    float invMax = 1.0f / maxVal;
    for (size_t i = 0; i < TARGET_SIZE; ++i) dest[i] *= invMax;
}

/**
 * @brief Load a .m1 bank file and extract individual single-cycle waves.
 *
 * Korg .m1 format: raw 16-bit big-endian signed PCM, contiguous waves.
 * @param data       Raw .m1 file bytes
 * @param size       File size in bytes
 * @param numWaves   Number of single-cycle waves in the file
 * @param out        Output: populated with numWaves float vectors
 * @return true on success
 */
inline bool loadM1Bank(const uint8_t* data, size_t size, size_t numWaves,
                       std::vector<std::vector<float>>& out) noexcept
{
    if (data == nullptr || size < 4 || numWaves == 0) return false;

    const size_t samplesPerWave = (size / 2) / numWaves;
    if (samplesPerWave < 4) return false;

    out.resize(numWaves);

    for (size_t w = 0; w < numWaves; ++w)
    {
        const uint8_t* waveStart = data + (w * samplesPerWave * 2);
        if (!loadRaw16BitBE(waveStart, samplesPerWave * 2, out[w]))
            return false;
    }

    return true;
}

} // namespace WavetableLoader

} // namespace ABDMS2000