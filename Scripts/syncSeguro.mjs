// Scripts/syncSeguro.mjs
// LA SINCRONIZACION QUE BORRA EL DESTINO, SIN PODER PREGUNTAR.
//
// ─────────────────────────────────────────────────────────────────────────────
// EL PROBLEMA
//
// Los `sync_*.js` de este repo hacen, todos, lo mismo:
//
//     fs.rmSync(destino, { recursive: true, force: true });   // <- borra TODO
//     fs.cpSync(origen, destino, { recursive: true, ... });
//
// No es una copia: es una sustitucion. Y una sustitucion no distingue entre «el
// destino esta viejo» y «el destino tiene dentro el trabajo de tres dias». El
// `rmSync` se lleva las dos igual, y la unica señal de que algo habia ahi era el
// `git status` de despues —si alguien lo miraba.
//
// El caso peor no es el normal. Es que la copia se quede a medias: `cpSync` puede
// fallar por permisos, por un symlink roto, por un fichero bloqueado, y para
// entonces el destino ya no existe. Un fallo visible. Lo que no es visible es
// volver a ejecutar el comando a ciegas porque el primero «no funciono».
//
// ─────────────────────────────────────────────────────────────────────────────
// LO QUE HACE ESTE FICHERO
//
// Un `--check` que no reimplementa los filtros de cada script (que se olvidarian y
// darian un falso verde), sino que hace lo honesto: monta la sincronizacion ENTERA
// en un directorio temporal y la COMPARA con lo que hay en el destino. Si son
// iguales, no hay nada que hacer. Si no, dice exactamente que ficheros faltan,
// cuales sobrarian —que el `rmSync` borraria— y cuales cambiarian.
//
// ─────────────────────────────────────────────────────────────────────────────
// CRLF
//
// `core.autocrlf=true` pone CRLF en el fichero de trabajo y el repo guarda LF. Un
// check que comparase bytes crudos seria rojo para siempre y acabaria quitandose,
// que es peor que no tenerlo. Se normalizan los finales de linea ANTES de comparar,
// que es comparar el contenido y no la envoltura del checkout.

import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

/** Sin finales de linea: CRLF y LF son el mismo contenido en este repo. */
const sinCRLF = (buf) => {
  const texto = buf.toString('binary');
  return texto.includes('\r\n') ? texto.split('\r\n').join('\n') : texto;
};

/** Huella de un fichero ya leido, con los finales de linea normalizados. */
const huella = (buf) => crypto.createHash('sha256').update(sinCRLF(buf)).digest('hex');

/**
 * Mapa `ruta relativa -> huella` de un arbol. Los directorios vacios no cuentan:
 * no tienen contenido, y `cpSync` puede o no recrearlos segun la version de Node.
 *
 * `filtro` se aplica a LOS DOS arboles, y esa asimetria importa: si se filtrara solo
 * el esperado, todo lo que el filtro excluye pareceria «de mas» en el destino y el
 * check diria que se va a borrar lo que este sincronizador nunca toco. Es lo que
 * pasaria con `WebUI/images/brands/`, que es de un segundo sincronizador.
 */
function arbol(dir, base = dir, filtro = null) {
  const salida = new Map();
  for (const entrada of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entrada.name);
    if (filtro && !filtro(full)) continue;
    if (entrada.isDirectory()) {
      for (const [k, v] of arbol(full, base, filtro)) { salida.set(k, v); }
    } else if (entrada.isFile()) {
      salida.set(path.relative(base, full).replace(/\\/g, '/'), huella(fs.readFileSync(full)));
    }
  }
  return salida;
}

/**
 * Que diferencia hay entre dos arboles, en las tres direcciones.
 * @returns {{faltan: string[], sobran: string[], distintos: string[]}}
 */
