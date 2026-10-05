#!/usr/bin/env node
/**
 * ABDMS2000 — Host model id generator
 *
 * Emite la identidad del sintetizador anfitrión (el `modelId` que
 * `BridgeActions` anuncia al ABD Bank Manager embebido con la acción `hostModel`)
 * como header C++, derivada del contrato canónico. Así el C++ no repite el
 * literal: la fuente única es el ModelContract.
 *
 * Cadena de procedencia (una sola fuente, artefactos generados):
 *   ABDBankManager/Source/Contracts/Models/korg-ms2000.ts   (korgAbdSm002Contract)
 *     -> ABDBankManager `npm run generate`
 *        -> WebUI/src/contracts/gen/modelContracts.gen.js
 *           -> Scripts/sync_bankmanager.js
 *              -> WebUI/abdbank/src/contracts/gen/modelContracts.gen.js
 *                 -> este script
 *                    -> Source/Plugin/HostModelId.gen.h
 *
 * Uso: node Scripts/generate_host_model_id.js
 *      (paso 1 de build.bat y de `npm run generate`, siempre tras sincronizar)
 *
 *      node Scripts/generate_host_model_id.js --check
 *      NO escribe nada: sale 1 si `Source/Plugin/HostModelId.gen.h` commiteado no es
 *      lo que saldria de generar.
 *
 *      UNA EXCEPCION, Y ES DELIBERADA: el check NO compara la linea
 *      `kHostBuildRevision`. Ese valor es el `git rev-parse` del momento, asi que
 *      cambia en cada commit POR DISENO —es un sello, no contenido—, y si se
 *      comparara el check seria rojo siempre y acabaria sin mirarse. Lo que se
 *      compara es todo lo demas: el modelId, el nombre, el fabricante, el nivel de
 *      puente. Si el contrato cambia, el check se pone rojo.
 *
 * El WebUI del host lee el mismo contrato por su lado
 * (WebUI/src/contracts/hostModel.js), así que nativo y dev server/WASM anuncian
 * el mismo id sin duplicarlo.
 */

import fs from 'fs';
import path from 'path';
import { execFileSync } from 'child_process';
import { fileURLToPath, pathToFileURL } from 'url';
import { escribirGenerado, leerFlags } from './syncSeguro.mjs';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(__dirname, '..');

function usage() {
  console.log([
    'Emite Source/Plugin/HostModelId.gen.h desde el contrato canonico del host.',
    '',
    'Uso: node Scripts/generate_host_model_id.js [--check] [--help]',
    '',
    '  (sin flag)  Escribe el header si su contenido cambio.',
    '  --check     NO escribe. Sale 1 si el commiteado no es lo que se generaria.',
    '              Compara todo MENOS kHostBuildRevision, que es un sello de git y',
    '              cambia en cada commit por diseño.',
  ].join('\n'));
}

const flags = leerFlags('host model id');
if (flags.ayuda) { usage(); process.exit(0); }
if (flags.desconocido) {
  usage();
  console.error(`Argumento desconocido: ${flags.desconocido}`);
  process.exit(2);
}

/** Artefacto sincronizado que exporta los contratos canónicos. */
const CONTRACTS_BUNDLE = path.join(
  rootDir, 'WebUI', 'abdbank', 'src', 'contracts', 'gen', 'modelContracts.gen.js'
);
/** Export del contrato que identifica a este anfitrión (ABD MS2000 / SM002). */
const HOST_CONTRACT_EXPORT = 'korgAbdSm002Contract';
const OUTPUT_H = path.join(rootDir, 'Source', 'Plugin', 'HostModelId.gen.h');

if (!fs.existsSync(CONTRACTS_BUNDLE)) {
  console.error(`ERROR: no se encontro ${path.relative(rootDir, CONTRACTS_BUNDLE)}`);
  console.error('       Ejecuta antes: node Scripts/sync_bankmanager.js');
  process.exit(1);
}

const bundle = await import(pathToFileURL(CONTRACTS_BUNDLE).href);
const contract = bundle[HOST_CONTRACT_EXPORT];

if (!contract || typeof contract.modelId !== 'string' || contract.modelId.length === 0) {
  console.error(`ERROR: ${HOST_CONTRACT_EXPORT}.modelId no esta en el bundle de contratos.`);
  console.error('       Revisa ABDBankManager/Source/Contracts/Models/korg-ms2000.ts y resincroniza.');
  process.exit(1);
}

