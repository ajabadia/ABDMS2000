/**
 * syncSeguro.test.js — el helper que hace que los `sync_*.js` puedan preguntar antes.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * POR QUE ESTE TEST NO EJECUTA LOS Scripts/sync_*.js DE VERDAD
 *
 * Porque no se puede. Los cuatro sincronizadores BORRAN el destino antes de copiar,
 * y sus destinos son `WebUI/images/`, `WebUI/abdbank/`, `WebUI/abdscope/` y
 * `WebUI/src/components/bank/`: directorios con trabajo dentro. Ejecutarlos para
 * comprobar que funcionan seria la Very cosa que se ha puesto aqui para evitar —
 * un test que se lleva por delante el estado que queria mirar.
 *
 * Asi que lo que se prueba es el HELPER, contra arboles de ficheros temporales que
 * son suyos. El contrato es el mismo en los dos casos: con `--check` no se escribe
 * nada y se dice que habria pasado; sin `--check` se sustituye el destino entero.
 * Lo unico que no se prueba aqui es que cada filtro de cada script este bien, y eso
 * lo comprueba el `--check` de cada uno contra el repo de verdad, que no escribe.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * LO QUE SE COMPRUEBA Y POR QUE CADA COSA
 *
 *   · que el check NO escriba, ni siquiera el directorio de destino;
 *   · que diga las tres cosas distintas: lo que falta, lo que SOBRA (que es lo que
 *     el `rmSync` borraria) y lo que cambia. Juntas, porque la que duele es la
 *     segunda, y es la unica que un `git status` posterior ya no enseña;
 *   · que la escritura de verdad siga SUSTITUYENDO, que es lo que `build.bat`
 *     espera: si se hiciera incremental, un fichero borrado en el origen se
 *     quedaria en el destino para siempre, y el dia que se suena eso es un bug
 *     dificil de ver;
 *   · que CRLF no haga el check rojo siempre, que es como un check deja de mirar.
 */
import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

import { compararArboles, sincronizarDir, sincronizarFicheros, escribirGenerado } from '../../Scripts/syncSeguro.mjs';

// Cada test crea SU raiz. No se usa `beforeEach` a proposito: varios ejecutores de
// esta suite (el harness de ABDSharedAssets, entre otros) no lo llaman por test, y
// con un unico directorio compartido todos los casos se pisan entre si y los rojos
// no son del codigo. Un test que necesita su estado limpio lo pide explicitamente.
const CREADOS = [];
process.on('exit', () => {
  for (const dir of CREADOS) {
    try { fs.rmSync(dir, { recursive: true, force: true }); } catch { /* el tmp del OS */ }
  }
});

let raiz = '';
function nuevaRaiz() {
  raiz = fs.mkdtempSync(path.join(os.tmpdir(), 'syncSeguro-'));
  CREADOS.push(raiz);
  return raiz;
}

const p = (...partes) => path.join(raiz, ...partes);

/** Crea `destino` con los ficheros indicados: ruta -> contenido. */
function montar(destino, mapa) {
  fs.mkdirSync(destino, { recursive: true });
  for (const [rel, contenido] of Object.entries(mapa)) {
    const full = path.join(destino, rel);
    fs.mkdirSync(path.dirname(full), { recursive: true });
    fs.writeFileSync(full, contenido);
  }
}

