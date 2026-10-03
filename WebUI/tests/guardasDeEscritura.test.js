/**
 * guardasDeEscritura.test.js — todo script que escriba tiene que decir COMO no escribir.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * QUE ESTE TEST VIGILA, Y POR QUE HACE FALTA UN TEST QUE VIGILE LOS TESTS
 *
 * En ABDEep y ABDNeural hay el mismo guard. Este es su hermano, y existe porque un guard
 * que solo vigila un repo vigila la mitad del problema: los `sync_*.js` de este repo
 * hacen `rmSync(destino, { recursive: true })` y luego `cpSync`, o sea SUSTITUYEN el
 * destino entero. Eso es mas peligroso que generar un header, y estaba sin ninguna
 * forma de preguntar.
 *
 * Exige dos cosas de cada script que escribe: que su cabecera declare `--check`,
 * `--dry-run` o `--force`, y que ese flag exista ademas en codigo (un flag documentado
 * que nadie parsea es peor que ninguno).
 *
 * Y hay una tercera puerta, la `SIN-GUARDIA:`. Sirve para los scripts que escriben y
 * NO PUEDEN tener un check que sea verde —`build_webui.js` estampa la fecha y la hora
 * en sus dos salidas, asi que su contenido cambia en cada build por diseno—. Con la
 * excepcion no se salta el test: hay que escribir el motivo, el motivo se lee en el
 * mensaje de fallo si alguien anade otro, y la linea esta en el fichero para el que
 * lo lea. Un script que se autoexime sin explicar nada no se exonera: el motivo vacio
 * es motivo insuficiente.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * ESCRIBIR A TRAVES DE UN HELPER TAMBIEN ES ESCRIBIR
 *
 * El fallo mas probable de este test no es que se le escape un `writeFileSync` en un
 * script nuevo: es el contrario. En cuanto se escribe un helper que escribe —que es
 * justo lo que se hace para que el guardia este en UN sitio y no en varios—, los
 * scripts que lo usan dejan de tener ninguna llamada de `node:fs` y desaparecerian
 * del inventario. Un guard que se queda sin ver al que vigila cuando este mejora es
 * un guard que ya no vigila.
 *
 * Por eso el inventario es transitivo: un script que importa un modulo RELATIVO que
 * escribe, escribe. Se propaga hasta que no cambia nada (y con tope de rondas, que
 * evita el bucle infinito si alguien se importa a si mismo).
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * POR QUE ESTE FICHERO ESTA COPIADO Y NO IMPORTADO
 *
 * Los tres repos tienen un guard identico, y podria haber uno solo en
 * ABDSharedAssets. No puede, por un motivo concreto: un guard que importara del
 * hermano dejaria de vigilar justo cuando ese hermano no este clonado —que es justo
 * cuando un script nuevo entra— y no se caeria, se saltaria. Un guard que se salta
 * en silencio es peor que no tener guard.
 *
 * Lo que si se hace es que los tres no se separen: los genera
 * ABDSharedAssets/scripts/generar-guardas-escritura.mjs, y su `--check` falla si
 * este fichero deja de ser lo que sale de ahi. Editarlo a mano no esta prohibido —
 * se nota en la siguiente corrida.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * QUE SE LEE Y QUE NO, Y POR QUE ESTA HECHO ASI
 *
 * El criterio de «escribe» es una lista de llamadas de `node:fs`, y se busca SOLO en
 * lineas que no sean de comentario. Eso evita el falso positivo de un comentario que
 * explica que el script no escribe nunca, pero tiene dos limitaciones que conviene
 * decir aqui en vez de descubrir dentro de seis meses:
 *
 *   · Un `writeFileSync` que aparezca dentro de una cadena de texto SI cuenta como
 *     escritura. No hay parser de JavaScript aqui a proposito —un test que necesita
 *     el parser del proyecto para leer un test no puede correr cuando el proyecto
 *     esta roto, que es justo cuando hace falta— y el coste de ese falso positivo es
 *     un test mas conservador, no uno que deja pasar al escritor. Para PowerShell si
 *     se quitan las cadenas antes de buscar, porque hay un `Write-Host` que MENciona
 *     `Remove-Item` para explicar que hace.
 *   · Los `.ps1` NO se miran. La convencion de flags de PowerShell no es la de Node, y meterlos aqui haria que el test aceptase dos lenguos de contrato distintos. Queda NOMBRADO en la configuracion del generador para que la exclusion se lea como decision y no como agujero.
 *
 * Y los `*.test.js` se excluyen del inventario: un test escribe ficheros a proposito
 * (este no, pero los de la bateria si), y exigirles un flag seria pedirles que no se
 * proben.
 */

