#!/usr/bin/env node
/**
 * ABDMS2000 — generador del registro de parametros.
 *
 * Lee `schemas/parameters-spec.schema.v1.json` y emite los TRES artefactos que
 * consumen el host nativo y la WebUI (ver el manifiesto `ARTEFACTOS` de mas abajo,
 * que es la unica fuente de verdad de lo que produce):
 *
 *   Source/State/ParameterRegistry.gen.h    — el registro en C++ para el host
 *   Source/State/ParameterRegistry.gen.cpp  — sus tablas y lookups
 *   WebUI/src/contracts/registry.gen.js     — el registro que importa la WebUI
 *
 * USO
 *   node Scripts/registry_generator.js            # genera los tres
 *   node Scripts/registry_generator.js --check    # NO escribe. Sale 1 si lo
 *                                                 # commiteado no esta al dia.
 *   node Scripts/registry_generator.js --help
 *
 * El `--check` recorre TODAS las claves del manifiesto, no las que estan en una
 * lista suelta: un `.gen` que se anadiera al manifiesto sin estar en el check no
 * puede existir sin que alguien se entere.
 */
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { validateAndProcess, formatFloat } from './registry_core.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const specPath = path.join(rootDir, 'schemas', 'parameters-spec.schema.v1.json');

// ── Manifiesto de artefactos ──────────────────────────────────────────
//
// La unica fuente de verdad de lo que este generador produce. Antes las tres
// rutas vivian sueltas en el cuerpo del script, una por seccion (2, 3 y 4), y
// ninguna decia quien la consumia: si un `.gen` se movia, el generador se
// limitaba a escribir el nuevo y elViejo se quedaba commiteado y stale, sin
// que nada fallara. Por eso cada entrada declara su consumidor y `--check`
// recorre TODAS las claves: un `.gen` no declarado no puede existir sin que el
// check se entere.
const ARTEFACTOS = [
  {
    clave: 'h',
    rel: 'Source/State/ParameterRegistry.gen.h',
    consumidores: 'Source/State/ParameterRegistry.gen.cpp y todo el que incluye el registro',
  },
  {
    clave: 'cpp',
    rel: 'Source/State/ParameterRegistry.gen.cpp',
    consumidores: 'Source/State (enlazado); el build lo regenera antes de compilar',
  },
  {
    clave: 'js',
    rel: 'WebUI/src/contracts/registry.gen.js',
    consumidores: 'WebUI/src (PARAMETERS_SPEC, PARAM_LOOKUP, PARAM_IDS, PARAM_LIST)',
  },
];

const OUT = {};
for (const a of ARTEFACTOS) {
  OUT[a.clave] = path.join(rootDir, ...a.rel.split('/'));
}

// --check: NO escribe. Compara lo commiteado con lo que se generaria y sale
// distinto de cero si algo no esta al dia. Sin este flag el script escribe
// siempre, y un `node registry_generator.js --check` en un pipeline se
// encargaba de reescribir los artefactos y devolver exito: verde falso.
const CHECK_ONLY = process.argv.includes('--check');
const args = process.argv.slice(2).filter((a) => a !== '--check');

if (args.includes('--help') || args.includes('-h')) {
  console.log([
    'Genera el registro de parametros de ABDMS2000 (3 artefactos).',
    '',
    'Uso: node Scripts/registry_generator.js [--check]',
    '',
    '  (sin flag)  Escribe los 3 artefactos.',
    '  --check     NO escribe. Sale 1 si lo commiteado no esta al dia.',
    '',
    'Artefactos producidos:',
  ].concat(ARTEFACTOS.map((a) => `  ${a.rel}  ->  ${a.consumidores}`)).join('\n'));
  process.exit(0);
}
const argsDesconocidos = args.filter((a) => !a.startsWith('-'));
if (argsDesconocidos.length > 0) {
  console.error(`[registry] Argumento desconocido: ${argsDesconocidos.join(', ')} (usa --help)`);
  process.exit(2);
}

