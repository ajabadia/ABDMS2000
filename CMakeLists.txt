cmake_minimum_required(VERSION 3.22)

project(ABDMS2000 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include JUCE 8
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/GITS/JUCE/CMakeLists.txt")
    add_subdirectory(GITS/JUCE)
elseif(EXISTS "C:/JUCE/CMakeLists.txt")
    add_subdirectory("C:/JUCE" JUCE)
else()
    find_package(JUCE 8 CONFIG REQUIRED)
endif()

# On Windows, fetch Microsoft.Web.WebView2 headers and static loader
if(WIN32)
    include(FetchContent)
    FetchContent_Declare(
        webview2
        URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2592.51
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(webview2)
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

# --- WebUI Binary Assets ---
file(GLOB_RECURSE WEBUI_ASSETS 
    "WebUI/src/*" 
    "WebUI/icons/*" 
    "WebUI/*.html" 
    "WebUI/*.js" 
    "WebUI/*.webmanifest" 
    "WebUI/*.css" 
    "WebUI/*.png" 
    "WebUI/*.svg" 
    "WebUI/*.json"
)
juce_add_binary_data(WebUIAssets 
    HEADER_NAME "WebUIAssets.h" 
    NAMESPACE "WebUIAssets" 
    SOURCES ${WEBUI_ASSETS}
)
target_link_libraries(ABDMS2000 PRIVATE WebUIAssets)

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
    Source/DSP/Oscillators/VAOscillator.h
    Source/DSP/Oscillators/VAOscillator.cpp
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

    # State & Parameters
    Source/State/ParameterRegistry.gen.h
    Source/State/ParameterRegistry.gen.cpp

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
target_include_directories(ABDMS2000 PRIVATE
    Source
    Source/Core
    Source/DSP
    Source/DSP/Common
    Source/DSP/Oscillators
    Source/DSP/Filters
    Source/DSP/Envelopes
    Source/DSP/Modulation
    Source/Plugin
    Source/State
    Source/MIDI
    Source/Utils
)

if(WIN32 AND DEFINED webview2_SOURCE_DIR)
    target_include_directories(ABDMS2000 PRIVATE
        ${webview2_SOURCE_DIR}/build/native/include
    )
endif()

# Link JUCE Modules
target_link_libraries(ABDMS2000 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# On Windows, statically link WebView2Loader and version.lib
if(WIN32 AND DEFINED webview2_SOURCE_DIR)
    target_link_libraries(ABDMS2000 PRIVATE
        ${webview2_SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib
        version.lib
    )
endif()

# Compile Definitions for WebView2
target_compile_definitions(ABDMS2000 PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
