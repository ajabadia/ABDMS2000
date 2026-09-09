cmake_minimum_required(VERSION 3.22)
project(ABDMS2000_WASM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- ABDSharedCode Integration ---
# Resolve the shared DSP primitives (PolyBLEP, ADSREnvelope, LFO, PortamentoGlide)
# from ABDSharedCode instead of compiling them via fragile relative paths.
# Same dual-path strategy as the root CMakeLists.txt:
#  * local ../ABDSharedCode if it exists (development)
#  * FetchContent from GitHub as fallback (CI/CD without local copy)
set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ABDSharedCode")
if(EXISTS "${ABDSHARED_CODE_DIR}/CMakeLists.txt")
    message(STATUS "ABDSharedCode (WASM): Using local at ${ABDSHARED_CODE_DIR}")
    # Disable modules that depend on JUCE (unavailable under Emscripten)
    set(ABDSHAREDCODE_BUILD_SYNTHCORE_TESTS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_AUTOUPDATER OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREDRIVERS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREMIDIDETECT OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ABDSHARED_CODE_DIR}" "${CMAKE_BINARY_DIR}/ABDSharedCode")
else()
    include(FetchContent)
    FetchContent_Declare(
      ABDSharedCode
      GIT_REPOSITORY https://github.com/ajabadia/ABDSharedCode.git
      GIT_TAG        master
    )
    set(ABDSHAREDCODE_BUILD_SYNTHCORE_TESTS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_AUTOUPDATER OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREDRIVERS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREMIDIDETECT OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ABDSharedCode)
endif()

# Source Files
# SynthCore (PolyBLEP, ADSREnvelope, LFO, PortamentoGlide) is consumed via
# ABDShared::SynthCore target — no more fragile ../../ABDSharedCode paths.
# SysExCodec is compiled directly because ABDShared::HardwareDrivers depends
# on juce_audio_basics which is not available in the Emscripten WASM build.
set(DSP_SOURCES
    ../Source/Wasm/WasmBridge.cpp
    ../Source/Core/VoiceManager.cpp
    ../Source/Core/Voice.cpp
    ../Source/DSP/Oscillators/VAOscillator.cpp
    ../Source/DSP/Oscillators/DWGSOscillator.cpp
    ../Source/DSP/Oscillators/DWGSTables.cpp
    ../Source/DSP/Oscillators/VoxWaveOscillator.cpp
    ../Source/DSP/Oscillators/NoiseGenerator.cpp
    ../Source/DSP/Filters/MultiModeFilter.cpp
    ../Source/DSP/Filters/FilterResonanceComp.cpp
    ../Source/DSP/Modulation/VirtualPatchMatrix.cpp
    ../Source/DSP/Effects/ModFX.cpp
    ../Source/DSP/Effects/DelayFX.cpp
    ../Source/DSP/Effects/Equalizer.cpp
    ../Source/DSP/Vocoder/Vocoder16Band.cpp
    ../Source/DSP/Sequencer/Arpeggiator.cpp
    ../Source/DSP/Sequencer/ModSequencer.cpp
    # SysExCodec compiled directly (HardwareDrivers has JUCE dep)
    ${ABDSHARED_CODE_DIR}/HardwareDrivers/SysExCodec.cpp
    ../Source/MIDI/MIDIMap.cpp
    ../Source/State/ParameterRegistry.gen.cpp
)

add_executable(ms2000_dsp ${DSP_SOURCES})

# Link ABDShared::SynthCore (propagates include dirs + sources automatically)
target_link_libraries(ms2000_dsp PRIVATE ABDShared::SynthCore)

target_include_directories(ms2000_dsp PRIVATE
    ../Source
    ${ABDSHARED_CODE_DIR}
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
    LINK_FLAGS "-s SINGLE_FILE=1 -s MALLOC=emmalloc -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue'] -s EXPORTED_FUNCTIONS=['_initEngine','_processAudio','_noteOn','_noteOff','_allNotesOff','_setParamNormalized','_setParamById','_getAudioSnapshot','_loadProgram','_initPatch','_randomizePatch','_malloc','_free'] -O3"
)