// Todo lo que se escribe, para el informe final y para el modo check.
const written = [];
const untouched = [];
const stale = [];

/**
 * Escribe un artefacto, o en `--check` solo lo compara con lo commiteado.
 * @param {string} clave clave del manifiesto (ver ARTEFACTOS)
 * @param {string} content contenido generado
 */
function emitir(clave, content) {
  const file = OUT[clave];
  const actual = (() => {
    try { return fs.readFileSync(file, 'utf8'); } catch { return null; }
  })();

  if (actual === content) {
    untouched.push(file);
    return;
  }
  if (CHECK_ONLY) {
    stale.push(file);
    return;
  }
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, content, 'utf8');
  written.push(file);
}

// ── Verificacion de rutas ────────────────────────────────────────────
//
// Que la fuente exista y que el manifiesto cuadre con OUT, ANTES de generar.
// Sin esto, un spec renombrado se manifiesta como un error de parseo de JSON o
// como `Cannot read properties of undefined`, que dice «el generador esta
// roto» sin decir «falta ESTE fichero en ESTA ruta». Un fallo de rutas debe
// leerse como un fallo de rutas.
(function verificarRutas() {
  const problemas = [];

  if (!fs.existsSync(specPath)) {
    problemas.push(`FUENTE_AUSENTE ${path.relative(rootDir, specPath).split(path.sep).join('/')}  (la fuente del spec)`);
  }

  for (const a of ARTEFACTOS) {
    // Normalizado a `/`: en Windows `path.relative` devuelve `Source\State\...`,
    // que nunca puede coincidir con una ruta escrita a mano en el manifiesto.
    const relativo = path.relative(rootDir, OUT[a.clave]).split(path.sep).join('/');
    if (relativo !== a.rel) {
      problemas.push(`MANIFIESTO_DESINCRONIZADO ${a.clave}: OUT dice ${relativo}, ARTEFACTOS dice ${a.rel}`);
    }
  }
  for (const clave of Object.keys(OUT)) {
    if (!ARTEFACTOS.some((a) => a.clave === clave)) {
      problemas.push(`OUT_SIN_DECLARAR ${clave} esta en OUT pero no en ARTEFACTOS`);
    }
  }

  // Directorios de salida: en --check se exige que existan (el artefacto commiteado
  // tiene que estar ahi para poder compararse); al escribir, se crean.
  for (const a of ARTEFACTOS) {
    const dir = path.dirname(OUT[a.clave]);
    if (fs.existsSync(dir)) continue;
    if (CHECK_ONLY) {
      problemas.push(`SALIDA_SIN_DIRECTORIO ${path.relative(rootDir, dir).split(path.sep).join('/')}/  (lo escribiría ${a.rel})`);
    } else {
      fs.mkdirSync(dir, { recursive: true });
    }
  }

  if (problemas.length === 0) return;

  console.error(`[registry] FALLO DE RUTAS — ${problemas.length} problema(s):`);
  for (const p of problemas) console.error(`  ✗ ${p}`);
  console.error('');
  console.error('  Fuente leida:');
  console.error(`    spec = ${path.relative(rootDir, specPath).split(path.sep).join('/')}`);
  console.error('  Artefactos escritos:');
  for (const a of ARTEFACTOS) console.error(`    ${a.rel}  ->  ${a.consumidores}`);
  process.exit(1);
})();

const spec = JSON.parse(fs.readFileSync(specPath, 'utf8'));

// 1. Normalize + validate the spec, auto-compute sysex offsets
const { processed: withOffsets, errors, sysexCount: sysexCounter } = validateAndProcess(spec.parameters);

if (errors.length > 0) {
  console.error('❌ Registry spec validation failed:');
  errors.forEach(e => console.error(`  - ${e}`));
  process.exit(1);
}

const params = withOffsets; // normalized + with sysexOffset attached