// ═══════════════════════════════════════════════════════════════════ GENERADO
// ESTE FICHERO NO SE EDITA A MANO.
//
// Motor y configuracion: ABDSharedAssets/scripts/generar-guardas-escritura.mjs
// Repo: ABDMS2000
//
//   Regenerar:  node ../ABDSharedAssets/scripts/generar-guardas-escritura.mjs --repo ABDMS2000
//   Comprobar:  node ../ABDSharedAssets/scripts/generar-guardas-escritura.mjs --check
//
// Editarlo a mano no da error: da un `--check` en rojo la proxima vez que corra
// alguien. Se materializa aqui y no se importa a proposito — un guard que
// dependiera del repo hermano dejaria de vigilar justo cuando ese repo no esta
// clonado, que es cuando un script nuevo entra.
// ═══════════════════════════════════════════════════════════════════════════
import { describe, it, expect } from 'vitest';
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const RAIZ = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..\\..");

const CARPETAS = [
  path.join(RAIZ, "Scripts"),
  path.join(RAIZ, "WebUI\\scripts"),
  path.join(RAIZ, "tools"),
];

/**
 * Flags que significan «no escribo sin que me lo pidas». Todos valen: cada script
 * elige el que le encaja, y lo que se exige es que ELGIA uno.
 */
const CONTRATOS = {
  js: [{ cabecera: "--check", codigo: "--check" }, { cabecera: "--dry-run", codigo: "--dry-run" }, { cabecera: "--force", codigo: "--force" }],

};

// ─────────────────────────────────────────────────────────────────────────────
// QUE CUENTA COMO ESCRIBIR
//
// Los cmdlets de PowerShell que tocan el disco. `Write-Host`, `Write-Error`,
// `Write-Warning` y `Write-Output` NO estan y no deben estar: escriben en la
// consola, no en el disco. Un guard que contara `Write-Host` como escribir
// declararia casi todos los scripts del repo, que es no distinguir nada.
//
// `Remove-Item` si esta: borrar un arbol es lo que un clean o un sincronizador no
// deben hacer sin que alguien lo haya pedido.
const ESCRIBE = {
  js: [
    "writeFileSync",
    "writeFile",
    "appendFileSync",
    "appendFile",
    "copyFileSync",
    "cpSync",
    "createWriteStream",
    "rmSync",
    "rm",
    "unlinkSync",
    "rmdirSync",
    "renameSync",
    "mkdirSync",
    "mkdir",
    "writeSync",
  ],

};

const EXENCION = "SIN-GUARDIA:";

const esPowerShell = (nombre) => /\.ps1$/i.test(nombre);
const lenguajeDe = (nombre) => (esPowerShell(nombre) ? 'ps1' : 'js');

// Los scripts de PowerShell se llaman SIN parentesis (`Set-Content $ruta`), asi que
// el detector de Node —que exige `(`— no los veria. En el regex es opcional.
const reJs = (fn) => new RegExp(`(?<![\\w$])${fn}\\s*\\(`);
const rePs = (fn) => new RegExp(`(?<![\\w$-])${fn}\\b`);

// Lo que hay entre comillas dentro de una linea no es codigo que se ejecuta: es un
// mensaje. Sin esto, un `Write-Host "... un Remove-Item ..."` —escrito para EXPLICAR
// el borrado— contaria como si el script borrara algo, y el script se declararia
// escritor de si mismo.
const sinCadenas = (linea) => linea
  .replace(/"(?:[^"\\]|\\.)*"/g, '""')
  .replace(/'(?:[^'\\]|\\.)*'/g, "''");

function noEsCodigo(linea, powerShell) {
  const t = linea.trim();
  if (t === '') return true;
  if (powerShell) return t.startsWith('#');
  return t.startsWith('//') || t.startsWith('*') || t.startsWith('/*') || t.startsWith('*/');
}