// El anfitrión tiene que ser un contrato registrado: si desaparece del registro,
// el Bank Manager embebido no podria resolver su hardware (ver modelRegistry).
const registered = (bundle.allModelContracts ?? []).some(c => c.modelId === contract.modelId);
if (!registered) {
  console.error(`ERROR: ${contract.modelId} no aparece en allModelContracts.`);
  process.exit(1);
}

/** Escapa un literal para un string C++ entre comillas. */
function cppString(value) {
  return String(value ?? '').replace(/\\/g, '\\\\').replace(/"/g, '\\"');
}

/**
 * Nivel de puente que habla este binario. Son capacidades, no fechas: así el Bank
 * Manager embebido puede exigir lo que necesita en vez de suponer por antiguedad.
 *   1 = bridge basico (sin Bank Manager embebido)
 *   2 = anuncia el modelId del host (`hostModel`)
 *   3 = responde `requestHostInfo` (identidad + sello de build)
 *   4 = puente MIDI de hardware (`hardware.listen` / `hardware.send` /
 *       `hardware.receive`), que es lo que permite traer un banco real
 */
const BRIDGE_PROTOCOL = 4;

/**
 * Revision del codigo con el que se genera este header (el build lo usa como
 * sello estable, al lado de la fecha de compilacion). `no-git` si no hay git.
 */
function gitRevision() {
  try {
    const sha = execFileSync('git', ['rev-parse', '--short', 'HEAD'], {
      cwd: rootDir,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore']
    }).trim();
    return sha || 'unknown';
  } catch {
    return 'no-git';
  }
}

const header = `// AUTO-GENERATED BY Scripts/generate_host_model_id.js - DO NOT EDIT MANUALLY
//
// Identidad del sintetizador anfitrion, derivada del contrato canonico
// (ABDBankManager/Source/Contracts/Models/korg-ms2000.ts -> ${HOST_CONTRACT_EXPORT})
// a traves del artefacto sincronizado
// WebUI/abdbank/src/contracts/gen/modelContracts.gen.js.
//
// Regenerar tras cualquier cambio de contrato:
//   node Scripts/generate_host_model_id.js
// (lo hacen el paso 1 de build.bat y \`npm run generate\`)

#pragma once

namespace ABDMS2000 {

/** modelId canonico del host: lo que anuncia la accion bridge \`hostModel\`. */
inline constexpr const char* kHostModelId = "${cppString(contract.modelId)}";

/** Nombre y fabricante del contrato (diagnostico / logs). */
inline constexpr const char* kHostModelDisplayName = "${cppString(contract.displayName)}";
inline constexpr const char* kHostModelManufacturer = "${cppString(contract.manufacturer)}";

/**
 * Nivel de puente del host (capacidades, no fechas). El Bank Manager embebido
 * lo usa para decir si el binario de al lado sabe anunciar su modelo (>=2),
 * responder su ficha (>=3) y mover bytes por el puente MIDI de hardware (>=4).
 */
inline constexpr int kHostBridgeProtocol = ${BRIDGE_PROTOCOL};

/** Revision del codigo en el momento de generar este header. */
inline constexpr const char* kHostBuildRevision = "${cppString(gitRevision())}";

/**
 * Sello de build del binario: la unidad que incluya este header se compila con
 * su propia fecha/hora, que es lo que identifica al ejecutable que hay al otro
 * lado del bridge.
 */
#define ABD_HOST_BUILD_STAMP (__DATE__ " " __TIME__)

} // namespace ABDMS2000
`;

// El sello de git se EXCLUYE de la comparacion, no del contenido: el header lo
// lleva porque el binario lo necesita, pero comparar un valor que cambia en cada
// commit daria un check rojo permanente. Ver la cabecera del fichero.
const esSelloDeGit = (linea) => linea.includes('kHostBuildRevision');

if (!escribirGenerado({
  destino: OUTPUT_H,
  contenido: header,
  ignorar: esSelloDeGit,
  check: flags.check,
})) {
  process.exit(1);
}

console.log(`   (${HOST_CONTRACT_EXPORT}.modelId = "${contract.modelId}")`);