describe('compararArboles — las tres diferencias, por separado', () => {
  it('marca lo que falta, lo que sobra y lo que cambia', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'igual', 'b.txt': 'nuevo', 'c.txt': 'antes' });
    montar(p('destino'), { 'a.txt': 'igual', 'c.txt': 'despues', 'd.txt': 'sobra' });

    const d = compararArboles(p('origen'), p('destino'));

    expect(d.faltan).toEqual(['b.txt']);
    expect(d.distintos).toEqual(['c.txt']);
    // `d.txt` esta en el destino y no en el origen: el rmSync se lo lleva.
    expect(d.sobran).toEqual(['d.txt']);
  });

  it('un destino que NO existe es todo lo que falta, y no un error', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'x', 'sub/b.txt': 'y' });

    const d = compararArboles(p('origen'), p('destino'));

    expect(d.faltan).toEqual(['a.txt', 'sub/b.txt']);
    expect(d.sobran).toEqual([]);
    expect(d.distintos).toEqual([]);
  });

  it('CRLF y LF son el mismo contenido: core.autocrlf no puede ponerlo rojo siempre', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'una\ndos\n' });
    montar(p('destino'), { 'a.txt': 'una\r\ndos\r\n' });

    expect(compararArboles(p('origen'), p('destino'))).toEqual({ faltan: [], sobran: [], distintos: [] });
  });

  it('el filtro se aplica a LOS DOS arboles: lo que excluye no es de este sincronizador', () => {
    // El caso que hace falta de verdad: dos sincronizadores sobre el MISMO arbol.
    // `models/` no tiene `brands/`, pero `brands/` SI esta en el destino porque lo
    // puse el segundo. Si el filtro no se aplicase al destino, el check diria que
    // este sincronizador BORRA los doce logos — y despues el segundo los recrea.
    // Un check que avisa de una destruccion que no va a pasar es peor que no tener.
    montar(p('origen'), { 'logos/a.svg': 'x' });
    montar(p('destino'), { 'logos/a.svg': 'x', 'brands/b.svg': 'y' });

    const sinFiltro = compararArboles(p('origen'), p('destino'));
    const conFiltro = compararArboles(
      p('origen'), p('destino'),
      (src) => path.basename(src) !== 'brands',
    );

    expect(sinFiltro.sobran).toEqual(['brands/b.svg']);
    expect(conFiltro.sobran).toEqual([]);
    expect(conFiltro.faltan).toEqual([]);
  });

  it('un cambio de bytes de veras SI se ve, aunque no cambie el numero de lineas', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'un caracter distinto aqui\n' });
    montar(p('destino'), { 'a.txt': 'un caracter distinto ahi\n' });

    expect(compararArboles(p('origen'), p('destino')).distintos).toEqual(['a.txt']);
  });
});

describe('sincronizarDir — con --check no se escribe NADA', () => {
  it('no crea el destino, no lo borra y sale en falso', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'x' });

    const ok = sincronizarDir({
      etiqueta: 'prueba',
      origen: p('origen'),
      destino: p('destino'),
      check: true,
    });

    expect(ok).toBe(false);
    // Lo importante: el destino sigue sin existir. Un check que crea el
    // directorio que luego va a llenar se lleva por delante lo que venia a mirar.
    expect(fs.existsSync(p('destino'))).toBe(false);
  });

  it('dice verde cuando el destino ya es lo que se copiaria', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'x', 'sub/b.txt': 'y' });
    montar(p('destino'), { 'a.txt': 'x', 'sub/b.txt': 'y' });

    const ok = sincronizarDir({
      etiqueta: 'prueba', origen: p('origen'), destino: p('destino'), check: true,
    });

    expect(ok).toBe(true);
  });

  it('no toca el destino aunque este DESFASADO', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'nuevo' });
    montar(p('destino'), { 'a.txt': 'viejo', 'se-borra.txt': 'trabajo' });

    const ok = sincronizarDir({
      etiqueta: 'prueba', origen: p('origen'), destino: p('destino'), check: true,
    });

    expect(ok).toBe(false);
    expect(fs.readFileSync(p('destino/a.txt'), 'utf8')).toBe('viejo');
    expect(fs.existsSync(p('destino/se-borra.txt'))).toBe(true);
  });

  it('NEGATIVO: un origen vacio hace rojo el check, no verde', () => {
    // El otro falso verde posible: si `origen` no tuviera nada, el espejo vacio
    // pareceria «todo lo que hay de mas se va a borrar», y un script con el origen
    // mal apuntado se llevaria el destino entero con luz verde.
    montar(p('origen'), {});
    montar(p('destino'), { 'importante.txt': 'trabajo' });

    const ok = sincronizarDir({
      etiqueta: 'prueba', origen: p('origen'), destino: p('destino'), check: true,
    });

    expect(ok).toBe(false);
  });

  it('aplica el filtro en el check tambien: si no, el check mintiria', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'x', 'ignorar.txt': 'no' });
    montar(p('destino'), { 'a.txt': 'x' });

    const ok = sincronizarDir({
      etiqueta: 'prueba',
      origen: p('origen'),
      destino: p('destino'),
      filtro: (src) => path.basename(src) !== 'ignorar.txt',
      check: true,
    });

    expect(ok).toBe(true);
  });

  it('el `extra` se aplica al espejo del check, y su efecto tambien se compara', () => {
    nuevaRaiz();
    montar(p('origen'), { 'thumbs/p.svg': '<svg/>' });
    montar(p('destino'), { 'thumbs/p.svg': '<svg/>' });

    let donde = '';
    const extra = (dest) => {
      donde = dest;
      fs.mkdirSync(path.join(dest, 'models', 'thumbs'), { recursive: true });
      fs.copyFileSync(path.join(dest, 'thumbs/p.svg'), path.join(dest, 'models/thumbs/p.svg'));
    };

    // El `extra` crea `models/thumbs/p.svg` en el espejo. El destino no lo tiene,
    // asi que el check dice DESFASADO — que es lo correcto: sincronizar de verdad
    // lo crearia. Lo que no puede es crear ese fichero para que el check lo vea.
    const antesDeSync = sincronizarDir({
      etiqueta: 'prueba', origen: p('origen'), destino: p('destino'), extra, check: true,
    });

    expect(antesDeSync).toBe(false);
    expect(donde).not.toBe(p('destino'));
    expect(fs.existsSync(p('destino/models'))).toBe(false);

    // Y una vez sincronizado de verdad, el check sale verde: el ciclo converge.
    sincronizarDir({ etiqueta: 'prueba', origen: p('origen'), destino: p('destino'), extra });
    const despuesDeSync = sincronizarDir({
      etiqueta: 'prueba', origen: p('origen'), destino: p('destino'), extra, check: true,
    });

    expect(despuesDeSync).toBe(true);
  });
});

