# Abstraction Layer Migration Plan: ABDMS2000 → Schema-Driven (ABDEep-style)

**Objective**: Elevate the current ad-hoc MIDI→DSP abstraction to a formal, schema-driven system identical in concept to ABDEep's `ParameterConversion` system. This will decouple the DSP entirely from raw CC/SysEx/NRPN values and provide a single source of truth for parameter conversions.

---

## 1. Current State (ABDMS2000)

| Layer | Status |
|---|---|
| **MIDI → Normalized [0,1]** | ✅ Implemented in `MIDITelemetryManager.cpp` via `MIDIMap` |
| **Normalized → DSP** | ⚠️ DSP receives normalized values (e.g., `filterCutoffNorm`) and converts internally with hardcoded formulas |
| **DSP ↔ raw CC/SysEx** | ❌ DSP is shielded but conversions are scattered and unnamed misleadingly (`convertSysExToCutoffHz` takes normalized 0-1, not raw SysEx) |
| **Schema/Registry** | ❌ No formal schema; `ParameterRegistry.gen.h` has `cc`, `sysexOffset`, `unit` but no `wire` type, `curve`, `domainMin/Max` driving conversions |
| **Conversion functions** | ⚠️ Scattered: `0.01f * pow(2000, pow(norm, 0.8))`, `0.020f + 0.980f * norm`, `DSPUtils::convertSysExToCutoffHz(norm)` |

**Goal**: Implement a schema-driven conversion system where:
- A single source (JSON or generated header) defines *how* each parameter converts
- The interface layer translates MIDI → normalized [0,1] (already done)
- The DSP layer uses generated functions to get physical units (Hz, seconds, etc.)
- The DSP never sees raw CC numbers or SysEx bytes

---

## 2. Target Architecture (ABDEep-style)

```
+---------------------+     +---------------------+     +---------------------+
|     MIDI IN         |     |   Interface Layer   |     |      DSP Core       |
+----------+----------+     +----------+----------+     +----------+----------+
           |                           |                           |
           | 1. CC/NRPN/SysEx raw      |                           |
           v                           v                           v
+----------+----------+     +----------+----------+     +----------+----------+
|   MIDI Map / APVTS  | --> |   Normalized [0,1]  | --> | Physical Units   |
|   (param ID + val)  |     |   values (norm)     |     |   (Hz, s, etc.)  |
+----------+----------+     +----------+----------+     +----------+----------+
           |                           |                           |
           | 2. Schema-driven        |                           |
           |    conversions          |                           |
           v                           v                           v
+---------------------+     +---------------------+     +---------------------+
|   Conversion Schema |     |   Generated Code    |     |   DSP Implementation|
|   (wire type,       |     |   (normalizedTo-   |     |   (uses generated   |
|    curve, unit,    |     |    domain, etc.)    |     |    functions)       |
+---------------------+     +---------------------+     +---------------------+
```

---

## 3. Migration Steps

### **Phase 1: Extend `ParameterRegistry.gen.h`** (1-2 days)
**Files**: `Source\State\ParameterRegistry.gen.h`

**Add fields to `ParameterMeta`**:
```cpp
struct ParameterMeta {
    // ... campos existentes ...
    int wire;              // -1=None, 0=CC, 1=SysEx, 2=NRPN
    float wireOffset;      // offset SysEx auto-computed (como ahora)
    // Nuevos campos:
    WireCodec wireCodec;   // Value, Bipolar, Enum
    Curve curve;           // Linear, Exp, Pow, Lut
    float domainMin;       // Mínimo físico (ej. 20.0 Hz)
    float domainMax;       // Máximo físico (ej. 20000.0 Hz)
    Smoothing smoothing;   // Linear, Log
    // ... resto ...
};
```

**Also add** (después de la enum `ParamType`):
```cpp
enum class Wire { None = 0, CC = 1, SysEx = 2, Nrpn = 3 };
enum class WireCodec { Value = 0, Bipolar = 1, Enum = 2 };
enum class Curve { Linear = 0, Exp = 1, Pow = 2, Lut = 3 };
enum class Smoothing { Linear = 0, Log = 1 };
```

**Update** `getAllParameters()` and `getParameter(const std::string& id)` to include new fields.

---

### **Phase 2: Populate the registry** (2-3 days)
**Files**: `Source\State\ParameterRegistry.gen.h` (datos) + `scripts/registry_generator.js` (o crear)