function esScript(fichero) {
  return (/\.m?js$/.test(fichero))
    && !/\.test\.[cm]?js$/.test(fichero);
}

// La cabecera de un `.ps1` es el bloque `<# ... #>` del principio, o los comentarios
// `#` que precedan al `param(`. Las lineas de texto plano DENTRO del bloque no
// empiezan por `#`, asi que la regla de «linea de comentario» por si sola cortaria
// la cabecera en la primera, y ni el flag documentado ni el marcador de exencion se
// verian.
function cabeceraDe(lineas, powerShell) {
  const cabecera = [];
  let enBloque = false;
  for (const linea of lineas) {
    const t = linea.trim();
    if (!powerShell) {
      // El shebang cuenta como cabecera: si no, un script que empieza por
      // `#!/usr/bin/env node` tendria la cabecera cortada en la primera linea.
      if (noEsCodigo(linea, false) || linea.startsWith('#!')) { cabecera.push(linea); continue; }
      break;
    }
    if (!enBloque && t.startsWith('<#')) { enBloque = true; cabecera.push(linea); continue; }
    if (enBloque) {
      cabecera.push(linea);
      if (t.startsWith('#>')) { enBloque = false; }
      continue;
    }
    if (t === '' || t.startsWith('#')) { cabecera.push(linea); continue; }
    break;
  }
  return cabecera;
}

const llamadas = (linea, lenguaje) => {
  const fuente = lenguaje === 'ps1' ? sinCadenas(linea) : linea;
  const lista = ESCRIBE[lenguaje] || [];
  const re = lenguaje === 'ps1' ? rePs : reJs;
  return lista.filter((fn) => re(fn).test(fuente));
};
const conFlags = (linea, lenguaje) => (CONTRATOS[lenguaje] || [])
  .map((c) => c.codigo)
  .filter((f) => linea.includes(f));

/**
 * Lo que dice de un script: escribe?, lo declara en la cabecera?, y el flag existe
 * en codigo o solo en el texto?
 */
export function analizar(texto, nombre = 'x.js') {
  const lenguaje = lenguajeDe(nombre);
  const powerShell = esPowerShell(nombre);
  const lineas = texto.split('\n');
  const cabecera = cabeceraDe(lineas, powerShell);

  const escribe = new Set();
  const alcanzables = new Set();
  const importados = new Set();
  for (const linea of lineas) {
    if (noEsCodigo(linea, powerShell)) continue;
    if (!powerShell && linea.startsWith('#!')) continue;
    for (const fn of llamadas(linea, lenguaje)) { escribe.add(fn); }
    for (const f of conFlags(linea, lenguaje)) { alcanzables.add(f); }
    // Solo los imports RELATIVOS: los de un paquete no se pueden seguir sin leer
    // node_modules, y ningun escritor de estos repos llega ahi para escribir.
    for (const m of linea.matchAll(/\bfrom\s+['"](\.[^'"]+)['"]/g)) { importados.add(m[1]); }
  }

  const declarados = (CONTRATOS[lenguaje] || [])
    .filter((c) => cabecera.some((l) => l.includes(c.cabecera)))
    .map((c) => c.cabecera);

  // Lo declarado y lo implementado se comparan por CONTRATO, no comparando cadenas.
  // En Node coinciden (`--check` en la cabecera, `--check` en el codigo) y comparar
  // los textos funciona. En PowerShell NO: se documenta `-Check` y se lee `$Check`,
  // asi que comparar los dos conjuntos de cadenas no encuentra nada y todos los
  // `.ps1` parecerian declarar un flag que no usan.
  const implementados = declarados.filter((cual) => {
    const contrato = (CONTRATOS[lenguaje] || []).find((c) => c.cabecera === cual);
    return contrato !== undefined && alcanzables.has(contrato.codigo);
  });

  // La exencion es un PARRAFO, no una palabra: se lee desde el marcador hasta la
  // linea en blanco siguiente. Recoger solo la primera linea dejaria el motivo a
  // medias, que es justo el fallo que la exencion existe para evitar.
  const i = cabecera.findIndex((l) => l.includes(EXENCION));
  let exencion = null;
  if (i >= 0) {
    const parrafo = [cabecera[i]];
    for (let j = i + 1; j < cabecera.length && cabecera[j].trim() !== ''; j++) {
      parrafo.push(cabecera[j]);
    }
    exencion = parrafo.join(' ');
  }

  return {
    escribe: escribe.size > 0,
    llamadas: [...escribe],
    importados: [...importados],
    declarados,
    implementados,
    exencion,
    via: null,
  };
}

