cmake_minimum_required(VERSION 3.22)

project(ABDMS2000 VERSION 1.0.0 LANGUAGES C CXX)

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

# Create JUCE Plugin
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

# --- WebUI Binary Assets ---
file(GLOB_RECURSE WEBUI_ASSETS 
    "WebUI/src/*" 
    "WebUI/assets/*"
    "WebUI/icons/*" 
    "WebUI/index.html" 
)
list(FILTER WEBUI_ASSETS EXCLUDE REGEX "WebUI/dist")
list(FILTER WEBUI_ASSETS EXCLUDE REGEX "WebUI/tests")
juce_add_binary_data(WebUIAssets 
    HEADER_NAME "WebUIAssets.h" 
    NAMESPACE "WebUIAssets" 
    SOURCES ${WEBUI_ASSETS}
)
target_link_libraries(ABDMS2000 PUBLIC WebUIAssets)

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
target_sources(ABDMS2000 PRIVATE
    # Core Engine & Architecture
    Source/Core/SynthEngine.h
    Source/Core/SynthEngine.cpp
    Source/Core/Voice.h
    Source/Core/Voice.cpp
    Source/Core/VoiceManager.h
    Source/Core/VoiceManager.cpp
    Source/Core/AudioThreadSnapshot.h
    Source/Core/HardwareConstants.h
    Source/Core/BuildVersion.h

    # DSP Common & Math
    Source/DSP/Common/DSPUtils.h
    Source/DSP/Common/PolyBLEP.h
    Source/DSP/Common/PolyBLEP.cpp

    # DSP Oscillators
    Source/DSP/Oscillators/WavetableLoader.h
    Source/DSP/Oscillators/WaveCategory.h
    Source/DSP/Oscillators/VAOscillator.h
    Source/DSP/Oscillators/VAOscillator.cpp
    Source/DSP/Oscillators/VoxWaveOscillator.h
    Source/DSP/Oscillators/VoxWaveOscillator.cpp
    Source/DSP/Oscillators/DWGSTables.h
    Source/DSP/Oscillators/DWGSTables.cpp
    Source/DSP/Oscillators/DWGSOscillator.h
    Source/DSP/Oscillators/DWGSOscillator.cpp
    Source/DSP/Oscillators/NoiseGenerator.h
    Source/DSP/Oscillators/NoiseGenerator.cpp
    Source/DSP/Oscillators/OSC2Modulator.h
    Source/DSP/Oscillators/OSC2Modulator.cpp

    # DSP Filters
    Source/DSP/Filters/MultiModeFilter.h
    Source/DSP/Filters/MultiModeFilter.cpp
    Source/DSP/Filters/FilterResonanceComp.h
    Source/DSP/Filters/FilterResonanceComp.cpp

    # DSP Envelopes & Modulation
    Source/DSP/Envelopes/EnvelopeCurves.h
    Source/DSP/Envelopes/ADSREnvelope.h
    Source/DSP/Envelopes/ADSREnvelope.cpp
    Source/DSP/Modulation/PortamentoGlide.h
    Source/DSP/Modulation/PortamentoGlide.cpp
    Source/DSP/Modulation/LFO.h
    Source/DSP/Modulation/LFO.cpp
    Source/DSP/Modulation/VirtualPatchMatrix.h
    Source/DSP/Modulation/VirtualPatchMatrix.cpp

    # DSP Sequencers
    Source/DSP/Sequencer/ModSequencer.h
    Source/DSP/Sequencer/ModSequencer.cpp
    Source/DSP/Sequencer/Arpeggiator.h
    Source/DSP/Sequencer/Arpeggiator.cpp

    # DSP Effects
    Source/DSP/Effects/ModFX.h
    Source/DSP/Effects/ModFX.cpp
    Source/DSP/Effects/DelayFX.h
    Source/DSP/Effects/DelayFX.cpp
    Source/DSP/Effects/Equalizer.h
    Source/DSP/Effects/Equalizer.cpp

    # DSP Vocoder
    Source/DSP/Vocoder/EnvelopeFollower.h
    Source/DSP/Vocoder/Vocoder16Band.h
    Source/DSP/Vocoder/Vocoder16Band.cpp

    # State & Parameters
    Source/State/ParameterRegistry.gen.h
    Source/State/ParameterRegistry.gen.cpp
    Source/State/MS2000PatchBuilder.h
    Source/State/MS2000FactoryBank.h
    Source/State/LCDMenuFormatter.h


    # MIDI Telemetry & SysEx
    Source/MIDI/MIDIMap.h
    Source/MIDI/MIDIMap.cpp
    Source/MIDI/NRPNParser.h
    Source/MIDI/NRPNParser.cpp
    Source/MIDI/MIDITelemetryManager.h
    Source/MIDI/MIDITelemetryManager.cpp
    Source/MIDI/SysExCodec.h
    Source/MIDI/SysExCodec.cpp
    Source/MIDI/MS2000ProgramData.h
    Source/MIDI/SysExManager.h
    Source/MIDI/SysExManager.cpp
    Source/MIDI/MS2000SysExExporter.h



    # Plugin Wrappers & WebUI Bridge
    Source/Plugin/PluginProcessor.h
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.h
    Source/Plugin/PluginEditor.cpp
    Source/Plugin/PluginEditor_ResourceProvider.h
    Source/Plugin/PluginEditor_ResourceProvider.cpp
    Source/Plugin/BridgeActions.h
    Source/Plugin/BridgeActions.cpp
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
    Source/Utils
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
)

# --- C++ Unit Tests (Standalone executable, no JUCE GUI) ---
add_executable(ABDMS2000_Tests
    Source/Tests/DSPCoreTests.cpp
    Source/Core/VoiceManager.cpp
    Source/Core/Voice.cpp
    Source/Core/SynthEngine.cpp
    Source/DSP/Common/PolyBLEP.cpp
    Source/DSP/Oscillators/VAOscillator.cpp
    Source/DSP/Oscillators/DWGSOscillator.cpp
    Source/DSP/Oscillators/DWGSTables.cpp
    Source/DSP/Oscillators/NoiseGenerator.cpp
    Source/DSP/Oscillators/VoxWaveOscillator.cpp
    Source/DSP/Oscillators/OSC2Modulator.cpp
    Source/DSP/Filters/MultiModeFilter.cpp
    Source/DSP/Filters/FilterResonanceComp.cpp
    Source/DSP/Envelopes/ADSREnvelope.cpp
    Source/DSP/Modulation/LFO.cpp
    Source/DSP/Modulation/PortamentoGlide.cpp
    Source/DSP/Modulation/VirtualPatchMatrix.cpp
    Source/DSP/Sequencer/ModSequencer.cpp
    Source/DSP/Sequencer/Arpeggiator.cpp
    Source/DSP/Effects/ModFX.cpp
    Source/DSP/Effects/DelayFX.cpp
    Source/DSP/Effects/Equalizer.cpp
    Source/DSP/Vocoder/Vocoder16Band.cpp
    Source/State/ParameterRegistry.gen.cpp
    Source/MIDI/MIDIMap.cpp
    Source/MIDI/NRPNParser.cpp
    Source/MIDI/SysExCodec.cpp
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
)

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
)
target_compile_features(ABDMS2000_Tests PRIVATE cxx_std_20)

# --- Compile Definitions for WebView2 and Standalone
target_compile_definitions(ABDMS2000 PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)


