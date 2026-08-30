#pragma once
#include "WaveCategory.h"
#include <vector>
#include <string>
#include <cstdint>

namespace ABDMS2000 {

struct WaveEntry {
    std::string name;           // Display name
    WaveCategory category;      // Category for filtering
    size_t slot;                // Slot index (0..511)
};

/**
 * @brief DWGS Wavetable Engine — 512-slot architecture.
 *
 * Slot map:
 *    0..63    Standard DWGS (Korg DW-8000 .m1, 32 real + 32 procedural)
 *   64..127   Reserved — Prophet VS / Korg T-Series (loaded at runtime)
 *  128..255   AKWF Curated Bank — 128 waves from Adventure Kid library
 *  256..511   User Bank — drag-and-drop WAV loading at runtime
 */
class DWGSTables {
public:
    static constexpr size_t NUM_STANDARD_TABLES  = 64;
    static constexpr size_t NUM_AKWF_TABLES      = 128;
    static constexpr size_t MAX_EXPANDED_TABLES  = 512;
    static constexpr size_t SAMPLES_PER_TABLE    = 2048;
    static constexpr size_t TOTAL_MAX_SAMPLES    = MAX_EXPANDED_TABLES * SAMPLES_PER_TABLE;

    static constexpr size_t AKWF_START_SLOT      = 128;
    static constexpr size_t USER_START_SLOT      = 256;

    // ── Core access ──
    static const float* getTableData() noexcept;
    static const char* getWaveName(size_t index) noexcept;
    static WaveCategory getWaveCategory(size_t index) noexcept;
    static size_t getTotalWaveforms() noexcept;

    // ── Advanced Mode: wavetable browser & search ──
    static const std::vector<WaveEntry>& getCatalog() noexcept;
    static std::vector<WaveEntry> search(const std::string& query) noexcept;
    static std::vector<WaveEntry> filterByCategory(WaveCategory cat) noexcept;

    // ── User bank: runtime loading ──
    static bool registerWaveform(size_t index, const std::string& name,
                                 WaveCategory cat, const float* samples,
                                 size_t numSamples) noexcept;
    static bool loadWavToSlot(size_t slot, const std::string& name,
                              WaveCategory cat, const uint8_t* data,
                              size_t size) noexcept;
    static bool loadM1Bank(const uint8_t* data, size_t size,
                           size_t startSlot, size_t numWaves,
                           WaveCategory cat) noexcept;

private:
    static void initTables() noexcept;
    static void generateStandardProcedural(size_t slot) noexcept;
    static void loadAKWFCurated() noexcept;

    static std::vector<float> wavetableMemory_;
    static std::vector<std::string> waveNames_;
    static std::vector<WaveCategory> waveCategories_;
    static std::vector<WaveEntry> catalog_;
    static bool isInitialized_;
};

} // namespace ABDMS2000