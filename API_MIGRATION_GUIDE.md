# Guía de Migración de API - ABDMS2000

**Versión:** 1.0  
**Fecha:** 2026-09-25  
**Estado:** Tests habilitados: 303/303 pasando | Pendientes: Voice, RapidSweep

---

## 🎯 Resumen Ejecutivo

La API de **ABDMS2000** diverge significativamente de **ABDEep** (base original de los tests portados). Los tests heredados usan APIs obsoletas que ya no existen en el código actual.

**Tests pasando:** 303/303 ✅  
**Tests deshabilitados:** Voice, RapidSweep  
**Riesgo de refactor:** 🟡 Medio-Alto  
**Beneficio estimado:** 🟢 Alto (cobertura completa de tests)

---

## 📋 Tabla de Migración API por Componente

### 1. SynthEngine

| Método Antiguo (ABDEep) | Método Actual (ABDMS2000) | Notas |
|------------------------|---------------------------|-------|
| `panic()` | **NO EXISTE** | Usar: `engine.allNotesOff(); engine.setPitchBend(0); engine.setModWheel(0);` |
| `processBlock(buffer, midi)` | `processBlock(buffer, midi, audioPlayHead*)` | 3er parámetro obligatorio (`AudioPlayHead*`), pasar `nullptr` en tests |
| `setPortamentoTime()` | No existe en API pública | Configurar via `VoiceParameters.portamentoTime` en `applyBlockParams()` |
| `setPortamentoMode()` | No existe en API pública | Configurar via `VoiceParameters.portaMode` |

### 2. ParameterLayout (Crítico - Breaking Change)

| Antes (ABDEep) | Ahora (ABDMS2000) | Solución |
|----------------|-------------------|----------|
| `juce::AudioProcessorValueTreeState::ParameterLayout layout = ...` | `auto layout = ParameterRegistry::createParameterLayout();` | Usar `auto` + `std::move(layout)` |
| `layout` se copiaba libremente | **Move-only** (copy constructor = delete) | `std::move(layout)` al construir APVTS |

```cpp
// ❌ ANTIGUO (no compila)
juce::AudioProcessorValueTreeState::ParameterLayout layout = ParameterRegistry::createParameterLayout();
juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", layout);

// ✅ ACTUAL
auto layout = ParameterRegistry::createParameterLayout();
juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));
```

### 3. SynthEngine::processBlock()

| Firma Antigua | Firma Actual | Uso en Tests |
|---------------|--------------|--------------|
| `processBlock(buffer, midi)` | `processBlock(buffer, midi, audioPlayHead*)` | `engine.processBlock(buffer, midi, nullptr)` |

**Regla:** Siempre pasar 3 argumentos. En tests pasar `nullptr` como 3er parámetro.

### 4. SysExManager

| Método Antiguo | Método Actual | Notas |
|----------------|---------------|-------|
| `handleIncomingSysEx(data, size)` | `parseSysEx(data, size, apvts)` | Retorna `SysExParseResult` (struct con `success`, `messageType`, `reply`) |
| `exportProgramDump()` | `exportSingleProgram(apvts, name, dump, channel)` | Usa `MemoryBlock` en lugar de `vector<uint8_t>` |

### 5. MS2000SysExExporter

| Método Antiguo | Método Actual |
|----------------|---------------|
| `exportProgramDump(apvts, index, name)` | `exportSingleProgram(apvts, name, dump, channel)` |
| Retorna `vector<uint8_t>` | Usa `MemoryBlock& dump` (out parameter) |

```cpp
// ❌ ANTIGUO
std::vector<uint8_t> dump = exporter.exportProgramDump(apvts, 0, "Test");

// ✅ ACTUAL
juce::MemoryBlock dump;
exporter.exportSingleProgram(apvts, "Test Patch", dump, 1);
std::vector<uint8_t> vec(static_cast<const uint8_t*>(dump.getData()), 
                          static_cast<const uint8_t*>(dump.getData()) + dump.getSize());
```

### 6. VirtualPatchMatrix

| API Antigua | API Actual |
|-------------|------------|
| `setRoute(slot, src, dst, amount)` | `setSlot(slot, PatchSource, PatchDestination, intensity)` |
| `getModulationValue(dst, sources)` | `evaluate(sources)` retorna `PatchModulationOutputs` struct |
| `PatchSource::LFO1` | `VirtualPatchMatrix::PatchSource::LFO1` (enum class) |
| `PatchDestination::Pitch` | `VirtualPatchMatrix::PatchDestination::Pitch` (enum class) |