**Option A**: Editar manualmente los primeros parámetros críticos:
- `filterCutoff`: `wire=CC, ccNumber=74, curve=Pow, domainMin=20, domainMax=20000`
- `lfo1Rate`: `wire=CC, ccNumber=15, curve=Exp, curveP0=0.035, curveP1=7.5, unit=Hz`
- `delayTime`: `wire=CC, ccNumber=13, curve=Pow, domainMin=0.001, domainMax=1.4`
- `env1Attack`: `wire=None, curve=Exp, domainMin=0.001, domainMax=10.0`

**Option B**: Extender `registry_generator.js` (que ya existe en `Scripts/`) para leer un archivo JSON/CSV y generar el `ParameterRegistry.gen.h` automáticamente.

**Ejemplo de schema JSON** (nuevo archivo `scripts/parameter_schema.json`):
```json
{
  "parameters": [
    {
      "id": "filterCutoff",
      "wire": "CC",
      "ccNumber": 74,
      "wireCodec": "Value",
      "curve": "Pow",
      "curveP0": 0.0,
      "curveP1": 0.8,
      "unit": "Hz",
      "domainMin": 20.0,
      "domainMax": 20000.0,
      "smoothing": "Linear"
    },
    {
      "id": "lfo1Rate",
      "wire": "CC",
      "ccNumber": 15,
      "wireCodec": "Value",
      "curve": "Exp",
      "curveP0": 0.035,
      "curveP1": 7.5,
      "unit": "Hz",
      "domainMin": 0.05,
      "domainMax": 15.0,
      "smoothing": "Log"
    }
  ]
}
```

El script generaría automáticamente el `ParameterRegistry.gen.h` con los `Wire`, `WireCodec`, `Curve`, `domainMin/Max` llenados.

---

