cmake_minimum_required(VERSION 3.22)
project(ABDMS2000_WASM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source Files
set(DSP_SOURCES
    ../Source/Wasm/WasmBridge.cpp
    ../Source/Core/VoiceManager.cpp
    ../Source/Core/Voice.cpp
    ../Source/DSP/Common/PolyBLEP.cpp
    ../Source/DSP/Oscillators/VAOscillator.cpp
    ../Source/DSP/Oscillators/DWGSOscillator.cpp
    ../Source/DSP/Oscillators/DWGSTables.cpp
    ../Source/DSP/Oscillators/VoxWaveOscillator.cpp
    ../Source/DSP/Oscillators/NoiseGenerator.cpp
    ../Source/DSP/Filters/MultiModeFilter.cpp
    ../Source/DSP/Filters/FilterResonanceComp.cpp
    ../Source/DSP/Envelopes/ADSREnvelope.cpp
    ../Source/DSP/Modulation/LFO.cpp
    ../Source/DSP/Modulation/PortamentoGlide.cpp
    ../Source/DSP/Modulation/VirtualPatchMatrix.cpp
    ../Source/DSP/Effects/ModFX.cpp
    ../Source/DSP/Effects/DelayFX.cpp
    ../Source/DSP/Effects/Equalizer.cpp
    ../Source/DSP/Vocoder/Vocoder16Band.cpp
    ../Source/DSP/Sequencer/Arpeggiator.cpp
    ../Source/DSP/Sequencer/ModSequencer.cpp
    ../Source/MIDI/SysExCodec.cpp
    ../Source/MIDI/MIDIMap.cpp
    ../Source/State/ParameterRegistry.gen.cpp
)

add_executable(ms2000_dsp ${DSP_SOURCES})

target_include_directories(ms2000_dsp PRIVATE
    ../Source
    ../Source/Core
    ../Source/DSP
    ../Source/DSP/Common
    ../Source/DSP/Oscillators
    ../Source/DSP/Filters
    ../Source/DSP/Envelopes
    ../Source/DSP/Modulation
    ../Source/DSP/Effects
    ../Source/DSP/Vocoder
    ../Source/DSP/Sequencer
    ../Source/MIDI
    ../Source/State
    ../Source/Wasm
)

target_compile_definitions(ms2000_dsp PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=0
    __EMSCRIPTEN__=1
)

# Emscripten Compilation & Link Flags
set_target_properties(ms2000_dsp PROPERTIES
    OUTPUT_NAME "ms2000_dsp"
    SUFFIX ".js"
    LINK_FLAGS "-s SINGLE_FILE=1 -s MALLOC=emmalloc -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPF32','HEAP32','getValue','setValue'] -s EXPORTED_FUNCTIONS=['_initEngine','_processAudio','_noteOn','_noteOff','_allNotesOff','_setParamNormalized','_setParamById','_getAudioSnapshot','_loadProgram','_initPatch','_randomizePatch'] -O3"
)