/** Donde acaba un `./x` si el repo lo tiene: con extension o sin ella. */
function resolver(desde, especificador) {
  const base = path.resolve(path.dirname(desde), especificador);
  for (const candidata of [base, base + '.js', base + '.mjs', path.join(base, 'index.js')]) {
    if (existsSync(candidata) && statSync(candidata).isFile()) { return candidata; }
  }
  return null;
}

/** Todos los scripts de las carpetas vigiladas, con la propagacion de escritores. */
export function inventario() {
  const entradas = [];
  for (const carpeta of CARPETAS) {
    if (!existsSync(carpeta)) continue;
    for (const nombre of readdirSync(carpeta)) {
      if (!esScript(nombre)) continue;
      const absoluta = path.join(carpeta, nombre);
      if (!statSync(absoluta).isFile()) continue;
      entradas.push({
        absoluta,
        ruta: path.relative(RAIZ, absoluta).replace(/\\/g, '/'),
        ...analizar(readFileSync(absoluta, 'utf8'), nombre),
      });
    }
  }

  const porRuta = new Map(entradas.map((e) => [e.absoluta, e]));
  for (const e of entradas) {
    e.deps = e.importados.map((spec) => resolver(e.absoluta, spec)).map((p) => porRuta.get(p)).filter(Boolean);
  }

  // Punto fijo: escribir a traves de un helper que escribe a traves de otro sigue
  // siendo escribir. El tope no es por cycling: es por no depender de que el grafo
  // de imports este bien formado, que no depende de este test.
  for (let ronda = 0; ronda < 10; ronda++) {
    let cambio = false;
    for (const e of entradas) {
      if (e.escribe) continue;
      const escritor = e.deps.find((d) => d.escribe);
      if (!escritor) continue;
      e.escribe = true;
      e.via = escritor.ruta;
      e.llamadas = [`via ${escritor.ruta}`];
      cambio = true;
    }
    if (!cambio) break;
  }

  return entradas;
}

