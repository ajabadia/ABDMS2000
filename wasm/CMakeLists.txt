cmake_minimum_required(VERSION 3.22)
project(ABDMS2000_WASM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# ABDShared (WASM) — DspCore, SynthCore
# ============================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../ABDShared/Code/CMakeLists.txt")
    set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ABDShared/Code")
    message(STATUS "ABDShared (WASM): Using canonical Code at ${ABDSHARED_CODE_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../ABDSharedCode/CMakeLists.txt")
    set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ABDSharedCode")
    message(STATUS "ABDShared (WASM): Using legacy sibling at ${ABDSHARED_CODE_DIR}")
endif()

if(DEFINED ABDSHARED_CODE_DIR)
    # Disable modules that depend on JUCE (unavailable under Emscripten)
    set(ABDSHAREDCODE_BUILD_SYNTHCORE_TESTS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_AUTOUPDATER OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREDRIVERS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREMIDIDETECT OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_SYNTHCORE_TESTS OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_AUTOUPDATER OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_HARDWAREDRIVERS OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_HARDWAREMIDIDETECT OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ABDSHARED_CODE_DIR}" "${CMAKE_BINARY_DIR}/ABDShared_build")
else()
    include(FetchContent)
    FetchContent_Declare(
      ABDShared
      GIT_REPOSITORY https://github.com/ajabadia/ABDShared.git
      GIT_TAG        main
    )
    set(ABDSHAREDCODE_BUILD_SYNTHCORE_TESTS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_AUTOUPDATER OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREDRIVERS OFF CACHE BOOL "" FORCE)
    set(ABDSHAREDCODE_BUILD_HARDWAREMIDIDETECT OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_SYNTHCORE_TESTS OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_AUTOUPDATER OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_HARDWAREDRIVERS OFF CACHE BOOL "" FORCE)
    set(ABDSHARED_BUILD_HARDWAREMIDIDETECT OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ABDShared)
endif()

# Source Files
# SynthCore (PolyBLEP, ADSREnvelope, LFO, PortamentoGlide) is consumed via
# ABDShared::SynthCore target — no more fragile ../../ABDSharedCode paths.
# SysExCodec is compiled directly because ABDShared::HardwareDrivers depends
# on juce_audio_basics which is not available in the Emscripten WASM build.
#
# Lista DSP compartida con el raiz (single-source, anti-drift): vive en
# DspSources.cmake. La diferencia WASM (SynthEngine/MIDITelemetryManager,
# dependientes de juce_audio_processors) esta documentada alli — NO drift.
include("${CMAKE_CURRENT_LIST_DIR}/../DspSources.cmake")

set(DSP_SOURCES
    ../Source/Wasm/WasmBridge.cpp
    ${MS2000_DSP_SOURCES_WASM}
    # SysExCodec compiled directly (HardwareDrivers has JUCE dep)
    ${ABDSHARED_CODE_DIR}/HardwareDrivers/SysExCodec.cpp
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
    SUFFIX ""
    LINK_FLAGS "-s MALLOC=emmalloc -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=16777216 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','HEAPF32'] -s EXPORTED_FUNCTIONS=['_initEngine','_processAudio','_noteOn','_noteOff','_allNotesOff','_setParamNormalized','_setParamById','_getAudioSnapshot','_loadProgram','_initPatch','_randomizePatch','_malloc','_free'] -O2"
)