console.log(`${CHECK_ONLY ? '[check] Verificando' : 'Generando'} registry for ${params.length} parameters (${sysexCounter} with SysEx offsets)...`);

// Emit spec to .gen.js with the computed offsets injected, so WebUI consumers

// keep the same shape (null = no SysEx offset) without hand-maintaining values.
const emitSpec = {
  ...spec,
  parameters: withOffsets.map(p => {
    const { sysex, sysexOffset, ...rest } = p;
    return { ...rest, sysexOffset: sysexOffset < 0 ? null : sysexOffset };
  }),
};

// 2. Generate ParameterRegistry.gen.h
// La ruta de escritura NO es una variable local: sale del manifiesto ARTEFACTOS
// a traves de emitir(), para que un .gen no se pueda escribir por una via que
// el manifiesto no declara (y --check no veraz).
let hContent = `// AUTO-GENERATED BY registry_generator.js - DO NOT EDIT MANUALLY
#pragma once
#if __has_include(<juce_audio_processors/juce_audio_processors.h>)
#include <juce_audio_processors/juce_audio_processors.h>
#define ABD_HAS_JUCE 1
#else
#define ABD_HAS_JUCE 0
#endif
#include <string>
#include <vector>
#include <unordered_map>

namespace ABDMS2000 {

enum class ParamType {
    Continuous,
    Integer,
    Choice,
    Boolean
};

struct ParameterMeta {
    const char* id;
    const char* name;
    const char* group;
    int cc; // -1 if none
    float min;
    float max;
    float defaultValue;
    float skew;
    ParamType type;
    const char* unit;
    int sysexOffset; // -1 if none (auto-computed by generator)
    std::vector<const char*> choices;
};

class ParameterRegistry {
public:
    static const std::vector<ParameterMeta>& getAllParameters();
    static const ParameterMeta* getParameter(const std::string& id);
#if ABD_HAS_JUCE
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
#endif
};

namespace ParamIDs {
`;

params.forEach(p => {
  hContent += `    inline constexpr const char* ${p.id} = "${p.id}";\n`;
});

hContent += `} // namespace ParamIDs
} // namespace ABDMS2000
`;

emitir('h', hContent);

// 3. Generate ParameterRegistry.gen.cpp
let cppContent = `// AUTO-GENERATED BY registry_generator.js - DO NOT EDIT MANUALLY
#include "ParameterRegistry.gen.h"

namespace ABDMS2000 {

static const std::vector<ParameterMeta> kAllParameters = {
`;

withOffsets.forEach((p, idx) => {
  const ccVal = (p.cc !== null && p.cc !== undefined) ? p.cc : -1;
  const sysexVal = p.sysexOffset;
  const minVal = formatFloat(p.min);
  const maxVal = formatFloat(p.max);
  const defVal = formatFloat(p.default);
  const skewVal = formatFloat(p.skew || 1.0);
  const unitVal = p.unit ? `"${p.unit}"` : '""';

  let typeEnum = 'ParamType::Continuous';
  if (p.type === 'integer') typeEnum = 'ParamType::Integer';
  else if (p.type === 'choice') typeEnum = 'ParamType::Choice';
  else if (p.type === 'boolean') typeEnum = 'ParamType::Boolean';

  let choicesInit = '{}';
  if (p.choices && p.choices.length > 0) {
    choicesInit = `{ ${p.choices.map(c => `"${c}"`).join(', ')} }`;
  }

  cppContent += `    { "${p.id}", "${p.name}", "${p.group}", ${ccVal}, ${minVal}, ${maxVal}, ${defVal}, ${skewVal}, ${typeEnum}, ${unitVal}, ${sysexVal}, ${choicesInit} }`;
  if (idx < params.length - 1) cppContent += ',';
  cppContent += '\n';
});