/** El motivo de una exencion, sin el adorno de comentario de la cabecera. */
export const motivoDe = (s) => (s.exencion === null
  ? ''
  : s.exencion.replace(EXENCION, '').replace(/^[\s/*#]+/, '').trim());

/** Los que escriben y NO se han eximido: la lista que tiene que estar justificada. */
const escritores = () => inventario().filter((s) => s.escribe && !s.exencion);

describe('Scripts/ — el que escribe, pregunta antes', () => {
  it('este test VE a los escritores de verdad, no un conjunto vacio', () => {
    // La asercion que hace que las de abajo valgan: si el detector se rompe y no ve
    // a nadie, todas pasan en verde y el test no vigila nada. Por eso se nombran
    // ficheros concretos de mecanismos distintos.
    const rutas = escritores().map((s) => s.ruta);

    // sustituye el destino entero.
    expect(rutas).toContain("Scripts/sync_assets.js");
    // sustituye el destino entero.
    expect(rutas).toContain("Scripts/sync_bankmanager.js");
    // sustituye el destino entero.
    expect(rutas).toContain("Scripts/sync_scope.js");
    // un generador de header.
    expect(rutas).toContain("Scripts/generate_korg_channel.js");
    // el otro generador de header.
    expect(rutas).toContain("Scripts/generate_host_model_id.js");

    // Y que el filtro de extension no se los haya tragado.
    expect(esScript("sync_assets.js")).toBe(true);
    expect(esScript("sync_bankmanager.js")).toBe(true);
    expect(esScript("sync_scope.js")).toBe(true);
    expect(esScript('registry_generator.test.js')).toBe(false);
    expect(esScript('bundle_code.ps1')).toBe(false);
  });

  it('ve a los que escriben a TRAVES de un helper, que es donde se le escaparian', () => {
    // Los sincronizadores delegan la escritura en syncSeguro.mjs. Un detector que solo mirase el fichero pasaria por alto justo a los que se dejaron el `rmSync` en un helper.
    const porRuta = new Map(inventario().map((s) => [s.ruta, s]));

    for (const ruta of ["Scripts/sync_assets.js","Scripts/sync_bankmanager.js","Scripts/sync_scope.js","Scripts/sync_bankmanager_ui.js","Scripts/generate_korg_channel.js","Scripts/generate_host_model_id.js"]) {
      expect({ ruta, escribe: porRuta.get(ruta).escribe }).toEqual({ ruta, escribe: true });
    }

    for (const ruta of ["Scripts/sync_bankmanager.js","Scripts/sync_scope.js","Scripts/sync_bankmanager_ui.js","Scripts/generate_korg_channel.js","Scripts/generate_host_model_id.js"]) {
      expect({ ruta, via: porRuta.get(ruta).via }).toEqual({ ruta, via: "Scripts/syncSeguro.mjs" });
    }

    // escribe por las dos vias: delega los directorios y ademas replica los placeholders a mano. Por eso `via` es null aqui: no lo ha heredado de nadie.
    const propio = porRuta.get("Scripts/sync_assets.js");
    expect(propio.via).toBeNull();
    expect(propio.llamadas).toEqual(expect.arrayContaining(["mkdirSync","copyFileSync"]));

    // Y el helper en si es escritor por su cuenta, no por herencia.
    const helper = porRuta.get("Scripts/syncSeguro.mjs");
    expect(helper.via).toBeNull();
    expect(helper.llamadas).toContain('writeFileSync');
  });

  it('el inventario no se inventa escritores de la nada', () => {
    // El otro extremo del mismo problema: si `escribe` se detectase por cualquier
    // cosa, el test pasaria por haber marcado de escritor a media carpeta y estorbaria
    // mas de lo que vigila.
    const rutas = inventario().filter((s) => s.escribe).map((s) => s.ruta);

    for (const quieto of ["Scripts/registry_core.js"]) {
      expect(rutas).not.toContain(quieto);
    }
  });
});

describe('lo que escribe, pregunta antes', () => {
  it('todo el que escribe declara en su cabecera como NO escribir', () => {
    const sinDeclarar = escritores().filter((s) => s.declarados.length === 0);

    expect(
      sinDeclarar.map((s) => `${s.ruta} (escribe con ${s.llamadas.join(', ')})`),
    ).toEqual([]);
  });

  it('y el flag que declara existe en codigo: no es decoracion', () => {
    // Un flag en la cabecera que nadie parsea es peor que no tenerlo: el que lo lea
    // se creera que puede preguntar, y no habra nadie al otro lado.
    const deMentira = escritores().filter((s) => s.declarados.length > 0 && s.implementados.length === 0);

    expect(
      deMentira.map((s) => `${s.ruta} declara ${s.declarados.join('/')} pero no lo lee en ningun sitio`),
    ).toEqual([]);
  });
});

describe('el detector — para que los tests de arriba no puedan pasar por nada', () => {
  it('este detector VE a los escritores de verdad, no un conjunto vacio', () => {
    const rutas = inventario().filter((s) => s.escribe).map((s) => s.ruta);

    expect(rutas.length).toBeGreaterThan(0);
  });

  it('un comentario que MENCIONA una escritura no convierte al script en escritor', () => {
    const r = analizar([
      '/**',
      ' * Antes escribia con writeFileSync, ahora no.',
      ' */',
      "import fs from 'node:fs';",
      'fs.readFileSync(1);',
    ].join('\n'));

    expect(r.escribe).toBe(false);
    expect(r.llamadas).toEqual([]);
  });

  it('una escritura en codigo, con el flag en la cabecera y en el parseo', () => {
    const r = analizar([
      '#!/usr/bin/env node',
      '/**',
      ' * Uso: node x.js [--check]',
      ' */',
      "import fs from 'node:fs';",
      'const check = args.includes("--check");',
      'if (!check) fs.writeFileSync(dest, txt);',
    ].join('\n'));

    expect(r.escribe).toBe(true);
    expect(r.llamadas).toEqual(['writeFileSync']);
    expect(r.declarados).toEqual(['--check']);
    expect(r.implementados).toEqual(['--check']);
  });

  it('NEGATIVO: escribir sin declarar nada', () => {
    const r = analizar([
      '/**',
      ' * Un script de verdad.',
      ' */',
      "import fs from 'node:fs';",
      'fs.writeFileSync(dest, txt);',
    ].join('\n'));

    expect(r.escribe).toBe(true);
    expect(r.declarados).toEqual([]);
    expect(r.implementados).toEqual([]);
  });

  it('NEGATIVO: flag en la cabecera que no aparece en codigo', () => {
    const r = analizar([
      '/**',
      ' * Uso: node x.js [--check]',
      ' */',
      "import fs from 'node:fs';",
      'fs.writeFileSync(dest, txt);',
    ].join('\n'));

    expect(r.declarados).toEqual(['--check']);
    expect(r.implementados).toEqual([]);
  });

  it('los tres flags valen igual: el que pide el script es el que importa', () => {
    for (const flag of ['--check', '--dry-run', '--force']) {
      const r = analizar([
        ` * Uso: node x.js [${flag}]`,
        "import fs from 'node:fs';",
        `const modo = args.includes("${flag}");`,
        'fs.copyFileSync(a, b);',
      ].join('\n'));

      expect(r.implementados).toEqual([flag]);
    }
  });

  it('cuenta las tres familias de escritura, no solo writeFileSync', () => {
    const r = analizar([
      ' * [--check]',
      "import fs from 'node:fs';",
      'const a = fs.mkdirSync(dir);',
      'const b = fs.copyFileSync(x, y);',
      'fs.rmSync(z, { force: true });',
    ].join('\n'));

    expect(r.llamadas).toEqual(expect.arrayContaining(['mkdirSync', 'copyFileSync', 'rmSync']));
  });

  it('recoge los imports RELATIVOS y no los de paquete', () => {
    const r = analizar([
      ' * [--check]',
      "import fs from 'node:fs';",
      "import { x } from './escrituraSegura.mjs';",
      "import { y } from '../WebUI/js/registry.gen.js';",
      "import { z } from '@abdsynths/shared/components';",
    ].join('\n'));

    // El de paquete no se puede seguir sin leer node_modules, y ningun escritor de
    // estos repos escribe por ahi. Los dos relativos si, y son los que propagan la
    // condicion de escritor en el inventario.
    expect(r.importados).toEqual(['./escrituraSegura.mjs', '../WebUI/js/registry.gen.js']);
  });

  it('el `fs.` de delante NO cuenta como «otra cosa»: es justo el caso que hay que cazar', () => {
    const r = analizar([
      ' * [--check]',
      "import fs from 'node:fs';",
      'fs.copyFileSync(a, b);',
    ].join('\n'));

    expect(r.llamadas).toEqual(['copyFileSync']);
  });
});

describe('la exencion — con motivo escrito, no con una palabra', () => {

  it("solo hay una, y su motivo aguanta", () => {
    // No se fija un numero: si alguien anade un script eximido sale en rojo con SU nombre, y la pregunta que hay que responder es si el motivo aguanta. Fijar la lista aqui solo haria que el proximo se colase por lo contrario.
    const exentos = inventario().filter((s) => s.exencion);

    expect(exentos.map((s) => s.ruta)).toEqual(["Scripts/build_webui.js"]);

  });

  it('NEGATIVO: la exencion sin motivo NO exonera', () => {
    // Es la unica defensa contra un `SIN-GUARDIA:` pegado por prisa: el motivo tiene
    // que existir, no solo la palabra.
    const sinMotivo = analizar([
      '// SIN-GUARDIA:',
      "import fs from 'node:fs';",
      'fs.writeFileSync(dest, txt);',
    ].join('\n'));
    const conMotivo = analizar([
      '// SIN-GUARDIA: es un build, su contenido cambia en cada ejecucion.',
      "import fs from 'node:fs';",
      'fs.writeFileSync(dest, txt);',
    ].join('\n'));

    // La linea de la cabecera viene con su propio adorno de comentario (`// ` o
    // ` * `): sin quitarselo, un `SIN-GUARDIA:` a secas tendria «motivo» = `//`, que
    // es largo y no dice nada.
    expect({ hay: sinMotivo.exencion !== null, motivo: motivoDe(sinMotivo) })
      .toEqual({ hay: true, motivo: '' });
    expect({ motivo: motivoDe(conMotivo).length > 0 }).toEqual({ motivo: true });
  });
});
