#include "DWGSTables.h"
#include "WavetableLoader.h"
#include "../Common/DSPUtils.h"
#if __has_include("DWGSBinary.h")
#include "DWGSBinary.h"
#else
namespace DWGSBinary {
    inline const char* getNamedResource(const char*, int& sz) { sz = 0; return nullptr; }
}
#endif
#include <cmath>
#include <algorithm>
#include <string>

namespace ABDMS2000 {

std::vector<float> DWGSTables::wavetableMemory_;
std::vector<std::string> DWGSTables::waveNames_;
std::vector<WaveCategory> DWGSTables::waveCategories_;
std::vector<WaveEntry> DWGSTables::catalog_;
bool DWGSTables::isInitialized_ = false;

// === DWGS Standard Names (Slots 0-63) ===
static const char* kDWGSStandardNames[64] = {
    "SynWave 1","SynWave 2","SynWave 3","SynWave 4","SynWave 5","SynWave 6","SynWave 7","SynWave 8",
    "5th Wave 1","5th Wave 2","5th Wave 3","Digi 1","Digi 2","Digi 3","Digi 4","Digi 5",
    "Digi 6","Digi 7","Digi 8","Endless","E.Organ 1","E.Organ 2","E.Organ 3","E.Organ 4",
    "E.Organ 5","E.Organ 6","E.Organ 7","E.Organ 8","Clav 1","Clav 2","A.Piano 1","A.Piano 2",
    "A.Piano 3","E.P. 1","E.P. 2","E.P. 3","E.P. 4","E.P. 5","Wurl 1","A.Guitar 1",
    "A.Guitar 2","E.Guitar 1","E.Guitar 2","E.Guitar 3","Mute Gt","A.Bass","E.Bass 1","E.Bass 2",
    "Synth Bass 1","Synth Bass 2","Synth Bass 3","Synth Bass 4","Bell 1","Bell 2","Bell 3","Bell 4",
    "Voice 1","Voice 2","Voice 3","Voice 4","Strings 1","Strings 2","Brass","SynBrass"
};

// === Prophet VS expansion (Slots 64-127): 96 vector synthesis waves ===
static const char* kProphetVSNames[96] = {
    // Wave 33-56
    "PVS_Digital 1","PVS_Digital 2","PVS_FM Bell","PVS_Digi Bell","PVS_Syn Mallet",
    "PVS_Metal 1","PVS_Metal 2","PVS_Reso 1","PVS_Reso 2","PVS_Wave 42","PVS_Wave 43","PVS_Wave 44",
    "PVS_Wave 45","PVS_Wave 46","PVS_Wave 47","PVS_Wave 48","PVS_Wave 49","PVS_Wave 50",
    "PVS_Wave 51","PVS_Wave 52","PVS_Wave 53","PVS_Wave 54","PVS_Wave 55","PVS_Wave 56",
    // Wave 57-80
    "PVS_Brass 1","PVS_Brass 2","PVS_Wind 1","PVS_Wind 2","PVS_Wind 3",
    "PVS_Strings 1","PVS_Strings 2","PVS_Strings 3","PVS_Strings 4","PVS_Voice 1","PVS_Voice 2","PVS_Voice 3",
    "PVS_Voice 4","PVS_Choir 1","PVS_Choir 2","PVS_Choir 3","PVS_Organ 1","PVS_Organ 2",
    "PVS_Wave 75","PVS_Wave 76","PVS_Wave 77","PVS_Wave 78","PVS_Wave 79","PVS_Wave 80",
    // Wave 81-104
    "PVS_Bass 1","PVS_Bass 2","PVS_Bass 3","PVS_Bass 4","PVS_Lead 1","PVS_Lead 2",
    "PVS_Lead 3","PVS_Synth 1","PVS_Synth 2","PVS_Synth 3","PVS_Synth 4","PVS_Synth 5",
    "PVS_Synth 6","PVS_Wave 93","PVS_Wave 94","PVS_Wave 95","PVS_Wave 96","PVS_Wave 97",
    "PVS_Wave 98","PVS_Wave 99","PVS_Wave 100","PVS_Wave 101","PVS_Wave 102","PVS_Wave 103",
    // Wave 105-128
    "PVS_FX 1","PVS_FX 2","PVS_FX 3","PVS_FX 4","PVS_FX 5","PVS_FX 6",
    "PVS_FX 7","PVS_FX 8","PVS_Perc 1","PVS_Perc 2","PVS_Perc 3","PVS_Perc 4",
    "PVS_Wave 117","PVS_Wave 118","PVS_Wave 119","PVS_Wave 120","PVS_Wave 121","PVS_Wave 122",
    "PVS_Wave 123","PVS_Wave 124","PVS_Wave 125","PVS_Wave 126","PVS_Wave 127","PVS_Wave 128"
};

// === AKWF curated (Slots 128-255) ===
struct AKWFEntry { int fileIdx; int waveInFile; const char* name; WaveCategory cat; };
static const AKWFEntry kAKWF[128] = {
    // Basic Shapes (0-15): file 0 (0001-0100)
    {0,0,"AKWF_Saw",WaveCategory::Basic},{0,1,"AKWF_Saw_Reso",WaveCategory::Basic},
    {0,2,"AKWF_Square",WaveCategory::Basic},{0,3,"AKWF_Square_50",WaveCategory::Basic},
    {0,4,"AKWF_Triangle",WaveCategory::Basic},{0,5,"AKWF_Tri_Soft",WaveCategory::Basic},
    {0,6,"AKWF_Sine",WaveCategory::Basic},{0,7,"AKWF_Sine_Harm",WaveCategory::Basic},
    {0,8,"AKWF_Pulse_25",WaveCategory::Basic},{0,9,"AKWF_Pulse_10",WaveCategory::Basic},
    {0,10,"AKWF_Pulse_5",WaveCategory::Basic},{0,11,"AKWF_Ramp",WaveCategory::Basic},
    {0,12,"AKWF_Sub_Bass",WaveCategory::Basic},{0,13,"AKWF_TB303_Saw",WaveCategory::Basic},
    {0,14,"AKWF_JP8000_Saw",WaveCategory::Basic},{0,15,"AKWF_JP8000_Sq",WaveCategory::Basic},
    // Bass/Lead (16-31): file 0
    {0,16,"AKWF_Bass_1",WaveCategory::BassLead},{0,17,"AKWF_Bass_2",WaveCategory::BassLead},
    {0,18,"AKWF_Bass_FM",WaveCategory::BassLead},{0,19,"AKWF_Bass_Reese",WaveCategory::BassLead},
    {0,20,"AKWF_Lead_1",WaveCategory::BassLead},{0,21,"AKWF_Lead_2",WaveCategory::BassLead},
    {0,22,"AKWF_Lead_Square",WaveCategory::BassLead},{0,23,"AKWF_Mono_Lead",WaveCategory::BassLead},
    {0,24,"AKWF_Acid_1",WaveCategory::BassLead},{0,25,"AKWF_Acid_2",WaveCategory::BassLead},
    {0,26,"AKWF_Hoover",WaveCategory::BassLead},{0,27,"AKWF_DnB_Bass",WaveCategory::BassLead},
    {0,28,"AKWF_Wobble_1",WaveCategory::BassLead},{0,29,"AKWF_Wobble_2",WaveCategory::BassLead},
    {0,30,"AKWF_Dubstep_Growl",WaveCategory::BassLead},{0,31,"AKWF_Neuro_Bass",WaveCategory::BassLead},
    // Pad/Ambient (32-47): file 0
    {0,32,"AKWF_Pad_1",WaveCategory::PadAmbient},{0,33,"AKWF_Pad_2",WaveCategory::PadAmbient},
    {0,34,"AKWF_Pad_3",WaveCategory::PadAmbient},{0,35,"AKWF_Pad_Bright",WaveCategory::PadAmbient},
    {0,36,"AKWF_Pad_Dark",WaveCategory::PadAmbient},{0,37,"AKWF_DigiPad",WaveCategory::PadAmbient},
    {0,38,"AKWF_Sweep_Up",WaveCategory::PadAmbient},{0,39,"AKWF_Sweep_Down",WaveCategory::PadAmbient},
    {0,40,"AKWF_Ambient_1",WaveCategory::PadAmbient},{0,41,"AKWF_Ambient_2",WaveCategory::PadAmbient},
    {0,42,"AKWF_Atmos_1",WaveCategory::PadAmbient},{0,43,"AKWF_Atmos_2",WaveCategory::PadAmbient},
    {0,44,"AKWF_Glass",WaveCategory::PadAmbient},{0,45,"AKWF_Crystal",WaveCategory::PadAmbient},
    {0,46,"AKWF_Evolving_1",WaveCategory::PadAmbient},{0,47,"AKWF_Evolving_2",WaveCategory::PadAmbient},
    // Organic/Vocal (48-63): file 0
    {0,48,"AKWF_Vocal_A",WaveCategory::Organic},{0,49,"AKWF_Vocal_E",WaveCategory::Organic},
    {0,50,"AKWF_Vocal_I",WaveCategory::Organic},{0,51,"AKWF_Vocal_O",WaveCategory::Organic},
    {0,52,"AKWF_Vocal_U",WaveCategory::Organic},{0,53,"AKWF_Choir_1",WaveCategory::Organic},
    {0,54,"AKWF_Choir_2",WaveCategory::Organic},{0,55,"AKWF_Voice_Synth",WaveCategory::Organic},
    {0,56,"AKWF_Whistle",WaveCategory::Organic},{0,57,"AKWF_Beatbox",WaveCategory::Organic},
    {0,58,"AKWF_AcGuitar",WaveCategory::Organic},{0,59,"AKWF_Violin",WaveCategory::Organic},
    {0,60,"AKWF_Cello",WaveCategory::Organic},{0,61,"AKWF_Flute",WaveCategory::Organic},
    {0,62,"AKWF_Clarinet",WaveCategory::Organic},{0,63,"AKWF_Oboe",WaveCategory::Organic},
    // Brass/Wind (64-79): file 0
    {0,64,"AKWF_Brass_1",WaveCategory::BrassWind},{0,65,"AKWF_Brass_2",WaveCategory::BrassWind},
    {0,66,"AKWF_Brass_Ens",WaveCategory::BrassWind},{0,67,"AKWF_Trumpet",WaveCategory::BrassWind},
    {0,68,"AKWF_Trombone",WaveCategory::BrassWind},{0,69,"AKWF_Sax_Alto",WaveCategory::BrassWind},
    {0,70,"AKWF_Sax_Tenor",WaveCategory::BrassWind},{0,71,"AKWF_Horn",WaveCategory::BrassWind},
    {0,72,"AKWF_Tuba",WaveCategory::BrassWind},{0,73,"AKWF_Brass_Syn",WaveCategory::BrassWind},
    {0,74,"AKWF_Brass_Fall",WaveCategory::BrassWind},{0,75,"AKWF_Brass_Stab",WaveCategory::BrassWind},
    {0,76,"AKWF_Harmonica",WaveCategory::BrassWind},{0,77,"AKWF_Accordion",WaveCategory::BrassWind},
    {0,78,"AKWF_Bagpipe",WaveCategory::BrassWind},{0,79,"AKWF_Didgeridoo",WaveCategory::BrassWind},
    // Keys/Mallets (80-95)
    {0,80,"AKWF_Piano_1",WaveCategory::KeysMallets},{0,81,"AKWF_Piano_2",WaveCategory::KeysMallets},
    {0,82,"AKWF_EPiano_1",WaveCategory::KeysMallets},{0,83,"AKWF_EPiano_2",WaveCategory::KeysMallets},
    {0,84,"AKWF_Clav",WaveCategory::KeysMallets},{0,85,"AKWF_Organ_1",WaveCategory::KeysMallets},
    {0,86,"AKWF_Organ_2",WaveCategory::KeysMallets},{0,87,"AKWF_Organ_TW",WaveCategory::KeysMallets},
    {0,88,"AKWF_Bell_1",WaveCategory::KeysMallets},{0,89,"AKWF_Bell_2",WaveCategory::KeysMallets},
    {0,90,"AKWF_Marimba",WaveCategory::KeysMallets},{0,91,"AKWF_Xylophone",WaveCategory::KeysMallets},
    {0,92,"AKWF_Vibes",WaveCategory::KeysMallets},{0,93,"AKWF_Glock",WaveCategory::KeysMallets},
    {0,94,"AKWF_MusicBox",WaveCategory::KeysMallets},{0,95,"AKWF_Kalimba",WaveCategory::KeysMallets},
    // FX/Metal (96-111)
    {0,96,"AKWF_Metal_Hit",WaveCategory::FxMetal},{0,97,"AKWF_Metal_Clang",WaveCategory::FxMetal},
    {0,98,"AKWF_Metal_Sheet",WaveCategory::FxMetal},{0,99,"AKWF_Metal_Pipe",WaveCategory::FxMetal},
    {1,0,"AKWF_Laser",WaveCategory::FxMetal},{1,1,"AKWF_Zap",WaveCategory::FxMetal},
    {1,2,"AKWF_Riser",WaveCategory::FxMetal},{1,3,"AKWF_Downer",WaveCategory::FxMetal},
    {1,4,"AKWF_Noise_W",WaveCategory::FxMetal},{1,5,"AKWF_Noise_P",WaveCategory::FxMetal},
    {1,6,"AKWF_Static",WaveCategory::FxMetal},{1,7,"AKWF_Buzz",WaveCategory::FxMetal},
    {1,8,"AKWF_Glitch_1",WaveCategory::FxMetal},{1,9,"AKWF_Glitch_2",WaveCategory::FxMetal},
    {1,10,"AKWF_Scratch",WaveCategory::FxMetal},{1,11,"AKWF_Siren",WaveCategory::FxMetal},
    // Chip/8-Bit (112-127)
    {5,0,"NES_Square_1",WaveCategory::Chip8Bit},{5,1,"NES_Square_2",WaveCategory::Chip8Bit},
    {5,2,"NES_Triangle",WaveCategory::Chip8Bit},{5,3,"NES_Noise_Short",WaveCategory::Chip8Bit},
    {5,4,"NES_Noise_Long",WaveCategory::Chip8Bit},{5,5,"NES_DPCM_1",WaveCategory::Chip8Bit},
    {5,6,"NES_DPCM_2",WaveCategory::Chip8Bit},{5,7,"NES_DPCM_3",WaveCategory::Chip8Bit},
    {5,8,"NES_DPCM_4",WaveCategory::Chip8Bit},{5,9,"NES_DPCM_5",WaveCategory::Chip8Bit},
    {5,10,"NES_DPCM_6",WaveCategory::Chip8Bit},{5,11,"NES_DPCM_7",WaveCategory::Chip8Bit},
    {5,12,"NES_DPCM_8",WaveCategory::Chip8Bit},{5,13,"NES_DPCM_9",WaveCategory::Chip8Bit},
    {5,14,"NES_DPCM_10",WaveCategory::Chip8Bit},{5,15,"NES_DPCM_11",WaveCategory::Chip8Bit},
};

// === AKWF extras (Slots 256-383): curated waves from files 0-4 ===
static const AKWFEntry kAKWFExtras[] = {
    // More basic shapes & synth
    {1,20,"AKWF_Saw_Bright",WaveCategory::Basic},{1,21,"AKWF_Saw_Dark",WaveCategory::Basic},
    {1,22,"AKWF_Square_Fat",WaveCategory::Basic},{1,23,"AKWF_Pulse_Tight",WaveCategory::Basic},
    {1,24,"AKWF_Tri_Fold",WaveCategory::Basic},{1,25,"AKWF_Sine_Warm",WaveCategory::Basic},
    {1,26,"AKWF_Reso_Sweep",WaveCategory::Basic},{1,27,"AKWF_Formant_1",WaveCategory::Basic},
    // More bass/lead
    {1,28,"AKWF_Bass_3",WaveCategory::BassLead},{1,29,"AKWF_Bass_Grit",WaveCategory::BassLead},
    {1,30,"AKWF_Lead_3",WaveCategory::BassLead},{1,31,"AKWF_Lead_Aggro",WaveCategory::BassLead},
    {1,32,"AKWF_Arp_1",WaveCategory::BassLead},{1,33,"AKWF_Arp_2",WaveCategory::BassLead},
    {1,34,"AKWF_Pluck_1",WaveCategory::BassLead},{1,35,"AKWF_Pluck_2",WaveCategory::BassLead},
    // More pads
    {1,36,"AKWF_Pad_4",WaveCategory::PadAmbient},{1,37,"AKWF_Pad_Vox",WaveCategory::PadAmbient},
    {1,38,"AKWF_Swell_1",WaveCategory::PadAmbient},{1,39,"AKWF_Swell_2",WaveCategory::PadAmbient},
    {1,40,"AKWF_Drone_1",WaveCategory::PadAmbient},{1,41,"AKWF_Drone_2",WaveCategory::PadAmbient},
    {1,42,"AKWF_Texture_1",WaveCategory::PadAmbient},{1,43,"AKWF_Texture_2",WaveCategory::PadAmbient},
    // More organic
    {1,44,"AKWF_Voice_Robot",WaveCategory::Organic},{1,45,"AKWF_Voice_Alien",WaveCategory::Organic},
    {1,46,"AKWF_Choir_Synth",WaveCategory::Organic},{1,47,"AKWF_Strings_Warm",WaveCategory::Organic},
    {1,48,"AKWF_Cello_Synth",WaveCategory::Organic},{1,49,"AKWF_Violin_Syn",WaveCategory::Organic},
    {1,50,"AKWF_Flute_Pan",WaveCategory::Organic},{1,51,"AKWF_Guitar_Nyl",WaveCategory::Organic},
    // More brass/wind
    {1,52,"AKWF_Brass_3",WaveCategory::BrassWind},{1,53,"AKWF_Brass_Soft",WaveCategory::BrassWind},
    {1,54,"AKWF_Horn_Ens",WaveCategory::BrassWind},{1,55,"AKWF_Trumpet_Mute",WaveCategory::BrassWind},
    {1,56,"AKWF_Sax_Sop",WaveCategory::BrassWind},{1,57,"AKWF_Sax_Bari",WaveCategory::BrassWind},
    {1,58,"AKWF_Clarinet_2",WaveCategory::BrassWind},{1,59,"AKWF_Flute_2",WaveCategory::BrassWind},
    // More keys/mallets
    {1,60,"AKWF_Piano_Soft",WaveCategory::KeysMallets},{1,61,"AKWF_EPiano_Warm",WaveCategory::KeysMallets},
    {1,62,"AKWF_Toy_Piano",WaveCategory::KeysMallets},{1,63,"AKWF_Harpsichord",WaveCategory::KeysMallets},
    {1,64,"AKWF_Steel_Drum",WaveCategory::KeysMallets},{1,65,"AKWF_Tubular",WaveCategory::KeysMallets},
    {1,66,"AKWF_Bell_3",WaveCategory::KeysMallets},{1,67,"AKWF_Celesta",WaveCategory::KeysMallets},
    // More FX
    {1,68,"AKWF_LFO_Sweep",WaveCategory::FxMetal},{1,69,"AKWF_Metal_Spring",WaveCategory::FxMetal},
    {2,0,"AKWF_FX_Drop",WaveCategory::FxMetal},{2,1,"AKWF_FX_Whoosh",WaveCategory::FxMetal},
    {2,2,"AKWF_FX_Impact",WaveCategory::FxMetal},{2,3,"AKWF_FX_Explosion",WaveCategory::FxMetal},
    {2,4,"AKWF_FX_Wind",WaveCategory::FxMetal},{2,5,"AKWF_FX_Thunder",WaveCategory::FxMetal},
    {2,6,"AKWF_FX_Robot",WaveCategory::FxMetal},{2,7,"AKWF_FX_Alien",WaveCategory::FxMetal},
    // Chip extras
    {2,8,"AKWF_Atari_Bass",WaveCategory::Chip8Bit},{2,9,"AKWF_Atari_Lead",WaveCategory::Chip8Bit},
    {2,10,"AKWF_Sega_FM",WaveCategory::Chip8Bit},{2,11,"AKWF_PC_Speaker",WaveCategory::Chip8Bit},
    {2,12,"AKWF_C64_Pulse",WaveCategory::Chip8Bit},{2,13,"AKWF_C64_Saw",WaveCategory::Chip8Bit},
    {2,14,"AKWF_Amiga_Paula",WaveCategory::Chip8Bit},{2,15,"AKWF_ST_Yamaha",WaveCategory::Chip8Bit},
    // Synth specials
    {2,16,"AKWF_Moog_Saw",WaveCategory::BassLead},{2,17,"AKWF_Moog_Square",WaveCategory::BassLead},
    {2,18,"AKWF_Oberheim_Saw",WaveCategory::BassLead},{2,19,"AKWF_Roland_Saw",WaveCategory::BassLead},
    {2,20,"AKWF_Yamaha_Saw",WaveCategory::BassLead},{2,21,"AKWF_Korg_Saw",WaveCategory::BassLead},
    {2,22,"AKWF_CS80_Saw",WaveCategory::BassLead},{2,23,"AKWF_Juno_Saw",WaveCategory::BassLead},
    // World/ethnic
    {2,24,"AKWF_Sitar",WaveCategory::KeysMallets},{2,25,"AKWF_Banjo",WaveCategory::KeysMallets},
    {2,26,"AKWF_Mandolin",WaveCategory::KeysMallets},{2,27,"AKWF_Balinese",WaveCategory::KeysMallets},
    {2,28,"AKWF_Harp",WaveCategory::KeysMallets},{2,29,"AKWF_Koto",WaveCategory::KeysMallets},
    {2,30,"AKWF_Shamisen",WaveCategory::KeysMallets},{2,31,"AKWF_Dulcimer",WaveCategory::KeysMallets},
    // Percussive
    {2,32,"AKWF_Kick_808",WaveCategory::FxMetal},{2,33,"AKWF_Snare_808",WaveCategory::FxMetal},
    {2,34,"AKWF_Hat_Closed",WaveCategory::FxMetal},{2,35,"AKWF_Cymbal",WaveCategory::FxMetal},
    {2,36,"AKWF_Tom_Low",WaveCategory::FxMetal},{2,37,"AKWF_Tom_High",WaveCategory::FxMetal},
    {2,38,"AKWF_Clap_808",WaveCategory::FxMetal},{2,39,"AKWF_Rimshot",WaveCategory::FxMetal},
    // Vocal / choir extras
    {2,40,"AKWF_Choir_Male",WaveCategory::Organic},{2,41,"AKWF_Choir_Female",WaveCategory::Organic},
    {2,42,"AKWF_Ooh",WaveCategory::Organic},{2,43,"AKWF_Aah",WaveCategory::Organic},
    {2,44,"AKWF_Talk_Box",WaveCategory::Organic},{2,45,"AKWF_Vocoder_1",WaveCategory::Organic},
    {2,46,"AKWF_Vocoder_2",WaveCategory::Organic},{2,47,"AKWF_Daft_Voice",WaveCategory::Organic},
    // More strings
    {2,48,"AKWF_Strings_Ens",WaveCategory::PadAmbient},{2,49,"AKWF_Strings_Solo",WaveCategory::PadAmbient},
    {2,50,"AKWF_Strings_Pizz",WaveCategory::PadAmbient},{2,51,"AKWF_Strings_Trem",WaveCategory::PadAmbient},
    {2,52,"AKWF_Orch_Hit",WaveCategory::PadAmbient},{2,53,"AKWF_Orch_Swell",WaveCategory::PadAmbient},
    {2,54,"AKWF_Brass_Swell",WaveCategory::BrassWind},{2,55,"AKWF_Brass_Punch",WaveCategory::BrassWind},
    // Hybrid
    {2,56,"AKWF_Hybrid_1",WaveCategory::Basic},{2,57,"AKWF_Hybrid_2",WaveCategory::Basic},
    {2,58,"AKWF_Hybrid_3",WaveCategory::Basic},{2,59,"AKWF_Hybrid_4",WaveCategory::Basic},
    {2,60,"AKWF_Wave_Seq_1",WaveCategory::PadAmbient},{2,61,"AKWF_Wave_Seq_2",WaveCategory::PadAmbient},
    {3,0,"AKWF_Bowed_1",WaveCategory::Organic},{3,1,"AKWF_Bowed_2",WaveCategory::Organic},
    {3,2,"AKWF_Bowed_3",WaveCategory::Organic},{3,3,"AKWF_Bowed_4",WaveCategory::Organic},
    {3,4,"AKWF_Reed_1",WaveCategory::BrassWind},{3,5,"AKWF_Reed_2",WaveCategory::BrassWind},
    {3,6,"AKWF_Reed_3",WaveCategory::BrassWind},{3,7,"AKWF_Reed_4",WaveCategory::BrassWind},
    {3,8,"AKWF_Ethnic_1",WaveCategory::KeysMallets},{3,9,"AKWF_Ethnic_2",WaveCategory::KeysMallets},
    {3,10,"AKWF_Ethnic_3",WaveCategory::KeysMallets},{3,11,"AKWF_Ethnic_4",WaveCategory::KeysMallets},
    // Synthetic Harmonics & Extended Shapes (106-127): file 4 (0401-0500)
    {4,0,"AKWF_Harmonic_1",WaveCategory::Basic},{4,1,"AKWF_Harmonic_2",WaveCategory::Basic},
    {4,2,"AKWF_Harmonic_3",WaveCategory::Basic},{4,3,"AKWF_Harmonic_4",WaveCategory::Basic},
    {4,4,"AKWF_Harmonic_5",WaveCategory::Basic},{4,5,"AKWF_Harmonic_6",WaveCategory::Basic},
    {4,6,"AKWF_Harmonic_7",WaveCategory::Basic},{4,7,"AKWF_Harmonic_8",WaveCategory::Basic},
    {4,8,"AKWF_Spectra_1",WaveCategory::Basic},{4,9,"AKWF_Spectra_2",WaveCategory::Basic},
    {4,10,"AKWF_Spectra_3",WaveCategory::Basic},{4,11,"AKWF_Spectra_4",WaveCategory::Basic},
    {4,12,"AKWF_Bell_Harms",WaveCategory::KeysMallets},{4,13,"AKWF_Chime_Harms",WaveCategory::KeysMallets},
    {4,14,"AKWF_Tine_Harms",WaveCategory::KeysMallets},{4,15,"AKWF_Gong_Harms",WaveCategory::KeysMallets},
    {4,16,"AKWF_Warm_Sub",WaveCategory::BassLead},{4,17,"AKWF_Deep_Sub",WaveCategory::BassLead},
    {4,18,"AKWF_Analog_Warm",WaveCategory::BassLead},{4,19,"AKWF_Analog_Punch",WaveCategory::BassLead},
    {4,20,"AKWF_Digital_Air",WaveCategory::PadAmbient},{4,21,"AKWF_Digital_Lush",WaveCategory::PadAmbient}
};

// ═══════════════════════════════════════════════════════════════
// Procedural fallback
// ═══════════════════════════════════════════════════════════════

void DWGSTables::generateStandardProcedural(size_t slot) noexcept
{
    float* t = &wavetableMemory_[slot * SAMPLES_PER_TABLE];
    float h[16]{}; h[0]=1.0f;

    if (slot<8) { float d=1.0f+slot*0.25f; for(int i=1;i<16;++i)h[i]=std::pow(1.0f/(i+1),d); }
    else if (slot<12) { h[1]=0.7f;h[3]=0.4f;h[5]=0.3f; }
    else if (slot<20) { h[1]=0.8f;h[3]=0.6f;h[6]=0.5f;h[10]=0.35f;h[13]=0.2f; }
    else if (slot<29) { int db=static_cast<int>(slot-21);h[0]=1.0f;h[1]=(db&1)?0.8f:0.0f;
        h[2]=(db&2)?0.7f:0.2f;h[3]=(db&4)?0.6f:0.0f;h[5]=(db&8)?0.5f:0.3f;h[7]=0.4f; }
    else if (slot<40) { for(int i=1;i<16;++i)h[i]=1.0f/(i+1)*std::sin(i*0.8f); }
    else if (slot<48) { h[0]=1.0f;h[1]=0.9f;h[2]=0.5f;h[3]=0.2f;h[4]=0.1f; }
    else if (slot<56) { for(int i=1;i<14;++i)h[i]=0.6f*std::exp(-std::pow((i-4.0f),2.0f)/8.0f); }
    else { h[0]=0.3f;h[4]=0.9f;h[7]=0.7f;h[11]=0.5f;h[15]=0.3f; }

    float mx=0.0001f;
    for(size_t s=0;s<SAMPLES_PER_TABLE;++s){double p=(double)s/SAMPLES_PER_TABLE*DSPUtils::TWO_PI;double a=0.0;
        for(int i=0;i<16;++i)if(std::abs(h[i])>0.0001f)a+=h[i]*std::sin((i+1)*p);
        t[s]=(float)a;mx=std::max(mx,std::abs(t[s]));}
    float im=1.0f/mx;for(size_t s=0;s<SAMPLES_PER_TABLE;++s)t[s]*=im;
}

// ═══════════════════════════════════════════════════════════════
// Load one .m1 file and extract curated waves from it
// ═══════════════════════════════════════════════════════════════

struct M1FileEntry { const char* resName; int numWaves; };
static const M1FileEntry kM1Files[] = {
    {"AKWF_0001_0100_m1",100},{"AKWF_0101_0200_m1",100},{"AKWF_0201_0300_m1",100},
    {"AKWF_0301_0400_m1",100},{"AKWF_0401_0500_m1",100},{"AKWF_nes_m1",64},
    {"ProphetVS_Wave33_56_m1",24},{"ProphetVS_Wave57_80_m1",24},
    {"ProphetVS_Wave81_104_m1",24},{"ProphetVS_Wave105_128_m1",24}
};

static bool loadM1File(const char* resName, int numWaves, std::vector<std::vector<float>>& out)
{
    int sz=0;const char* d=DWGSBinary::getNamedResource(resName,sz);
    if(!d||sz<1000)return false;
    return WavetableLoader::loadM1Bank(reinterpret_cast<const uint8_t*>(d),(size_t)sz,(size_t)numWaves,out);
}

// ═══════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════

void DWGSTables::initTables() noexcept
{
    if(isInitialized_)return;
    wavetableMemory_.assign(TOTAL_MAX_SAMPLES,0.0f);
    waveNames_.assign(MAX_EXPANDED_TABLES,"");
    waveCategories_.assign(MAX_EXPANDED_TABLES,WaveCategory::User);

    // ── Slots 0-63: Korg DWGS ──
    for(size_t t=0;t<NUM_STANDARD_TABLES;++t){
        waveNames_[t]=(t < 64 && kDWGSStandardNames[t] != nullptr) ? kDWGSStandardNames[t] : ("DWGS_" + std::to_string(t + 1));
        waveCategories_[t]=getDWGSStandardCategory(t);
    }
    int ds=0;const char* dd=DWGSBinary::getNamedResource("DW_8000_All_m1",ds);
    if(dd&&ds>1000){std::vector<std::vector<float>> w;if(WavetableLoader::loadM1Bank(reinterpret_cast<const uint8_t*>(dd),(size_t)ds,32,w))
        for(size_t i=0;i<std::min(w.size(),NUM_STANDARD_TABLES);++i)WavetableLoader::resample(w[i],&wavetableMemory_[i*SAMPLES_PER_TABLE]);}
    else for(size_t t=0;t<NUM_STANDARD_TABLES;++t)generateStandardProcedural(t);

    // ── Slots 64-159: Prophet VS (96 real waves) ──
    for(size_t t=64;t<64+96;++t){
        size_t idx = t - 64;
        waveNames_[t]=(idx < 96 && kProphetVSNames[idx] != nullptr) ? kProphetVSNames[idx] : ("PVS_" + std::to_string(idx + 1));
    }
    // Load Prophet VS .m1 files: 4 files x 24 waves = 96 total at slot 64
    const char* pvsFiles[]={"ProphetVS_Wave33_56_m1","ProphetVS_Wave57_80_m1","ProphetVS_Wave81_104_m1","ProphetVS_Wave105_128_m1"};
    size_t pvsSlot=64;
    for(int f=0;f<4;++f){
        std::vector<std::vector<float>> w;
        if(loadM1File(pvsFiles[f],24,w)){
            for(size_t i=0;i<w.size();++i){WavetableLoader::resample(w[i],&wavetableMemory_[pvsSlot*SAMPLES_PER_TABLE]);waveCategories_[pvsSlot]=WaveCategory::Basic;if(++pvsSlot>=160)break;}
        }else{for(size_t i=0;i<24&&pvsSlot<160;++i){generateStandardProcedural(pvsSlot%64);pvsSlot++;}}
    }

    // ── AKWF curated (160-287) + AKWF extras (288-383) ──
    auto loadAKWFSet=[&](const AKWFEntry*list,size_t count,size_t baseSlot){
        std::vector<std::vector<float>> files[6];
        for(int fi=0;fi<6;++fi){int nw=(fi<5)?100:64;if(!loadM1File(kM1Files[fi].resName,nw,files[fi]))files[fi].clear();}
        for(size_t i=0;i<count;++i){
            const auto&e=list[i];size_t slot=baseSlot+i;
            if (slot >= MAX_EXPANDED_TABLES) break;
            const char* entryName = (e.name != nullptr) ? e.name : "AKWF_Wave";
            waveNames_[slot]=entryName;waveCategories_[slot]=e.cat;
            if(e.fileIdx<6&&!files[e.fileIdx].empty()&&e.waveInFile<files[e.fileIdx].size())
                WavetableLoader::resample(files[e.fileIdx][e.waveInFile],&wavetableMemory_[slot*SAMPLES_PER_TABLE]);
            else{memset(&wavetableMemory_[slot*SAMPLES_PER_TABLE],0,SAMPLES_PER_TABLE*sizeof(float));}}
    };
    loadAKWFSet(kAKWF,std::size(kAKWF),160);
    loadAKWFSet(kAKWFExtras,std::size(kAKWFExtras),288);

    // ── Slots 384-511: User bank (empty for drag & drop) ──
    for(size_t t=384;t<MAX_EXPANDED_TABLES;++t){waveNames_[t]="User_"+std::to_string(t-383);waveCategories_[t]=WaveCategory::User;}

    // Build catalog
    catalog_.clear();catalog_.reserve(MAX_EXPANDED_TABLES);
    for(size_t t=0;t<MAX_EXPANDED_TABLES;++t)catalog_.push_back({waveNames_[t],waveCategories_[t],t});
    isInitialized_=true;
}

// ═══════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════

const float* DWGSTables::getTableData()noexcept{if(!isInitialized_)initTables();return wavetableMemory_.data();}
const char* DWGSTables::getWaveName(size_t i)noexcept{if(!isInitialized_)initTables();return(i<waveNames_.size()&&!waveNames_[i].empty())?waveNames_[i].c_str():"Unknown";}
WaveCategory DWGSTables::getWaveCategory(size_t i)noexcept{if(!isInitialized_)initTables();return(i<waveCategories_.size())?waveCategories_[i]:WaveCategory::User;}
size_t DWGSTables::getTotalWaveforms()noexcept{return MAX_EXPANDED_TABLES;}
const std::vector<WaveEntry>& DWGSTables::getCatalog()noexcept{if(!isInitialized_)initTables();return catalog_;}

std::vector<WaveEntry> DWGSTables::search(const std::string& q)noexcept{
    if(!isInitialized_)initTables();std::vector<WaveEntry> r;if(q.empty())return r;
    std::string lo=q;for(auto&c:lo)c=(char)tolower((unsigned char)c);
    for(auto&e:catalog_){std::string wl=e.name;for(auto&c:wl)c=(char)tolower((unsigned char)c);if(wl.find(lo)!=std::string::npos)r.push_back(e);}
    return r;
}

std::vector<WaveEntry> DWGSTables::filterByCategory(WaveCategory cat)noexcept{
    if(!isInitialized_)initTables();std::vector<WaveEntry> r;
    for(auto&e:catalog_)if(e.category==cat)r.push_back(e);
    return r;
}

bool DWGSTables::registerWaveform(size_t idx,const std::string& name,WaveCategory cat,const float*s,size_t n)noexcept{
    if(idx>=MAX_EXPANDED_TABLES||!s||!n)return false;if(!isInitialized_)initTables();
    waveNames_[idx]=name;waveCategories_[idx]=cat;catalog_[idx]={name,cat,idx};
    float*d=&wavetableMemory_[idx*SAMPLES_PER_TABLE];float mx=0.0001f;
    for(size_t i=0;i<SAMPLES_PER_TABLE;++i){double p=(double)i/SAMPLES_PER_TABLE*n;size_t i0=(size_t)p%n,i1=(i0+1)%n;
        float fr=(float)(p-(double)i0);d[i]=s[i0]+fr*(s[i1]-s[i0]);mx=std::max(mx,std::abs(d[i]));}
    float im=1.0f/mx;for(size_t i=0;i<SAMPLES_PER_TABLE;++i)d[i]*=im;
    return true;
}

bool DWGSTables::loadWavToSlot(size_t slot,const std::string& name,WaveCategory cat,const uint8_t* d,size_t s)noexcept{
    if(slot>=MAX_EXPANDED_TABLES||!d||!s)return false;if(!isInitialized_)initTables();
    std::vector<float> raw;if(!WavetableLoader::loadWav(d,s,raw))return false;
    waveNames_[slot]=name;waveCategories_[slot]=cat;catalog_[slot]={name,cat,slot};
    WavetableLoader::resample(raw,&wavetableMemory_[slot*SAMPLES_PER_TABLE]);
    return true;
}

bool DWGSTables::loadM1Bank(const uint8_t*d,size_t sz,size_t start,size_t nw,WaveCategory cat)noexcept{
    if(start>=MAX_EXPANDED_TABLES)return false;if(!isInitialized_)initTables();
    std::vector<std::vector<float>> w;if(!WavetableLoader::loadM1Bank(d,sz,nw,w))return false;
    size_t mx=std::min(w.size(),MAX_EXPANDED_TABLES-start);
    for(size_t i=0;i<mx;++i){WavetableLoader::resample(w[i],&wavetableMemory_[(start+i)*SAMPLES_PER_TABLE]);}
    return mx>0;
}

} // namespace ABDMS2000