describe('sincronizarDir — sin --check sigue SUSTITUYENDO el conjunto, que es el contrato', () => {
  it('borra del destino lo que no este en el origen', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'nuevo' });
    montar(p('destino'), { 'a.txt': 'viejo', 'se-borra.txt': 'trabajo' });

    const ok = sincronizarDir({
      etiqueta: 'prueba', origen: p('origen'), destino: p('destino'),
    });

    expect(ok).toBe(true);
    expect(fs.readFileSync(p('destino/a.txt'), 'utf8')).toBe('nuevo');
    // Aqui es donde se pierde trabajo si nadie lo mire antes: por eso el --check.
    expect(fs.existsSync(p('destino/se-borra.txt'))).toBe(false);
  });

  it('y despues de sincronizar, el check sale verde', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': 'x', 'sub/b.txt': 'y' });
    montar(p('destino'), { 'c.txt': 'obsoleto' });

    sincronizarDir({ etiqueta: 'prueba', origen: p('origen'), destino: p('destino') });
    const ok = sincronizarDir({
      etiqueta: 'prueba', origen: p('origen'), destino: p('destino'), check: true,
    });

    expect(ok).toBe(true);
  });

  it('origen inexistente: falla, y NO borra el destino que ya habia', () => {
    nuevaRaiz();
    montar(p('destino'), { 'a.txt': 'importante' });

    const ok = sincronizarDir({ etiqueta: 'prueba', origen: p('no-existe'), destino: p('destino') });

    expect(ok).toBe(false);
    // El orden importa: si se borrara primero y luego se comprobara la fuente,
    // un origen mal escrito seria «borrar el destino y no copiar nada».
    expect(fs.existsSync(p('destino/a.txt'))).toBe(true);
  });
});