cppContent += `};

const std::vector<ParameterMeta>& ParameterRegistry::getAllParameters() {
    return kAllParameters;
}

const ParameterMeta* ParameterRegistry::getParameter(const std::string& id) {
    static const std::unordered_map<std::string, const ParameterMeta*> kLookup = []() {
        std::unordered_map<std::string, const ParameterMeta*> map;
        for (const auto& meta : kAllParameters) {
            map[meta.id] = &meta;
        }
        return map;
    }();

    auto it = kLookup.find(id);
    return (it != kLookup.end()) ? it->second : nullptr;
}

#if ABD_HAS_JUCE
juce::AudioProcessorValueTreeState::ParameterLayout ParameterRegistry::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layoutParams;

    for (const auto& meta : kAllParameters) {
        if (meta.type == ParamType::Choice && !meta.choices.empty()) {
            juce::StringArray choiceArray;
            for (const auto* c : meta.choices) {
                choiceArray.add(c);
            }
            layoutParams.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(meta.id, 1),
                meta.name,
                choiceArray,
                static_cast<int>(meta.defaultValue)
            ));
        } else if (meta.type == ParamType::Boolean) {
            layoutParams.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(meta.id, 1),
                meta.name,
                meta.defaultValue > 0.5f
            ));
        } else if (meta.type == ParamType::Integer) {
            layoutParams.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID(meta.id, 1),
                meta.name,
                static_cast<int>(meta.min),
                static_cast<int>(meta.max),
                static_cast<int>(meta.defaultValue)
            ));
        } else {
            juce::NormalisableRange<float> range(meta.min, meta.max);
            if (meta.skew > 0.01f && meta.skew < 0.99f) {
                range.setSkewForCentre(meta.min + (meta.max - meta.min) * meta.skew);
            }
            layoutParams.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(meta.id, 1),
                meta.name,
                range,
                meta.defaultValue
            ));
        }
    }

    return { layoutParams.begin(), layoutParams.end() };
}
#endif

} // namespace ABDMS2000
`;

emitir('cpp', cppContent);

// 4. Generate registry.gen.js
let jsContent = `// AUTO-GENERATED BY registry_generator.js - DO NOT EDIT MANUALLY

export const PARAMETERS_SPEC = ${JSON.stringify(emitSpec, null, 2)};

export const PARAM_LOOKUP = Object.freeze(
  PARAMETERS_SPEC.parameters.reduce((acc, param) => {
    acc[param.id] = param;
    return acc;
  }, {})
);

export const PARAM_IDS = Object.freeze(
  PARAMETERS_SPEC.parameters.reduce((acc, param) => {
    acc[param.id] = param.id;
    return acc;
  }, {})
);

export const PARAM_LIST = Object.freeze(PARAMETERS_SPEC.parameters);
`;

emitir('js', jsContent);

// ── Informe ─────────────────────────────────────────────────────────
console.log(`   SysEx offset map: ${withOffsets.filter(p => p.sysexOffset >= 0).length} params, max=${sysexCounter - 1}`);
console.log(`   Artefactos declarados: ${ARTEFACTOS.length} · al día: ${untouched.length} · desfasados: ${stale.length}`);
for (const a of ARTEFACTOS) console.log(`  ${a.clave.padEnd(4)} ${a.rel}  ->  ${a.consumidores}`);

if (CHECK_ONLY) {
  if (stale.length > 0) {
    console.error('[registry] DESFASADO — ejecuta `node Scripts/registry_generator.js` y commitea:');
    for (const f of stale) console.error(`  ✗ ${path.relative(rootDir, f).split(path.sep).join('/')}`);
    process.exit(1);
  }
  console.log('✅ [registry] OK — los 3 artefactos commiteados están al día.');
  process.exit(0);
}

console.log(`✅ Registry artifacts successfully generated! (${written.length} escritos, ${untouched.length} ya al día)`);
for (const f of written) console.log(`  ~ ${path.relative(rootDir, f).split(path.sep).join('/')}`);