export function compararArboles(esperado, actual, filtro = null) {
  const a = arbol(esperado, esperado, filtro);
  const b = existe(actual) ? arbol(actual, actual, filtro) : new Map();

  const faltan = [];   // estan en el origen y no en el destino: se anadiran
  const sobran = [];   // estan en el destino y no en el origen: el rmSync los BORRARIA
  const distintos = [];

  for (const [rel, h] of a) {
    if (!b.has(rel)) { faltan.push(rel); continue; }
    if (b.get(rel) !== h) { distintos.push(rel); }
  }
  for (const rel of b.keys()) {
    if (!a.has(rel)) { sobran.push(rel); }
  }

  const ordena = (x) => x.sort();
  return { faltan: ordena(faltan), sobran: ordena(sobran), distintos: ordena(distintos) };
}

const existe = (dir) => fs.existsSync(dir) && fs.statSync(dir).isDirectory();

/** Cuantas lineas de un listado caben sin que el reporte sea un muro. */
const TOPE = 12;
function listar(linea, cosas) {
  for (const c of cosas.slice(0, TOPE)) { console.log(`      ${linea} ${c}`); }
  if (cosas.length > TOPE) { console.log(`      ... y ${cosas.length - TOPE} mas`); }
}

/**
 * Sincroniza un directorio, o en `--check` dice si habria que hacerlo.
 *
 * @param {object} o
 * @param {string} o.etiqueta  lo que se imprime, para que el log se lea solo
 * @param {string} o.origen    directorio fuente
 * @param {string} o.destino   directorio a SUSTITUIR por completo
 * @param {(src: string) => boolean} [o.filtro]
 * @param {(dest: string) => void} [o.extra]  ajustes tras copiar (sync_assets)
 * @param {boolean} [o.check]  no escribe nada
 * @param {() => void} [o.ayuda] imprime el uso y sale
 * @returns {boolean} false = el check encontro diferencias, o la fuente falta
 */
