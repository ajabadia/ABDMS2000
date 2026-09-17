cmake_minimum_required(VERSION 3.22)

project(ABDMS2000 VERSION 1.0.0 LANGUAGES C CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(JUCE_USE_WIN_WEBVIEW2 ON CACHE BOOL "Use WebView2 on Windows" FORCE)

# Include JUCE 8
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/GITS/JUCE/CMakeLists.txt")
    add_subdirectory(GITS/JUCE)
elseif(EXISTS "C:/JUCE/CMakeLists.txt")
    add_subdirectory("C:/JUCE" JUCE)
else()
    find_package(JUCE 8 CONFIG REQUIRED)
endif()

# --- ABDBankManager Integration (Direct local subdirectory + WebUI source-of-truth) ---
# C++ core lib + WebUI se resuelven desde el mismo repo ABDBankManager:
#  * local ../ABDBankManager si existe (desarrollo)
#  * FetchContent desde GitHub v0.3.0-lib como fallback (CI/CD sin copia local)
set(ABDBANKMANAGER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDBankManager")
if(EXISTS "${ABDBANKMANAGER_DIR}/cpp/CMakeLists.txt")
    set(ABDBANKMANAGER_WEBUI_DIR "${ABDBANKMANAGER_DIR}/WebUI")
    message(STATUS "ABDBankManager: Using local repo at ${ABDBANKMANAGER_DIR}")
    add_subdirectory("${ABDBANKMANAGER_DIR}/cpp" "${CMAKE_BINARY_DIR}/ABDBankManagerCore")
else()
    # Fallback: FetchContent para CI/CD sin copia local
    include(FetchContent)
    FetchContent_Declare(
      ABDBankManager
      GIT_REPOSITORY https://github.com/ajabadia/ABDBankManager.git
      GIT_TAG        v0.3.0-lib
    )
    FetchContent_MakeAvailable(ABDBankManager)
    set(ABDBANKMANAGER_WEBUI_DIR "${abdbankmanager_SOURCE_DIR}/WebUI")
endif()

# --- ABDSharedCode Integration ---
set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode")
if(EXISTS "${ABDSHARED_CODE_DIR}/CMakeLists.txt")
    message(STATUS "ABDSharedCode: Using local at ${ABDSHARED_CODE_DIR}")
    add_subdirectory("${ABDSHARED_CODE_DIR}" "${CMAKE_BINARY_DIR}/ABDSharedCode")
else()
    include(FetchContent)
    FetchContent_Declare(
      ABDSharedCode
      GIT_REPOSITORY https://github.com/ajabadia/ABDSharedCode.git
      GIT_TAG        master
    )
    FetchContent_MakeAvailable(ABDSharedCode)
endif()

# --- ABDScope Integration (analytical multi-lane scope, native C++ taps) ---
# ABDScope es un proyecto hermano que empaqueta el core C++ header-only (ScopeTap,
# ScopeDataCollector, ScopeFrameSerializer) + componente JUCE WebView2 listo
# (JuceWebScopeComponent) + WebUI embebido como binary data. Se resuelve como
# dependencia local fija (../ABDScope). Targets: ABDScope::ABDScopeCore y
# ABDScope::ABDScopeWebAssets (linkado INTERFACE al core).
set(ABDSCOPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDScope")
if(EXISTS "${ABDSCOPE_DIR}/CMakeLists.txt")
    message(STATUS "ABDScope: Using local at ${ABDSCOPE_DIR}")
    add_subdirectory("${ABDSCOPE_DIR}" "${CMAKE_BINARY_DIR}/ABDScope")
else()
    message(FATAL_ERROR "ABDScope not found at ${ABDSCOPE_DIR}; ABDMS2000 scope integration requires the ABDScope repo.")
endif()

# Create JUCE Plugin
# Lista DSP compartida con el build WASM (single-source, anti-drift):
# DspSources.cmake define MS2000_DSP_SOURCES y MS2000_DSP_SOURCES_WASM.
include("${CMAKE_CURRENT_SOURCE_DIR}/DspSources.cmake")

juce_add_plugin(ABDMS2000
    COMPANY_NAME "ajabadia"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Abad
    PLUGIN_CODE Ms2k
    FORMATS Standalone VST3
    PRODUCT_NAME "ABDMS2000"
    NEEDS_WEBVIEW2 TRUE
)

juce_generate_juce_header(ABDMS2000)

# --- WebUI Binary Assets (native synth UI, bundled dist — Fase 3 DRY) ---
# El WebUI se empaqueta con Vite (Scripts/build_webui.js -> WebUI/dist) ANTES de
# configurar CMake: dist ya trae los bare imports @abdsynths/* resueltos y el
# cascade de tokens compartidos que antes solo existia en dev.
# Los vendor JS (dexie/jszip/file-saver) solo los usa el SPA de abdbank, que se
# sirve desde disco (bankwebui:// / disk fallback), asi que ya no se embeben.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/dist/index.html")
    message(FATAL_ERROR
        "WebUI/dist/index.html not found. Run 'node Scripts/build_webui.js' (vite build) before configuring CMake.")
endif()
file(GLOB_RECURSE WEBUI_ASSETS 
    "WebUI/dist/*"
)
# Paridad con el embed historico: las imagenes del bank manager se sirven via
# bankwebui:// (junction ABDSharedAssets), no van en el binario.
list(FILTER WEBUI_ASSETS EXCLUDE REGEX "WebUI/dist/assets/images")
juce_add_binary_data(WebUIAssets 
    HEADER_NAME "WebUIAssets.h" 
    NAMESPACE "WebUIAssets" 
    SOURCES ${WEBUI_ASSETS}
)
target_link_libraries(ABDMS2000 PUBLIC WebUIAssets)

# --- ABDBankManager WebUI is served from disk ---
# The modal loads /abdbank/* via the disk fallback in pluginResourceProvider.

# Embedded DWGS Wavetable Binaries (Korg DW-8000 + AKWF curated bundles)
juce_add_binary_data(DWGSBinary
    HEADER_NAME "DWGSBinary.h"
    NAMESPACE "DWGSBinary"
    SOURCES
        "DOCS/cycles/DW-8000/DW-8000_All.m1"
        "DOCS/cycles/DW-8000/DW-8000_Int.m1"
        "DOCS/cycles/DW-8000/DW-8000_Exp.m1"
        "DOCS/cycles/AKWF/AKWF_0001-0100.m1"
        "DOCS/cycles/AKWF/AKWF_0101-0200.m1"
        "DOCS/cycles/AKWF/AKWF_0201-0300.m1"
        "DOCS/cycles/AKWF/AKWF_0301-0400.m1"
        "DOCS/cycles/AKWF/AKWF_0401-0500.m1"
        "DOCS/cycles/AKWF/AKWF_nes.m1"
        "DOCS/ProphetVS/ProphetVS_Wave33-56.m1"
        "DOCS/ProphetVS/ProphetVS_Wave57-80.m1"
        "DOCS/ProphetVS/ProphetVS_Wave81-104.m1"
        "DOCS/ProphetVS/ProphetVS_Wave105-128.m1"
)
target_link_libraries(ABDMS2000 PUBLIC DWGSBinary)

# Source Files
# Los .cpp del DSP viven en DspSources.cmake (compartidos con wasm/).
# Aqui solo los headers del DSP y las fuentes de UI/Plugin/MIDI-SysEx.
target_sources(ABDMS2000 PRIVATE
    # Core Engine & Architecture
    Source/Core/SynthEngine.h
    Source/Core/Voice.h
    Source/Core/VoiceManager.h
    Source/Core/AudioThreadSnapshot.h
    Source/Core/HardwareConstants.h
    Source/Core/BuildVersion.h

    ${MS2000_DSP_SOURCES}

    # DSP Common & Math (implementaciones en ABDShared::SynthCore, shims locales)
    Source/DSP/Common/DSPUtils.h
    Source/DSP/Common/PolyBLEP.h

    # DSP Oscillators
    Source/DSP/Oscillators/WavetableLoader.h
    Source/DSP/Oscillators/WaveCategory.h
    Source/DSP/Oscillators/VAOscillator.h
    Source/DSP/Oscillators/VoxWaveOscillator.h
    Source/DSP/Oscillators/DWGSTables.h
    Source/DSP/Oscillators/DWGSOscillator.h
    Source/DSP/Oscillators/NoiseGenerator.h
    Source/DSP/Oscillators/OSC2Modulator.h

    # DSP Filters
    Source/DSP/Filters/MultiModeFilter.h
    Source/DSP/Filters/FilterResonanceComp.h

    # DSP Envelopes & Modulation (implementaciones en ABDShared::SynthCore, shims locales)
    Source/DSP/Envelopes/EnvelopeCurves.h
    Source/DSP/Envelopes/ADSREnvelope.h
    Source/DSP/Modulation/PortamentoGlide.h
    Source/DSP/Modulation/LFO.h
    Source/DSP/Modulation/VirtualPatchMatrix.h

    # DSP Sequencers
    Source/DSP/Sequencer/ModSequencer.h
    Source/DSP/Sequencer/Arpeggiator.h

    # DSP Effects
    Source/DSP/Effects/ModFX.h
    Source/DSP/Effects/DelayFX.h
    Source/DSP/Effects/Equalizer.h

    # DSP Vocoder
    Source/DSP/Vocoder/EnvelopeFollower.h
    Source/DSP/Vocoder/Vocoder16Band.h

    # State & Parameters
    Source/State/ParameterRegistry.gen.h
    Source/State/MS2000PatchBuilder.h
    Source/State/MS2000FactoryBank.h
    Source/State/LCDMenuFormatter.h


    # MIDI Telemetry & SysEx
    # NOTE: SysExCodec.{h,cpp} and NRPNParser.{h,cpp} have been migrated to
    # ABDShared::HardwareDrivers (namespace abd::hw). They are consumed via
    # target_link_libraries below — do NOT add local copies back.
    Source/MIDI/MIDIMap.h
    Source/MIDI/MIDITelemetryManager.h
    Source/MIDI/MS2000ProgramData.h
    Source/MIDI/SysExManager.h
    Source/MIDI/SysExManager.cpp
    Source/MIDI/MS2000SysExExporter.h
    # Tramas Korg del contrato del Bank Manager (Scripts/generate_korg_channel.js).
    # El .h vive en el repo para que un clone + CMake configure funcione.
    Source/MIDI/KorgChannel.gen.h


    # Plugin Wrappers & WebUI Bridge
    Source/Plugin/PluginProcessor.h
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.h
    Source/Plugin/PluginEditor.cpp
    Source/Plugin/PluginEditor_ResourceProvider.h
    Source/Plugin/PluginEditor_ResourceProvider.cpp
    Source/Plugin/ScopeFloatingWindow.h
    Source/Plugin/BridgeActions.h
    Source/Plugin/BridgeActions.cpp
    # Identidad del host generada desde el ModelContract
    # (Scripts/generate_host_model_id.js). El .h vive en el repo como
    # ParameterRegistry.gen.h para que un clone + CMake configure funcione.
    Source/Plugin/HostModelId.gen.h
)

# Target Include Directories
target_include_directories(ABDMS2000 PUBLIC
    Source
    Source/Core
    Source/DSP
    Source/DSP/Common
    Source/DSP/Oscillators
    Source/DSP/Filters
    Source/DSP/Envelopes
    Source/DSP/Modulation
    Source/DSP/Sequencer
    Source/DSP/Effects
    Source/DSP/Vocoder
    Source/Plugin
    Source/State
    Source/MIDI
)

# Link JUCE Modules
target_link_libraries(ABDMS2000 PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    ABDBankManager::ABDBankManagerCore
    ABDShared::AutoUpdater
    ABDShared::SynthCore
    ABDShared::HardwareDrivers
    ABDScope::ABDScopeCore
)

# --- C++ Unit Tests (Standalone executable, no JUCE GUI) ---
add_executable(ABDMS2000_Tests
    Source/Tests/DSPCoreTests.cpp
    # BridgeActions is compiled here (instead of the PluginProcessor) so the
    # WebUI event dispatch is unit-tested directly: the host is injected through
    # the BridgeHost port and the JS messages through a sink, with no WebView
    # and no JUCE GUI. See Source/Plugin/BridgeHost.h.
    Source/Plugin/BridgeActions.cpp
    Source/Core/VoiceManager.cpp
    Source/Core/Voice.cpp
    Source/Core/SynthEngine.cpp
    Source/DSP/Oscillators/VAOscillator.cpp
    Source/DSP/Oscillators/DWGSOscillator.cpp
    Source/DSP/Oscillators/DWGSTables.cpp
    Source/DSP/Oscillators/NoiseGenerator.cpp
    Source/DSP/Oscillators/VoxWaveOscillator.cpp
    Source/DSP/Oscillators/OSC2Modulator.cpp
    Source/DSP/Filters/MultiModeFilter.cpp
    Source/DSP/Filters/FilterResonanceComp.cpp
    Source/DSP/Modulation/VirtualPatchMatrix.cpp
    Source/DSP/Sequencer/ModSequencer.cpp
    Source/DSP/Sequencer/Arpeggiator.cpp
    Source/DSP/Effects/ModFX.cpp
    Source/DSP/Effects/DelayFX.cpp
    Source/DSP/Effects/Equalizer.cpp
    Source/DSP/Vocoder/Vocoder16Band.cpp
    Source/State/ParameterRegistry.gen.cpp
    Source/MIDI/MIDIMap.cpp
    Source/MIDI/MIDITelemetryManager.cpp
    Source/MIDI/SysExManager.cpp
)

target_link_libraries(ABDMS2000_Tests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    DWGSBinary
    WebUIAssets
    ABDShared::SynthCore
    ABDShared::HardwareDrivers
)

# BridgeActions.h/BridgeHost.h live in Source/Plugin for the tests too.
target_include_directories(ABDMS2000_Tests PRIVATE Source/Plugin)

target_include_directories(ABDMS2000_Tests PRIVATE
    Source
    Source/DSP
    Source/DSP/Common
    Source/DSP/Oscillators
    Source/DSP/Envelopes
    Source/DSP/Modulation
    Source/DSP/Filters
    Source/MIDI
    Source/State
    Source/Tests
    "${ABDSCOPE_DIR}/Source"
    "${ABDSCOPE_DIR}/Source/Core"
)
target_compile_features(ABDMS2000_Tests PRIVATE cxx_std_20)

add_test(NAME ABDMS2000_Tests COMMAND ABDMS2000_Tests)

# Pila del binario de tests. `runAllTests()` es una sola funcion con todos los
# casos y el marcador reserva de golpe los locales de todos ellos, incluidos los
# dobles de host (SynthEngine + MIDITelemetryManager + SysExManager) que viven en
# la pila. Al anadir el Test 25 el marco paso del 1 MB por defecto de Windows y el
# proceso moria con STATUS_STACK_OVERFLOW (0xC00000FD) ya dentro del Test 17, que
# es solo el siguiente en usar un doble de host. 16 MB dejan margen de sobra.
#
# (Ese flag ya no esta: la suite vive ahora en una funcion por caso —ver
# `runAllTests()` en Source/Tests/DSPCoreTests.cpp—, asi que cada marco se libera
# al volver y el binario cabe en el 1 MB por defecto de Windows. Si algun dia se
# vuelve a juntar todo en una funcion, hara falta el flag otra vez.)
#
# El margen medido (Release, MSVC): con el binario partido en una funcion por caso la suite
# entera pasa con una pila de 512 KB y el 1 MB por defecto de Windows sobra; con 256 KB
# muere ya en el Test 21, que es el que mas pila necesita. Ese es el orden de magnitud del
# peor caso actual (el `SynthEngine` de cada test, no la suma de todos).

# --- Compile Definitions for WebView2 and Standalone
target_compile_definitions(ABDMS2000 PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)


