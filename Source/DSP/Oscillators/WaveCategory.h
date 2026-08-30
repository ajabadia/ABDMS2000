#pragma once
#include <cstdint>
#include <string>

namespace ABDMS2000 {

/**
 * @brief Canonic wave categories for the ABDMS2000 wavetable browser.
 *
 * Used in Advanced Mode to filter/search across 512 wavetable slots.
 * Categories are mapped to CSS tokens for themed UI rendering.
 */
enum class WaveCategory : uint8_t {
    // ── Hardware Korg DWGS (slots 0..63) ──
    SynthWave   = 0,   // SynWave 1-8, 5th Wave 1-3
    DigiBell    = 1,   // Digi 1-8, Bell 1-4
    Organ       = 2,   // E.Organ 1-8, Endless
    PianoClav   = 3,   // A.Piano 1-3, E.P. 1-5, Wurl 1, Clav 1-2
    Guitar      = 4,   // A.Guitar 1-2, E.Guitar 1-3, Mute Gt
    Bass        = 5,   // A.Bass, E.Bass 1-2, Synth Bass 1-4
    Strings     = 6,   // Strings 1-2, Brass
    Voice       = 7,   // Voice 1-4

    // ── AKWF Expansion (slots 128..255) ──
    Basic       = 8,   // Saw, Square, Triangle, Sine variants
    BassLead    = 9,   // Bass and lead synth waves
    PadAmbient  = 10,  // Pads, atmospheres, evolving textures
    Organic     = 11,  // Acoustic, vocal, choir, natural
    BrassWind   = 12,  // Brass, wind, reed instruments
    KeysMallets = 13,  // Piano, EP, clav, bells, mallets
    FxMetal     = 14,  // FX, metallic, industrial, noise
    Chip8Bit    = 15,  // 8-bit, NES, chiptune, game audio

    // ── User bank (slots 256..511) ──
    User        = 16,  // Drag-and-drop user waves

    Count       = 17
};

/** Human-readable category names for UI */
inline const char* getCategoryName(WaveCategory cat) noexcept {
    switch (cat) {
        case WaveCategory::SynthWave:   return "Synth Wave";
        case WaveCategory::DigiBell:    return "Bell / Digital";
        case WaveCategory::Organ:       return "Organ";
        case WaveCategory::PianoClav:   return "Piano / Clav";
        case WaveCategory::Guitar:      return "Guitar";
        case WaveCategory::Bass:        return "Bass";
        case WaveCategory::Strings:     return "Strings / Brass";
        case WaveCategory::Voice:       return "Voice";
        case WaveCategory::Basic:       return "Basic Shapes";
        case WaveCategory::BassLead:    return "Bass / Lead";
        case WaveCategory::PadAmbient:  return "Pad / Ambient";
        case WaveCategory::Organic:     return "Organic / Vocal";
        case WaveCategory::BrassWind:   return "Brass / Wind";
        case WaveCategory::KeysMallets: return "Keys / Mallets";
        case WaveCategory::FxMetal:     return "FX / Metal";
        case WaveCategory::Chip8Bit:    return "Chip / 8-Bit";
        case WaveCategory::User:        return "User Waves";
        default: return "Unknown";
    }
}

/** CSS token name for themed UI rendering */
inline const char* getCategoryCssClass(WaveCategory cat) noexcept {
    switch (cat) {
        case WaveCategory::SynthWave:   return "cat-synthwave";
        case WaveCategory::DigiBell:    return "cat-digibell";
        case WaveCategory::Organ:       return "cat-organ";
        case WaveCategory::PianoClav:   return "cat-piano";
        case WaveCategory::Guitar:      return "cat-guitar";
        case WaveCategory::Bass:        return "cat-bass";
        case WaveCategory::Strings:     return "cat-strings";
        case WaveCategory::Voice:       return "cat-voice";
        case WaveCategory::Basic:       return "cat-basic";
        case WaveCategory::BassLead:    return "cat-basslead";
        case WaveCategory::PadAmbient:  return "cat-pad";
        case WaveCategory::Organic:     return "cat-organic";
        case WaveCategory::BrassWind:   return "cat-brass";
        case WaveCategory::KeysMallets: return "cat-keys";
        case WaveCategory::FxMetal:     return "cat-fx";
        case WaveCategory::Chip8Bit:    return "cat-chip";
        case WaveCategory::User:        return "cat-user";
        default: return "";
    }
}

/**
 * @brief Static category assignment for standard DWGS slots 0..63.
 * Maps each of the 64 canonical waves to its WaveCategory.
 */
inline WaveCategory getDWGSStandardCategory(size_t slot) noexcept {
    if (slot < 8)       return WaveCategory::SynthWave;   // SynWave 1-8
    if (slot < 11)      return WaveCategory::SynthWave;   // 5th Wave 1-3
    if (slot < 19)      return WaveCategory::DigiBell;    // Digi 1-8
    if (slot == 19)     return WaveCategory::SynthWave;   // Endless
    if (slot < 28)      return WaveCategory::Organ;       // E.Organ 1-8
    if (slot < 30)      return WaveCategory::PianoClav;   // Clav 1-2
    if (slot < 33)      return WaveCategory::PianoClav;   // A.Piano 1-3
    if (slot < 38)      return WaveCategory::PianoClav;   // E.P. 1-5
    if (slot == 38)     return WaveCategory::PianoClav;   // Wurl
    if (slot < 41)      return WaveCategory::Guitar;      // A.Guitar 1-2
    if (slot < 44)      return WaveCategory::Guitar;      // E.Guitar 1-3
    if (slot == 44)     return WaveCategory::Guitar;      // Mute Gt
    if (slot < 47)      return WaveCategory::Bass;        // A.Bass, E.Bass 1-2
    if (slot < 51)      return WaveCategory::Bass;        // Synth Bass 1-4
    if (slot < 55)      return WaveCategory::DigiBell;    // Bell 1-4
    if (slot < 59)      return WaveCategory::Voice;       // Voice 1-4
    if (slot < 61)      return WaveCategory::Strings;     // Strings 1-2
                        return WaveCategory::Strings;     // Brass
}

} // namespace ABDMS2000