describe('sincronizarDir — no reescribe lo que no ha cambiado', () => {
  // El fallo que motiva esto: `sincronizarDir` era `rmSync(destino)` +
  // `cpSync(origen)`, que reescribe los 113 ficheros aunque 107 no cambien. Con
  // `core.autocrlf=true` el checkout deja el destino con CRLF y el origen esta
  // commiteado con LF, asi que una sincronizacion que no cambia nada marca diez
  // logos como modificados. El `git status` que sale de ahi no lo lee nadie.

  it('un fichero identico conserva su mtime: no se ha tocado', () => {
    nuevaRaiz();
    montar(p('origen'), { 'igual.txt': 'sin cambios', 'cambia.txt': 'v1' });
    montar(p('destino'), { 'igual.txt': 'sin cambios', 'cambia.txt': 'v0' });

    // El mtime es la unica prueba de que un fichero no se ha reescrito: su
    // contenido es el mismo antes y despues, asi que comparar bytes no diria nada.
    // Se fija a una fecha antigua y reconocible: si se reescribiera, el mtime
    // pasaria a "ahora" y no habria forma de distinguirlo de una copia legitima.
    const FIJO = 1_000_000_000_000;
    fs.utimesSync(p('destino/igual.txt'), FIJO / 1000, FIJO / 1000);

    sincronizarDir({ etiqueta: 'prueba', origen: p('origen'), destino: p('destino') });

    expect({ igual: fs.statSync(p('destino/igual.txt')).mtimeMs, cambia: fs.statSync(p('destino/cambia.txt')).mtimeMs > FIJO })
      .toEqual({ igual: FIJO, cambia: true });
    // Y el que si cambia, cambia de verdad.
    expect(fs.readFileSync(p('destino/cambia.txt'), 'utf8')).toBe('v1');
  });

  it('un CRLF del destino sobrevive si el origen solo se diferencia en eso', () => {
    nuevaRaiz();
    montar(p('origen'), { 'texto.txt': 'una\ndos\n' });
    montar(p('destino'), { 'texto.txt': 'una\r\ndos\r\n' });

    sincronizarDir({ etiqueta: 'prueba', origen: p('origen'), destino: p('destino') });

    // Si se reescribiera, el destino pasaria a LF. Que conserve el CRLF es
    // exactamente lo que evita que `git status` ensucie el arbol.
    expect(fs.readFileSync(p('destino/texto.txt'), 'utf8')).toBe('una\r\ndos\r\n');
  });

  it('pero si ademas cambia el contenido, se escribe con el EOL del origen', () => {
    nuevaRaiz();
    montar(p('origen'), { 'texto.txt': 'una\nDISTINTO\n' });
    montar(p('destino'), { 'texto.txt': 'una\r\ndos\r\n' });

    sincronizarDir({ etiqueta: 'prueba', origen: p('origen'), destino: p('destino') });

    expect(fs.readFileSync(p('destino/texto.txt'), 'utf8')).toBe('una\nDISTINTO\n');
  });

  it('sigue añadiendo lo que falta y borrando lo que sobra', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': '1', 'nuevo.txt': '2' });
    montar(p('destino'), { 'a.txt': '0', 'viejo.txt': '3' });

    sincronizarDir({ etiqueta: 'prueba', origen: p('origen'), destino: p('destino') });

    expect({
      a: fs.readFileSync(p('destino/a.txt'), 'utf8'),
      nuevo: fs.existsSync(p('destino/nuevo.txt')),
      viejo: fs.existsSync(p('destino/viejo.txt')),
    }).toEqual({ a: '1', nuevo: true, viejo: false });
  });

  it('si el destino no existe, se crea entero', () => {
    nuevaRaiz();
    montar(p('origen'), { 'a.txt': '1', 'sub/b.txt': '2' });

    sincronizarDir({ etiqueta: 'prueba', origen: p('origen'), destino: p('destino') });

    expect({
      a: fs.readFileSync(p('destino/a.txt'), 'utf8'),
      b: fs.readFileSync(p('destino/sub/b.txt'), 'utf8'),
    }).toEqual({ a: '1', b: '2' });
  });
});