export function sincronizarDir({ etiqueta, origen, destino, filtro, extra, check = false, ayuda }) {
  if (ayuda) { ayuda(); return true; }

  console.log(`Sincronizando ${etiqueta}`);
  console.log(`  Origen:  ${origen}`);
  console.log(`  Destino: ${destino}`);

  if (!fs.existsSync(origen)) {
    console.error(`ERROR: no se encontro la fuente: ${origen}`);
    return false;
  }

  const contar = (dir) => {
    let n = 0;
    for (const entrada of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, entrada.name);
      if (filtro && !filtro(full)) { continue; }
      n += entrada.isDirectory() ? contar(full) : 1;
    }
    return n;
  };
  const antes = contar(origen);
  console.log(`  Archivos en origen: ${antes}`);

  // ORIGEN VACIO. Es el fallo mas caro que puede tener una sustitucion: una ruta
  // mal escrita, un repositorio hermano sin clonar o un filtro que ya no deja pasar
  // nada, y el `rmSync` se lleva el destino entero sin que haya un solo byte de
  // origen que lo justifique. Un sincronizador que SUSTITUYA no tiene este caso
  // como escenario normal, asi que se corta aqui en vez de confiar en que nadie
  // apunta mal.
  if (antes === 0) {
    console.error(`ERROR: el origen ${origen} no tiene NINGUN fichero que copiar (con el filtro puesto).`);
    if (check) {
      console.error(`       Sincronizarlo ahora BORRARIA todo lo que hay en ${destino}.`);
    } else {
      console.error(`       No se ha tocado ${destino}.`);
    }
    return false;
  }

  const opciones = { recursive: true, dereference: true, filter: filtro };

  if (check) {
    // La sincronizacion se MONTA en un temporal y se compara. No se toca el
    // destino, ni se crea: un check que prepara el terreno se lleva por delante
    // justo lo que venia a mirar. El filtro va a la COMPARACION tambien, porque
    // lo que el filtro excluye es de otro sincronizador y no es de este.
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'sync-check-'));
    try {
      const espejo = path.join(tmp, 'espejo');
      fs.cpSync(origen, espejo, opciones);
      if (extra) { extra(espejo); }

      const d = compararArboles(espejo, destino, filtro);
      console.log(`  --check: ${destino} ${existe(destino) ? '' : '(no existe todavia)'}`);
      console.log(`    se anadirian : ${d.faltan.length}`);
      listar('+', d.faltan);
      console.log(`    se BORRARIAN : ${d.sobran.length}`);
      listar('-', d.sobran);
      console.log(`    cambiarian   : ${d.distintos.length}`);
      listar('~', d.distintos);

      const limpio = d.faltan.length === 0 && d.sobran.length === 0 && d.distintos.length === 0;
      if (limpio) {
        console.log(`  OK — ${destino} ya esta sincronizado.`);
      } else {
        console.error(`  DESFASADO — ejecuta el sincronizador SIN --check y commitea.`);
        if (d.sobran.length > 0) {
          console.error('  Ojo: lo que sobra se BORRA. Si hay trabajo ahi dentro, commitealo antes.');
        }
      }
      return limpio;
    } finally {
      fs.rmSync(tmp, { recursive: true, force: true });
    }
  }

  // La sincronizacion se MONTA primero en un temporal y luego se APLICA como diff.
  //
  // Antes esto era `rmSync(destino)` + `cpSync(origen, destino)`: una sustitucion
  // entera, que reescribe los 113 ficheros aunque 107 no hayan cambiado. Con
  // `core.autocrlf=true` eso tiene un efecto visible: el checkout deja el destino con
  // CRLF, el origen esta commiteado con LF, y al copiar cada fichero "sincronizado"
  // sin cambios aparece como modificado en `git status`. Diez logos que no habian
  // cambiado de un dia para otro, y un `git status` que no se lee.
  //
  // Aplicando solo lo que cambia, cada fichero conserva su envoltura salvo que su
  // contenido cambie de verdad, que es lo que un sincronizador debe hacer.
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'sync-aplicar-'));
  try {
    const espejo = path.join(tmp, 'espejo');
    fs.cpSync(origen, espejo, opciones);
    if (extra) { extra(espejo); }

    const d = compararArboles(espejo, destino, filtro);

    fs.mkdirSync(destino, { recursive: true });
    for (const rel of [...d.distintos, ...d.faltan]) {
      const src = path.join(espejo, rel);
      const dst = path.join(destino, rel);
      fs.mkdirSync(path.dirname(dst), { recursive: true });
      fs.copyFileSync(src, dst);
    }
    for (const rel of d.sobran) {
      fs.rmSync(path.join(destino, rel), { force: true });
    }

    console.log(`  Escritos: ${d.distintos.length} cambiados, ${d.faltan.length} nuevos`);
    console.log(`  Borrados:  ${d.sobran.length}${d.sobran.length > 0 ? '  (lo que sobraba; el check lo habia avisado)' : ''}`);
    console.log(`  Sin tocar: ${arbol(espejo, espejo, filtro).size - d.distintos.length - d.faltan.length} ya estaban al dia`);
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }

  const despues = contar(destino);
  console.log(`  Archivos copiados:  ${despues}`);
  if (despues !== antes) {
    console.warn(`  ADVERTENCIA: conteo difiere (origen ${antes} vs destino ${despues}).`);
  }
  return true;
}

/**
 * Copia una lista de ficheros encima, o en `--check` dice si alguno esta stale.
 *
 * A diferencia de `sincronizarDir`, aqui NO se borra nada: cada fichero se
 * sobrescribe. Que es menos peligroso, pero no inocuo —si el destino ha derivado
 * a mano, el overwrite se lo lleva sin preguntar— asi que el check tambien existe.
 *
 * @returns {boolean} false = algo esta stale, o falta un origen
 */