```cpp
// ❌ ANTIGUO
matrix.setRoute(0, 0, 0, 0.8f); // src=0(LFO1), dst=0(Pitch)
float mod = matrix.getModulationValue(0, sources);

// ✅ ACTUAL
matrix.setSlot(0, VirtualPatchMatrix::PatchSource::LFO1, 
               VirtualPatchMatrix::PatchDestination::Pitch, 0.8f);
auto outputs = matrix.evaluate(sources);
float mod = outputs.pitchMod;
```

### 7. ADSREnvelope (abd::synth::ADSREnvelope)

| Método Antiguo | Método Actual |
|----------------|---------------|
| `setSampleRate(sr)` | `prepare(sampleRate)` |
| `setParameters(a,d,s,r)` | `setParameters(attack, decay, sustain, release)` |
| `trigger()` | `trigger()` ✅ |
| `release()` | `release()` ✅ |
| `nextSample()` | `getNextSample()` |
| `isActive()` | `!isIdle()` o `isActive()` según versión |
| `setLoopMode(bool)` | `setLoopMode(bool)` ✅ |
| `getCurrentLevel()` | `getCurrentLevel()` ✅ |

### 8. LFO (abd::synth::LFO)

| Método Antiguo | Método Actual |
|----------------|---------------|
| `setSampleRate(sr)` | `prepare(sampleRate)` |
| `setFrequencyHz(hz)` | `setFrequencyHz(hz)` ✅ |
| `setWaveform(shape)` | `setWaveformLFO1(LFOWaveform)` / `setWaveformLFO2(LFOWaveformLFO2)` |
| `setWaveform(LFO::Sine)` | `setWaveformLFO1(LFOWaveform::Sine)` |
| `setKeySync(mode)` | `setKeySyncMode(KeySyncMode)` + `triggerKeySync(bool)` |
| `getNextSample()` | `getNextSample()` ✅ |
| `getCurrentValue()` | `getCurrentValue()` ✅ |

### 9. PortamentoGlide (abd::synth::PortamentoGlide)

| Método Antiguo | Método Actual |
|----------------|---------------|
| `setMode(mode)` | **Eliminado** - modo implícito en `setTargetNote` |
| `setTime(time)` | `setGlideTime(float normalized)` (0.0 = instant, 1.0 = max) |
| `getRate()` | **Eliminado** - usar `getNextPitchSemitones()` |
| `reset()` | `reset(startNote)` |
| `setTargetNote(note)` | `setTargetNote(targetNote, glideEnabled)` |
| `getNextPitchSemitones()` | ✅ Igual |

```cpp
// ❌ ANTIGUO
glide.setMode(0);
glide.setTime(0.5f);
glide.reset(60.0f);
glide.setTargetNote(72.0f);
float rate = glide.getRate();

// ✅ ACTUAL
glide.prepare(sampleRate);
glide.setGlideTime(0.5f);
glide.reset(60.0f);
glide.setTargetNote(72.0f, true); // true = glide enabled
float pitch = glide.getNextPitchSemitones();
```

### 9. ParameterLayout - Move Only

```cpp
// ❌ ANTIGUO (no compila - copy constructor deleted)
juce::AudioProcessorValueTreeState::ParameterLayout layout = ParameterRegistry::createParameterLayout();
juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", layout);

// ✅ CORRECTO
auto layout = ParameterRegistry::createParameterLayout();
juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));
```

### 10. M_PI → juce::MathConstants

```cpp
// ❌ ANTIGUO
#define M_PI 3.14159265358979323846

// ✅ ACTUAL
juce::MathConstants<double>::pi
// o para float:
juce::MathConstants<float>::pi
```

### 10. expectWithinAbsoluteError - Template Ambiguity

```cpp
// ❌ PROBLEMA: template ambiguity con literales double
expectWithinAbsoluteError(value, expected, 1e-5f, "msg");  // error: ambiguous

// ✅ SOLUCIÓN: Cast explícito
expectWithinAbsoluteError(value, expected, (float)1e-5f, "msg");
expectWithinAbsoluteError(value, expected, (float)0.001f, "msg");
```

---

## 📋 Checklist de Migración por Archivo

### DSPCoreTests_RapidSweep.cpp
- [ ] ADSREnvelope: `setSampleRate` → `prepare`, `setParameters`, `trigger`, `release`, `getNextSample`, `isActive` → `!isIdle()`
- [ ] LFO: `Sine` → `LFOWaveform::Sine`, `setSampleRate` → `prepare`, `setWaveform` → `setWaveformLFO1`
- [ ] VirtualPatchMatrix: `setRoute` → `setSlot`, `getModulationValue` → `evaluate`
- [ ] `expectWithinAbsoluteError` con casts explícitos `(float)`

