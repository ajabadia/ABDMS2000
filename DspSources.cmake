# ============================================================================
# ABDMS2000 - Listas compartidas de fuentes DSP (single-source, anti-drift)
#
# Consumidas por DOS builds:
#   - CMakeLists.txt raiz (plugin nativo + suite de tests)
#   - wasm/CMakeLists.txt (build WebAssembly)
#
# HISTORIA: el build WASM duplicaba la lista a mano y sufrio drift — le
# faltaban SynthEngine.cpp, DWGSOscillator.cpp, OSC2Modulator.cpp y
# MIDITelemetryManager.cpp respecto al nativo (el motor web sonaba, pero no
# era el sintetizador completo). Ahora las dos listas viven aqui.
#
# MS2000_DSP_SOURCES       : DSP completo (nativo, JUCE disponible).
# MS2000_DSP_SOURCES_WASM  : subconjunto compilable bajo Emscripten (sin JUCE).
#
# DIFERENCIA WASM (documentada, NO drift):
#   - Core/SynthEngine.cpp         -> su cabecera incluye juce_audio_processors
#                                     (ineligible en el build WASM sin-JUCE).
#   - MIDI/MIDITelemetryManager.cpp-> idem (juce_audio_processors/basics).
#   El resto de la lista es identica: si anades un modulo DSP al nativo,
#   debe ir tambien en WASM (o justificar arriba por que no).
#
# Las rutas usan CMAKE_CURRENT_LIST_DIR: al procesar este fichero apunta al
# directorio que lo contiene (la raiz del proyecto), sea quien sea el
# consumidor (raiz o wasm/).
#
# NOTA: el build WASM anade por su cuenta SysExCodec.cpp (de ABDSharedCode,
# compilado directo porque HardwareDrivers depende de juce_audio_basics) y
# WasmBridge.cpp — extras por-consumidor, no parte de esta lista.
# ============================================================================

set(MS2000_DSP_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/SynthEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/Voice.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/VoiceManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/VAOscillator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/VoxWaveOscillator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/DWGSTables.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/DWGSOscillator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/NoiseGenerator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/OSC2Modulator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Filters/MultiModeFilter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Filters/FilterResonanceComp.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Modulation/VirtualPatchMatrix.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Sequencer/ModSequencer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Sequencer/Arpeggiator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Effects/ModFX.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Effects/DelayFX.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Effects/Equalizer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Vocoder/Vocoder16Band.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/State/ParameterRegistry.gen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/MIDI/MIDIMap.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/MIDI/MIDITelemetryManager.cpp"
)

set(MS2000_DSP_SOURCES_WASM
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/Voice.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/VoiceManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/VAOscillator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/VoxWaveOscillator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/DWGSTables.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/DWGSOscillator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/NoiseGenerator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Oscillators/OSC2Modulator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Filters/MultiModeFilter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Filters/FilterResonanceComp.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Modulation/VirtualPatchMatrix.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Sequencer/ModSequencer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Sequencer/Arpeggiator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Effects/ModFX.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Effects/DelayFX.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Effects/Equalizer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DSP/Vocoder/Vocoder16Band.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/State/ParameterRegistry.gen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/MIDI/MIDIMap.cpp"
)