export function sincronizarFicheros({ etiqueta, origen, destino, ficheros, check = false, ayuda }) {
  if (ayuda) { ayuda(); return true; }

  console.log(`Sincronizando ${etiqueta}`);
  console.log(`  Origen:  ${origen}`);
  console.log(`  Destino: ${destino}`);

  if (!fs.existsSync(origen)) {
    console.error(`ERROR: no se encontro la fuente: ${origen}`);
    return false;
  }

  // PRIMERO se comprueba que esten TODOS los origenes. Si se copiara whilst se
  // recorre, un origen que faltara en el segundo fichero abortaria con el primero
  // ya sobrescrito: el destino a medias, que es justo el estado que no se
  // puede volver atras. O entra la sincronizacion entera o no entra ninguna.
  for (const nombre of ficheros) {
    const src = path.join(origen, nombre);
    if (!fs.existsSync(src)) {
      console.error(`ERROR: falta ${nombre} en el origen: ${src}`);
      console.error('       No se ha copiado nada: se comprueban todos los origenes antes.');
      return false;
    }
  }

  const stales = [];
  for (const nombre of ficheros) {
    const src = path.join(origen, nombre);
    const dst = path.join(destino, nombre);

    const origenBuf = fs.readFileSync(src);
    const alDia = fs.existsSync(dst) && huella(fs.readFileSync(dst)) === huella(origenBuf);

    if (check) {
      if (!alDia) { stales.push(nombre); }
      continue;
    }

    fs.mkdirSync(path.dirname(dst), { recursive: true });
    if (!alDia) { fs.copyFileSync(src, dst); }
  }

  if (check) {
    console.log(`  ${ficheros.length} fichero(s); stale: ${stales.length}`);
    listar('~', stales);
    if (stales.length === 0) {
      console.log('  OK — nada que sincronizar.');
      return true;
    }
    console.error('  DESFASADO — ejecuta el sincronizador SIN --check y commitea.');
    return false;
  }

  console.log(`  Ficheros copiados: ${ficheros.length}`);
  return true;
}

/**
 * Escribe un artefacto generado, o en `--check` dice si lo commiteado esta al dia.
 *
 * @param {object} o
 * @param {string} o.destino
 * @param {string} o.contenido
 * @param {(linea: string) => boolean} [o.ignorar]  lineas que NO se comparan
 * @param {boolean} [o.check]
 * @param {() => void} [o.ayuda]
 */
export function escribirGenerado({ destino, contenido, ignorar, check = false, ayuda }) {
  if (ayuda) { ayuda(); return true; }

  const actual = fs.existsSync(destino) ? fs.readFileSync(destino, 'utf8') : null;
  const resumen = (texto) => (ignorar ? texto.split('\n').filter((l) => !ignorar(l)) : texto.split('\n')).join('\n');

  if (check) {
    const alDia = actual !== null && resumen(actual) === resumen(contenido);
    if (alDia) {
      console.log(`OK - ${destino} esta al dia.`);
      return true;
    }
    if (actual === null) {
      console.error(`DESFASADO - ${destino} no existe.`);
    } else {
      console.error(`DESFASADO - ${destino} no es lo que saldria de generar.`);
      const lineas = resumen(contenido).split('\n');
      const actuales = resumen(actual).split('\n');
      const cambiadas = [];
      for (let i = 0; i < Math.max(lineas.length, actuales.length); i++) {
        if (lineas[i] !== actuales[i]) { cambiadas.push(`${i + 1}: ${String(actuales[i] ?? '(sin linea)').trim()}`); }
      }
      listar('|', cambiadas.slice(0, TOPE));
    }
    console.error('Regenera sin --check y commitea.');
    return false;
  }

  // Solo se escribe si el contenido cambia. Regenerar en cada build movia el
  // mtime del header y hacia que CMake lo recompilara sin motivo.
  if (actual === contenido) {
    console.log(`OK - ${destino} ya estaba al dia (no se toca).`);
    return true;
  }
  fs.mkdirSync(path.dirname(destino), { recursive: true });
  fs.writeFileSync(destino, contenido, 'utf8');
  console.log(`OK - ${destino} regenerado.`);
  return true;
}

/** `--check` y `--help` de los cuatro sincronizadores, con el mismo formato. */
export function leerFlags(descripcion) {
  const args = process.argv.slice(2);
  return {
    check: args.includes('--check'),
    ayuda: args.includes('--help') || args.includes('-h'),
    desconocido: args.find((a) => a.startsWith('-') && a !== '--check' && a !== '--help' && a !== '-h'),
    descripcion,
  };
}