describe('sincronizarFicheros — sobrescribe sin borrar', () => {
  const origen = () => {
    montar(p('origen'), { 'X.js': 'nuevo', 'X.css': 'igual' });
    return p('origen');
  };

  it('--check dice cuales estan stale y no copia', () => {
    nuevaRaiz();
    montar(p('destino'), { 'X.js': 'viejo', 'X.css': 'igual' });

    const ok = sincronizarFicheros({
      etiqueta: 'prueba', origen: origen(), destino: p('destino'),
      ficheros: ['X.js', 'X.css'], check: true,
    });

    expect(ok).toBe(false);
    expect(fs.readFileSync(p('destino/X.js'), 'utf8')).toBe('viejo');
  });

  it('al copiar, deja el destino igual al origen', () => {
    nuevaRaiz();
    montar(p('destino'), { 'X.js': 'viejo', 'X.css': 'igual' });

    const ok = sincronizarFicheros({
      etiqueta: 'prueba', origen: origen(), destino: p('destino'),
      ficheros: ['X.js', 'X.css'],
    });

    expect(ok).toBe(true);
    expect(fs.readFileSync(p('destino/X.js'), 'utf8')).toBe('nuevo');
  });

  it('falta un fichero en el origen: falla y NO copia nada de los otros', () => {
    nuevaRaiz();
    montar(p('destino'), { 'X.js': 'intacto' });

    const ok = sincronizarFicheros({
      etiqueta: 'prueba', origen: origen(), destino: p('destino'),
      ficheros: ['X.js', 'NO_EXISTE.js'],
    });

    // X.js va el primero y SI se podria haber copiado. Que no se copie es el punto:
    // un origen a medias deja el destino a medias, y desde ahi no se vuelve atras.
    expect(ok).toBe(false);
    expect(fs.readFileSync(p('destino/X.js'), 'utf8')).toBe('intacto');
  });
});

describe('escribirGenerado — el header que se regenera en cada build', () => {
  it('--check sale verde con el mismo contenido y en rojo con otro', () => {
    nuevaRaiz();
    montar(p('destino'), { 'X.gen.h': 'linea1\nlinea2\n' });

    const igual = escribirGenerado({ destino: p('destino/X.gen.h'), contenido: 'linea1\nlinea2\n', check: true });
    const otro = escribirGenerado({ destino: p('destino/X.gen.h'), contenido: 'linea1\nlinea3\n', check: true });

    expect(igual).toBe(true);
    expect(otro).toBe(false);
  });

  it('ignora las lineas que se le digan: el sello de git cambia en cada commit', () => {
    nuevaRaiz();
    montar(p('destino'), { 'X.gen.h': 'modelId = "sm002"\nkHostBuildRevision = "aaa111"\n' });
    const contenido = 'modelId = "sm002"\nkHostBuildRevision = "bbb222"\n';

    // Sin ignorar, seria rojo: dos SHAs distintos son el mismo contrato.
    expect(escribirGenerado({ destino: p('destino/X.gen.h'), contenido, check: true })).toBe(false);
    expect(escribirGenerado({
      destino: p('destino/X.gen.h'),
      contenido,
      ignorar: (l) => l.includes('kHostBuildRevision'),
      check: true,
    })).toBe(true);
  });

  it('pero lo que NO se le dice que ignore, sigue mirandolo', () => {
    nuevaRaiz();
    montar(p('destino'), { 'X.gen.h': 'modelId = "sm002"\nbridge = 4\n' });
    const contenido = 'modelId = "otra-cosa"\nbridge = 4\n';

    expect(escribirGenerado({
      destino: p('destino/X.gen.h'),
      contenido,
      ignorar: (l) => l.includes('kHostBuildRevision'),
      check: true,
    })).toBe(false);
  });

  it('sin --check, no toca el fichero si el contenido no cambio', () => {
    nuevaRaiz();
    montar(p('destino'), { 'X.gen.h': 'igual\n' });
    const antes = fs.statSync(p('destino/X.gen.h')).mtimeMs;

    escribirGenerado({ destino: p('destino/X.gen.h'), contenido: 'igual\n' });

    // El mtime intacto es lo que evita que CMake recompile el header entero.
    expect(fs.statSync(p('destino/X.gen.h')).mtimeMs).toBe(antes);
  });

  it('destino inexistente en --check: rojo, no verde', () => {
    nuevaRaiz();
    expect(escribirGenerado({ destino: p('no-existe/X.gen.h'), contenido: 'x\n', check: true })).toBe(false);
    expect(fs.existsSync(p('no-existe/X.gen.h'))).toBe(false);
  });
});