### DSPCoreTests_Voice.cpp
- [ ] VirtualPatchMatrix: `setRoute` → `setSlot`, `getModulationValue` → `evaluate()`
- [ ] ADSREnvelope: mismos cambios que RapidSweep
- [ ] LFO: `Sine` → `LFOWaveform::Sine`
- [ ] PolyBLEP: `polyBlep2` → `ABDMS2000::DSPUtils::polyBlep2` o `abd::synth::DSPUtils::polyBlep2`
- [ ] `std::max` con float: usar `std::max(a, b)` no `std::max({a,b})`

---

## ⚠️ Errores Comunes y Soluciones

| Error | Causa | Solución |
|-------|-------|----------|
| `ParameterLayout copy constructor deleted` | Intentar copiar `ParameterLayout` | Usar `auto layout = ...` + `std::move(layout)` |
| `expectWithinAbsoluteError ambiguous` | Literal double/float ambiguo | Cast explícito: `(float)1e-5f` |
| `M_PI not defined` | `M_PI` no estándar en MSVC | Usar `juce::MathConstants<double>::pi` |
| `processBlock takes 2 args but 3 given` | Firma cambiada | Añadir `nullptr` como 3er param: `engine.processBlock(buf, midi, nullptr)` |
| `exportProgramDump not found` | API cambiada | Usar `exportSingleProgram(apvts, name, dump, channel)` |
| `SysExParseResult not found` | Namespace incorrecto | Usar `SysExManager::SysExParseResult` o `ABDMS2000::SysExParseResult` |
| `getOwner() not found` | DummyProcessor no tiene getOwner | Usar `MinimalProcessor` sin getOwner, pasar engine directamente |

---

## 📝 Plantilla para DummyProcessor en Tests

```cpp
class MinimalProcessor : public juce::AudioProcessor {
public:
    MinimalProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
    const juce::String getName() const override { return "Minimal"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Minimal"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
```

---

## 📋 Checklist de Validación Pre-Commit

- [ ] `clang-format --dry-run --Werror Source/**/*.h Source/**/*.cpp`
- [ ] `cppcheck --enable=warning,performance,portability Source/`
- [ ] `cmake --build build --config Release --target ABDMS2000_Tests --parallel`
- [ ] `./build/Release/ABDMS2000_Tests.exe` → 303 tests pass
- [ ] `DSPCoreTests_Voice.cpp` compila (si habilitado)
- [ ] `DSPCoreTests_RapidSweep.cpp` compila (si habilitado)

---

## 📌 Referencias Rápidas

| Archivo | Ubicación | Descripción |
|---------|-----------|-------------|
| `TestCompat.h` | `Source/Tests/TestCompat.h` | Shims de compatibilidad |
| `TestCompat.cpp` | `Source/Tests/TestCompat.cpp` | Implementación shims |
| `SynthEngine.h` | `Source/Core/SynthEngine.h` | API principal motor |
| `VirtualPatchMatrix.h` | `Source/DSP/Modulation/VirtualPatchMatrix.h` | API matriz patches |
| `ADSREnvelope.h` | `Source/DSP/Envelopes/ADSREnvelope.h` | Envolventes (shim a abd::synth) |
| `LFO.h` | `Source/DSP/Modulation/LFO.h` | LFOs (shim a abd::synth) |
| `PortamentoGlide.h` | `Source/DSP/Modulation/PortamentoGlide.h` | Portamento (shim a abd::synth) |
| `MS2000SysExExporter.h` | `Source/MIDI/MS2000SysExExporter.h` | Exportador SysEx |
| `SysExManager.h` | `Source/MIDI/SysExManager.h` | Gestor SysEx |
| `ParameterRegistry.gen.h` | `Source/State/ParameterRegistry.gen.h` | Registry parámetros |

---

## 🚫 Errores Prohibidos (No Cometer)

| ❌ No Hacer | ✅ Hacer En Su Lugar |
|-------------|---------------------|
| `ParameterLayout layout = ...` (copia) | `auto layout = ...; std::move(layout)` |
| `M_PI` | `juce::MathConstants<double>::pi` |
| `expectWithinAbsoluteError(x, y, 1e-5f)` | `EXPECT_WITHIN_ABS_ERROR_FLOAT(x, y, 1e-5f)` |
| `processBlock(buf, midi)` | `processBlock(buf, midi, nullptr)` |
| `exportProgramDump()` | `exportSingleProgram()` |
| `handleIncomingSysEx()` | `parseSysEx()` |
| `glide.setMode()` / `glide.setTime()` | `glide.setGlideTime()` + `setTargetNote()` |
| `LFO::Sine` | `LFOWaveform::Sine` |
| `setSampleRate()` en ADSR/LFO | `prepare(sampleRate)` |
| `nextSample()` en ADSR | `getNextSample()` |
| `isActive()` en ADSR | `!isIdle()` o `isActive()` según versión |

---

*Documento generado automáticamente - Actualizar tras cada cambio de API significativo*