### **Phase 3: Generate conversion functions** (2-3 días)
**Archivos nuevos**: `Source\State\Conversion.gen.h` / `Conversion.gen.cpp` (análogo a ABDEep's `ParameterConversion.gen.h/.cpp`)

**Functions to generate** (mismo patrón que ABDEep):

```cpp
// normalized 0..1 <-> dominio, segun la curva del parametro.
float normalizedToDomain (const Entry& entry, float t) noexcept;
float domainToNormalized (const Entry& entry, float domain) noexcept;
// wire <-> dominio: EL CODEC. Es lo que sustituye a los numeros magicos del motor.
float wireToDomain (const Entry& entry, std::uint16_t raw) noexcept;
std::uint16_t domainToWire (const Entry& entry, float domain) noexcept;
```

**Lógica de curva** (mismo que ABDEep):

| `curve` | Fórmula |
|---|---|
| `Linear` | `t` (identidad) |
| `Exp` | `curveP0 * e^(curveP1 * t)` |
| `Pow` | `t^curveP1` (ej: `t^0.8`) |
| `Lut` | Tabla lookup (`lut[índice]`) |

**Ejemplo implementación** `normalizedToDomain`:
```cpp
float normalizedToDomain (const Entry& entry, float t) noexcept {
    switch (entry.curve) {
        case Curve::Linear:  return t;
        case Curve::Exp:     return entry.curveP0 * std::exp(entry.curveP1 * t);
        case Curve::Pow:     return std::pow(t, entry.curveP1);
        case Curve::Lut:     return entry.lut ? entry.lut[/*índice*/] : t;
    }
    return t;
}
```

---

### **Phase 4: Migrate DSP code to use generated conversions** (3-5 días)
**Archivo**: `Source\DSP\SynthEngine_Parameters.cpp`

**De (actual - fórmulas sueltas)**:
```cpp
// En updateParameters(), para cada voz:
voices[i].env1VCA.setParameters(
    domainOf("env1_attack", getFloat("env1_attack")),   // hardcoded
    domainOf("env1_decay",   getFloat("env1_decay")),
    getFloat("env1_sustain"),
    domainOf("env1_release", getFloat("env1_release"))
);

// LFOs - modelo Classic hardcoded:
const float lfo1RateHz = kDefaultRateScale * std::exp(kDefaultRateExp * getFloat("lfo1_rate"));

// VCF cutoff - también hardcoded en Voice.cpp:
float baseHz = DSPUtils::convertSysExToCutoffHz(p.filterCutoffNorm);
```

**A (schema-driven)**:
```cpp
// Usando las funciones generadas:
voices[i].env1VCA.setParameters(
    conversion::normalizedToDomain("env1_attack", getFloat("env1_attack")),
    conversion::normalizedToDomain("env1_decay",   getFloat("env1_decay")),
    getFloat("env1_sustain"),
    conversion::normalizedToDomain("env1_release", getFloat("env1_release"))
);

// LFOs - ahora del schema:
const float lfo1RateHz = conversion::normalizedToDomain("lfo1_rate", getFloat("lfo1_rate"));

// VCF cutoff - ahora del schema (ya no DSPUtils::convertSysExToCutoffHz):
float baseHz = conversion::normalizedToDomain("filterCutoff", getFloat("filterCutoff"));
```

**Key changes**:
- Agregar `#include "Conversion.gen.h"` al `SynthEngine_Parameters.cpp`
- Reemplazar todas las fórmulas `domainOf("param", ...)` por `conversion::normalizedToDomain("param", getFloat("param"))`
- Renombrar/mover `DSPUtils::convertSysExToCutoffHz` → `conversion::normToCutoffHz` (o mantener como wrapper que llama al schema)

---

### **Phase 5: Update MIDI translation** (Opcional, 2-3 días)
**Archivo**: `Source\MIDI\MidiTranslationEngine.h` (o crear nuevo `MidiTranslationEngine.gen.h`)

**Mejoras** (mismo patrón ABDEep `MidiTranslationEngine.h`):

**Añadir** funciones para traducir usando el schema:
```cpp
// Convierte un parámetro normalizado a mensajes MIDI CC/NRPN/SysEx
static std::vector<juce::MidiMessage> translateParamToMidiWithSchema(
    const juce::String& paramID, float normalizedValue, int midiChannel = 1);

// Convierte un mensaje MIDI entrante a parámetro normalizado usando el schema
static std::pair<juce::String, float> translateMidiToParamWithSchema(
    const juce::MidiMessage& message);
```

Esto permitiría:
- Desde el DSP generar SysEx/CC/NRPN para enviar al hardware
- Desde el hardware recibir y convertir automáticamente a valores normalizados

---

### **Phase 6: Rename/mfix misleading DSP functions** (1 día)
**Archivo**: `Source\DSP\Common\DSPUtils.h` / `Source\Core\Voice.cpp`

**Renombrar**:
- `DSPUtils::convertSysExToCutoffHz(norm)` → `conversion::normToCutoffHz(norm)` o `conversion::filteredToHz(norm)`
- Actualizar todos los `using` y calls

**Justificación**: La función no convierte de SysEx; convierte de normalized 0-1 a Hz. El nombre actual es misleading.

---

## 4. Standard Documentation (para futuros proyectos)

### Estándar: "Standard ABD Abstraction Layer"

#### Metadata per parameter (template - JSON o generado a header):

| Campo | Tipo | Descripción | Ejemplo |
|---|---|---|---|
| `id` | `string` | Identificador único del parámetro | `"filterCutoff"` |
| `wire` | `enum` | Tipo de mensaje MIDI origen | `CC`, `SysEx`, `Nrpn`, `None` |
| `ccNumber` / `nrpnMSB`, `nrpnLSB` | `int` | Número MIDI (-1 si none) | `74` o `-1` |
| `wireCodec` | `enum` | Códec de wire | `Value` (0-127/255), `Bipolar`, `Enum` |
| `curve` | `enum` | Curva de conversión | `Linear`, `Exp`, `Pow`, `Lut` |
| `curveP0` | `float` | Parámetro P0 de la curva | `0.035` (para Exp) |
| `curveP1` | `float` | Parámetro P1 de la curva | `7.5` (para Exp: `e^(7.5*t)`) |
| `unit` | `string` | Unidad física destino | `"Hz"`, `"s"`, `"ms"`, `"dB"` |
| `domainMin` | `float` | Mínimo del dominio físico | `20.0` |
| `domainMax` | `float` | Máximo del dominio físico | `20000.0` |
| `smoothing` | `enum` | Suavizado aplicado después | `Linear`, `Log` |

#### Generated code output (qué produce el script):

1. `ParameterConversion.gen.h/.cpp` - Funciones `normalizedToDomain`, `domainToNormalized`, `wireToDomain`, `domainToWire`
2. `ParameterRegistry.gen.h` - Expanded con campos `wire`, `curve`, `unit`, `domainMin/Max`
3. `MidiTranslationEngine.gen.h` - Funciones de traducción bidireccional CC/NRPN/SysEx ↔ parámetros
4. Actualizaciones automáticas a `SynthEngine_Parameters.cpp` hooks

#### Example schema JSON (fuente de verdad):

```json
{
  "schemaVersion": 1,
  "model": "MS2000",    // o "MicroKorg", "DeepMind12", etc.
  "parameters": [
    {
      "id": "filterCutoff",
      "wire": "CC",
      "ccNumber": 74,
      "wireCodec": "Value",
      "curve": "Pow",
      "curveP1": 0.8,
      "unit": "Hz",
      "domainMin": 20.0,
      "domainMax": 20000.0,
      "smoothing": "Linear"
    },
    {
      "id": "lfo1Rate",
      "wire": "CC",
      "ccNumber": 15,
      "wireCodec": "Value",
      "curve": "Exp",
      "curveP0": 0.035,
      "curveP1": 7.5,
      "unit": "Hz",
      "domainMin": 0.05,
      "domainMax": 15.0,
      "smoothing": "Log"
    },
    {
      "id": "delayTime",
      "wire": "CC",
      "ccNumber": 13,
      "wireCodec": "Value",
      "curve": "Pow",
      "curveP1": 0.5,
      "unit": "s",
      "domainMin": 0.001,
      "domainMax": 1.4,
      "smoothing": "Linear"
    },
    {
      "id": "env1Attack",
      "wire": "None",   // viene del panel/preset, no de MIDI en tiempo real
      "wireCodec": "Value",
      "curve": "Exp",
      "curveP0": 0.001,
      "curveP1": 3.0,
      "unit": "s",
      "domainMin": 0.001,
      "domainMax": 10.0,
      "smoothing": "Log"
    }
  ]
}
```

#### Benefits of this standard:

- ✅ **Single source of truth**: No fórmulas sueltas dispersas
- ✅ **Hardware independence**: Cambiar de CC a NRPN/SysEx sin tocar DSP
- ✅ **Multiple models**: El schema tiene `model` field (Dm12Hardware, AbyssmindPro, MS2000, etc.)
- ✅ **Consistent curves**: Exp, Pow, Lut idénticas en todas las capas
- ✅ **Future-proof**: Añadir nuevo parámetro = añadir entrada al schema, no DSP changes
- ✅ **Testable**: Las conversiones son verificables contra el schema
- ✅ **Reusable**: Mismo patrón para cualquier proyecto synth/signal processing

---

## 5. Risks & Mitigations

| Risk | Mitigation |
|---|---|
| **Breaking existing patches** | Hacer la migración incremental: primero añadir los nuevos campos al schema sin borrar los old. Las fórmulas old siguen funcionando hasta que se reemplaza `domainOf()` → `conversion::normalizedToDomain()`. |
| **Schema becomes too complex** | Empezar con los parámetros críticos (VCF, LFO, Envelopes) y ir añadiendo poco a poco. El schema es opcional: si falta un campo, la función fallback usa la fórmula hardcoded antigua. |
| **Performance overhead** | Las funciones `normalizedToDomain` son inline y la curva es switch simple. El impacto es negligible comparado con el DSP processing itself. |
| **Team adoption** | Documentar el estándar como se hace aquí. El script `registry_generator.js` puede generar el código automáticamente, reduciendo la carga manual. |

---

## 6. Immediate Next Steps (what to do TODAY)

1. **Leer** `Scripts/registry_generator.js` para entender cómo genera el registry actual
2. **Crear** `scripts/parameter_schema.json` con los primeros 10-20 parámetros críticos
3. **Extender** `ParameterRegistry.gen.h` con los nuevos campos `wire`, `curve`, `unit`, `domainMin/Max` (solo añadadir, no quitar)
4. **Probar** que el build sigue funcionando
5. **Migrar** 1 solo parámetro (ej. `filterCutoff`) en `SynthEngine_Parameters.cpp` de fórmulas hardcoded a `conversion::normalizedToDomain()`
6. **Renombrar** `DSPUtils::convertSysExToCutoffHz` → wrapper que llama al schema

---

## 7. File Map

| Propósito | Ruta |
|---|---|
| **Plan actual** | `ABSTRACTION_LAYER_MIGRATION_PLAN.md` (este archivo) |
| **Registry actual** | `Source\State\ParameterRegistry.gen.h` |
| **Script generator** | `Scripts/registry_generator.js` |
| **Schema JSON (nuevo)** | `scripts/parameter_schema.json` (por crear) |
| **Conversión generated** | `Source\State\Conversion.gen.h/.cpp` (por generar) |
| **MIDI Engine** | `Source\MIDI\MidiTranslationEngine.h` |
| **DSP migration** | `Source\DSP\SynthEngine_Parameters.cpp` |
| **Naming fix** | `Source\DSP\Common\DSPUtils.h`, `Source\Core\Voice.cpp` |

---

*Plan creado el [fecha] para proyecto ABDMS2000. Objetivo: migración a abstracción schema-driven tipo ABDEep, permitiendo total desacoplamiento DSP↔MIDI raw values y provingiendo una fuente única de verdad para conversiones de parámetros.*