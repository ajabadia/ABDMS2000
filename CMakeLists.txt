cmake_minimum_required(VERSION 3.22)

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build Universal Binary for macOS (Intel + Apple Silicon)")
endif()

project(ABDMS2000 VERSION 1.0.0 LANGUAGES C CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(JUCE_USE_WIN_WEBVIEW2 ON CACHE BOOL "Use WebView2 on Windows" FORCE)

# --- JUCE 9 discovery/setup ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/GITS/JUCE/CMakeLists.txt")
    add_subdirectory(GITS/JUCE)
elseif(EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory(${JUCE_PATH} JUCE)
elseif(EXISTS "C:/JUCE/CMakeLists.txt")
    add_subdirectory("C:/JUCE" JUCE)
else()
    find_package(JUCE 9 CONFIG REQUIRED)
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

# ============================================================
# ABDShared — DspCore, SynthCore, Scope, AutoUpdater, WebView2Bridge
# Prioriza el repositorio canónico ABDShared/Code, con fallback a ABDSharedCode.
# ============================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../ABDShared/Code/CMakeLists.txt")
    set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDShared/Code")
    message(STATUS "ABDShared: Using canonical Code at ${ABDSHARED_CODE_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/CMakeLists.txt")
    set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode")
    message(STATUS "ABDShared: Using legacy sibling at ${ABDSHARED_CODE_DIR}")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../ABDBankManager/cpp/CMakeLists.txt")
    # ABDBankManager ya incluye ABDShared; evitar duplicacion de targets
    set(ABDSHAREDCODE_SKIP_ADD_SUBDIRECTORY ON CACHE INTERNAL "" FORCE)
    message(STATUS "ABDShared: Skipping add_subdirectory (included by ABDBankManager)")
endif()

if(NOT ABDSHAREDCODE_SKIP_ADD_SUBDIRECTORY)
    if(DEFINED ABDSHARED_CODE_DIR)
        add_subdirectory("${ABDSHARED_CODE_DIR}" "${CMAKE_BINARY_DIR}/ABDShared_build")
    else()
        include(FetchContent)
        FetchContent_Declare(
          ABDShared
          GIT_REPOSITORY https://github.com/ajabadia/ABDShared.git
          GIT_TAG        main
        )
        FetchContent_MakeAvailable(ABDShared)
    endif()
endif()

# --- ABDScope Integration (analytical multi-lane scope, native C++ taps) ---
# El modulo Scope vive AHORA en ABDSharedCode (subtree desde v0.4.0) y se
# registra al anadir ABDSharedCode mas arriba: targets ABDShared::ScopeCore
# (core header-only + glue JUCE + WebUI embebida) y ABDShared::ScopeCoreHeaders
# (solo el core, sin JUCE, que es lo que enlazan los tests).
#
# Antes habia aqui un add_subdirectory(../ABDScope) con target
# ABDScope::ABDScopeCore. No se puede mantener: dos add_subdirectory del mismo
# modulo en un mismo build mueren en "add_library cannot create target
# ABDScopeCore...". Fuente unica de verdad, un solo registro, en ABDSharedCode.

# Create JUCE Plugin
# Lista DSP compartida con el build WASM (single-source, anti-drift):
# DspSources.cmake define MS2000_DSP_SOURCES y MS2000_DSP_SOURCES_WASM.
include("${CMAKE_CURRENT_SOURCE_DIR}/DspSources.cmake")

juce_add_plugin(ABDMS2000
    COMPANY_NAME "ajabadia"
    COMPANY_WEBSITE "https://github.com/ajabadia/ABDMS2000"
    COMPANY_EMAIL "support@abdsynths.com"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Abad
    PLUGIN_CODE Ms2k
    VST3_CATEGORIES "Instrument" "Synth"
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
list(FILTER WEBUI_ASSETS EXCLUDE REGEX "WebUI/dist/abdbank")
juce_add_binary_data(WebUIAssets 
    HEADER_NAME "WebUIAssets.h" 
    NAMESPACE "WebUIAssets" 
    SOURCES ${WEBUI_ASSETS}
)
target_link_libraries(ABDMS2000 PUBLIC WebUIAssets)

# --- ABDBankManager WebUI is served from disk ---
# The modal loads /abdbank/* via the disk fallback in pluginResourceProvider.

# Embedded DWGS Wavetable Binaries (Korg DW-8000 + AKWF curated bundles) + Official MS2000 v1.07 Firmware
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
        "DOCS/firmware/x811v107.sys"
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
    Source/MIDI/MS2000IplUpdater.h
    Source/MIDI/MS2000IplUpdater.cpp
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
    ABDShared::SynthCore
    ABDShared::HardwareDrivers
    ABDShared::ScopeCore
    ABDShared::AutoUpdater
)

# --- C++ Unit Tests (Standalone executable, no JUCE GUI) ---
add_executable(ABDMS2000_Tests
    Source/Tests/DSPCoreTests.cpp
    Source/Tests/DSPCoreTests_VCF.cpp
    Source/Tests/DSPCoreTests_Pitch.cpp
    Source/Tests/DSPCoreTests_Transfer.cpp
    Source/Tests/DSPCoreTests_CalSpec.cpp
    Source/Tests/DSPCoreTests_Portamento.cpp
    Source/Tests/DSPCoreTests_RapidSweep.cpp
    Source/Tests/DSPCoreTests_EqualizerParity.cpp
    Source/Tests/TestCompat.cpp
    Source/Tests/DSPCoreTests_Panic.cpp
    # Source/Tests/DSPCoreTests_Voice.cpp          # TODO: Fix VirtualPatchMatrix/ADSREnvelope/LFO API
    # Source/Tests/DSPCoreTests_Portamento.cpp     # TODO: Fix PortamentoGlide API
    Source/Plugin/BridgeActions.cpp
    ${MS2000_DSP_SOURCES}
    Source/MIDI/SysExManager.cpp
    Source/MIDI/MS2000IplUpdater.cpp
)

# Add JUCE module include directories for <JuceHeader.h> support
target_include_directories(ABDMS2000_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/GITS/JUCE/modules
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
    ABDShared::AutoUpdater
    # Core de Scope header-only (SynthEngine.h incluye <ScopeDataCollector.h>):
    # propaga .../Scope/Source y .../Source/Core sin arrastrar el glue JUCE.
    ABDShared::ScopeCoreHeaders
)

# BridgeActions.h/BridgeHost.h live in Source/Plugin for the tests too.
target_include_directories(ABDMS2000_Tests PRIVATE Source/Plugin)

target_include_directories(ABDMS2000_Tests PRIVATE
    Source
    Source/Core
    Source/DSP
    Source/DSP/Common
    Source/DSP/Oscillators
    Source/DSP/Envelopes
    Source/DSP/Modulation
    Source/DSP/Filters
    Source/MIDI
    Source/State
    Source/Tests
)

# El core de Scope (ScopeDataCollector.h, <ScopeDataCollector.h> a pelo) llega
# por target y no por path: ABDShared::ScopeCoreHeaders propaga
# ABDSharedCode/Scope/Source y .../Source/Core, igual que hacian las dos lineas
# de ABDSCOPE_DIR que aqui se borraban.

# Remove JUCE modules path to avoid PATH duplication in MSBuild
# JUCE targets already propagate include directories via target_link_libraries
target_compile_features(ABDMS2000_Tests PRIVATE cxx_std_20)

# Increase MSVC compiler heap limit for large files (SynthEngine.cpp)
if (MSVC)
    target_compile_options(ABDMS2000_Tests PRIVATE /Zm2000)
    # Apply higher limit specifically to SynthEngine.cpp
    set_source_files_properties(Source/Core/SynthEngine.cpp PROPERTIES COMPILE_FLAGS "/Zm2000")
endif()

add_test(NAME ABDMS2000_Tests COMMAND ABDMS2000_Tests)

# Pila del binario de tests: en Release (/O2) MSVC puede hacer inline de funciones
# `static` dentro de `runAllTests()` y acumular los marcos de pila (SynthEngine +
# MIDITelemetryManager + SysExManager + MS2000FactoryBank), superando el 1 MB por
# defecto de Windows y abortando en `__chkstk` con STATUS_STACK_OVERFLOW (0xC00000FD)
# antes del primer `printf`. 16 MB dejan margen de sobra (igual que en ABDShared).
if(WIN32)
    if(MSVC)
        target_link_options(ABDMS2000_Tests PRIVATE /STACK:16777216)
    else()
        target_link_options(ABDMS2000_Tests PRIVATE "-Wl,--stack,16777216")
    endif()
endif()

# --- C++ ProcessBlock Benchmarks & Zero-Allocation Audit ---
add_executable(ABDMS2000_Benchmarks
    Source/Tools/Benchmarks/ProcessBlockBenchmark.cpp
    ${MS2000_DSP_SOURCES}
)

target_include_directories(ABDMS2000_Benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/GITS/JUCE/modules
    Source
    Source/Core
    Source/DSP
    Source/DSP/Common
    Source/DSP/Oscillators
    Source/DSP/Envelopes
    Source/DSP/Modulation
    Source/DSP/Filters
    Source/MIDI
    Source/State
)

target_link_libraries(ABDMS2000_Benchmarks PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    DWGSBinary
    ABDShared::SynthCore
    ABDShared::HardwareDrivers
    ABDShared::ScopeCoreHeaders
)

target_compile_features(ABDMS2000_Benchmarks PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(ABDMS2000_Benchmarks PRIVATE /Zm2000)
endif()

if(WIN32)
    if(MSVC)
        target_link_options(ABDMS2000_Benchmarks PRIVATE /STACK:16777216)
    else()
        target_link_options(ABDMS2000_Benchmarks PRIVATE "-Wl,--stack,16777216")
    endif()
endif()

add_test(NAME ABDMS2000_Benchmarks COMMAND ABDMS2000_Benchmarks --ci)

# --- Compile Definitions for WebView2 and Standalone
target_compile_definitions(ABDMS2000 PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)


