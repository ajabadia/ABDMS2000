/**
 * ABDMS2000 — embedded Bank Manager (WebUI/abdbank): el **Fetch** trae la memoria
 * completa del plugin por el **bridge de software**, sin ningún puerto MIDI.
 *
 * Es el camino que usa el plugin con su propio Banco embebido: el destino conectado es
 * el synth que lo hospeda (transporte de software), no un equipo. Hasta ahora el Fetch
 * abortaba si el anfitrión reportaba «Hardware MIDI unavailable», de modo que el Banco
 * necesitaba un puerto MIDI —físico o **virtual**— para leer la memoria de un synth que
 * tiene delante y con el que ya habla por el bridge. Este test recorre el camino
 * completo y comprueba que no se manda ni una acción MIDI:
 *
 *   planBankFetch (bulk) → transport.fetchAll() → `bank.read` → `bank.data` (128 × 384 B)
 *                                              → summarizeBankFetch → 128/128 plazas
 *
 * El host al otro lado es `BridgeActions::handleBankRead`
 * (`Source/Plugin/BridgeActions.cpp`), que contesta los `SysExManager::BANK_SIZE`
 * programas con el bloque nativo v2 de 384 B y el `requestId` en eco; la forma de esa
 * respuesta está fijada en `Source/Tests/DSPCoreTests.cpp` [Test 19] y el contrato del
 * cable en `ABDBankManager/DOCS/BANK_MANAGER_TRANSPORT_MODEL.md` §6.2.
 *
 * Siblings: abdbankMs2000SingleDumpE2E.test.js (trama real de 297 B) y
 * abdbankSm002OwnFormatE2E.test.js (formato propio de 444 B).
 */

import { describe, it, expect, beforeAll } from 'vitest';
import { planBankFetch, summarizeBankFetch, midiBridgeBlocksFetch } from '../../abdbank/src/core/fetchFlow.js';
import { connectionRequiredMessage, connectionRequiredLabel } from '../../abdbank/src/core/transportCapabilities.js';
import { getModelContract } from '../../abdbank/src/contracts/modelContracts.js';

const PROGRAM_SIZE = 384;  // bloque nativo v2 (opaco para el Banco)
const BANK_SIZE = 128;     // las 8 bancas × 16 programas del MS2000

// El transporte importa el singleton del bridge (que necesita `window` al evaluarse),
// así que se carga después de montar el shim — igual que el resto de tests del bridge.
let createSoftwareTransport;

beforeAll(async () => {
  globalThis.window = {};
  globalThis.document = { contains: () => false };
  ({ createSoftwareTransport } = await import('../../abdbank/src/bridge/softwareTransport.js'));
});

/** Bloque nativo v2 con el nombre en 0x00, distinto en cada plaza. */
function nativeProgram(index) {
  const data = new Uint8Array(PROGRAM_SIZE);
  for (let i = 0; i < PROGRAM_SIZE; i++) data[i] = (i * 31 + index * 7 + 5) & 0xFF;
  data.set(new TextEncoder().encode(`MEM ${String(index).padStart(3, '0')}`.padEnd(12, ' ')), 0x00);
  return data;
}

/** `bank.data` tal como lo emite `BridgeActions::handleBankRead`. */
function bankData(requestId, count = BANK_SIZE) {
  return {
    requestId,
    slots: Array.from({ length: count }, (_, slot) => ({
      slot,
      name: `MEM ${String(slot).padStart(3, '0')}`,
      // base64 ESTÁNDAR (el `atob` del WebUI), no `MemoryBlock::toBase64Encoding`.
      payload: Buffer.from(nativeProgram(slot)).toString('base64'),
    })),
  };
}

/**
 * `parentBridge` falso: registra todo lo que el Banco manda al anfitrión y permite
 * contestar por acción. `onSend` deja al host auto-contestar (el caso del verbo por
 * slot, que es un ida y vuelta por programa).
 */
function makeBridge(onSend = null) {
  const handlers = new Map();
  const sent = [];
  const bridge = {
    sent,
    on(action, cb) {
      if (!handlers.has(action)) handlers.set(action, new Set());
      handlers.get(action).add(cb);
      return () => handlers.get(action)?.delete(cb);
    },
    send(action, data) {
      sent.push({ action, data });
      onSend?.(action, data, bridge);
    },
    emit(action, data) { handlers.get(action)?.forEach(cb => cb(data)); },
    actions() { return sent.map(message => message.action); },
    countSent(action) { return sent.filter(message => message.action === action).length; },
    lastSent(action) { return [...sent].reverse().find(message => message.action === action); },
  };
  return bridge;
}

/** El contrato del sistema nativo (`transport.software.systems: ['native']`). */
const contract = getModelContract('abd-sm002');

describe('Banco embebido — Fetch del plugin por el bridge, sin puerto MIDI', () => {
  it('el contrato y el transporte del host declaran el sistema nativo por bridge', () => {
    expect(contract.modelId).toBe('abd-sm002');
    expect(contract.bankCapacity).toBe(BANK_SIZE);
    expect(contract.transport?.software?.systems).toEqual(['native']);
    expect(contract.transport.software.read).toBe(true);
  });

  it('los avisos de la UI embebida no mandan a conectar un dispositivo MIDI', () => {
    // Auditoría 2026-09-15: ningún punto de la UI que dependa del destino software
    // exige un puerto MIDI. El último que quedaba era el guard del Fetch (arriba) y las
    // copias del aviso «conecta un dispositivo MIDI», que embebido nombra la salida
    // real (el synth anfitrión o hardware).
    expect(connectionRequiredMessage({ embedded: true })).toBe('Connect a destination first (host synth or MIDI hardware)');
    expect(connectionRequiredMessage({ embedded: true })).not.toContain('MIDI device');
    expect(connectionRequiredLabel({ embedded: true })).toBe('Connect a destination');
  });

  it('un anfitrión sin hardware MIDI no bloquea el Fetch de su propio synth', () => {
    // Lo que el host reporta cuando no encuentra dispositivos: antes esto abortaba el
    // Fetch aunque el destino fuera el propio plugin.
    expect(midiBridgeBlocksFetch({
      software: true,
      midiError: 'Hardware MIDI unavailable',
      midiListening: false,
    })).toBe(false);

    // Y por MIDI sí lo sigue bloqueando, que es el caso que evita 128 timeouts.
    expect(midiBridgeBlocksFetch({
      software: false,
      midiError: 'Hardware MIDI unavailable',
      midiListening: false,
    })).toBe(true);
  });

  it('trae la memoria completa del plugin con un solo bank.read, sin ninguna acción MIDI', async () => {
    const bridge = makeBridge();
    const transport = createSoftwareTransport({ contract, bridgeManager: bridge });

    const plan = planBankFetch({ transport, contract, software: true });
    expect(plan).toEqual({ mode: 'bulk', count: BANK_SIZE });

    const pending = transport.fetchAll();
    const request = bridge.lastSent('bank.read');
    expect(request.data.system).toBe('native');
    expect(typeof request.data.requestId).toBe('string');

    bridge.emit('bank.data', bankData(request.data.requestId));
    const patches = await pending;

    expect(patches).toHaveLength(BANK_SIZE);

    // Byte a byte las plazas que se comprueban (el resto por longitud: 128 × 384 B).
    for (const slot of [0, 1, 63, 64, 127]) {
      expect(patches[slot].slot).toBe(slot);
      expect(patches[slot].rawData.length).toBe(PROGRAM_SIZE);
      expect(patches[slot].rawData).toEqual(nativeProgram(slot));
      expect(patches[slot].name).toBe(`MEM ${String(slot).padStart(3, '0')}`);
    }
    expect(patches.reduce((total, patch) => total + patch.rawData.length, 0)).toBe(BANK_SIZE * PROGRAM_SIZE);

    // Un solo intercambio para el banco entero, y **ni una** acción del puente MIDI:
    // el Fetch de este destino no depende de ningún puerto, ni físico ni virtual.
    expect(bridge.countSent('bank.read')).toBe(1);
    expect(bridge.countSent('preset.read')).toBe(0);
    expect(bridge.actions()).toEqual(['bank.read']);
    expect(bridge.actions().some(action => action.startsWith('hardware.'))).toBe(false);
  });

  it('el contador de plazas cierra el Fetch en 128/128 con su direccionamiento', async () => {
    const bridge = makeBridge();
    const transport = createSoftwareTransport({ contract, bridgeManager: bridge });

    const pending = transport.fetchAll();
    bridge.emit('bank.data', bankData(bridge.lastSent('bank.read').data.requestId));
    const patches = await pending;

    const summary = summarizeBankFetch({
      patches,
      expected: contract.bankCapacity,
      getProgramAddress: contract.getProgramAddress,
    });
    expect(summary.level).toBe('success');
    expect(summary.received).toBe(BANK_SIZE);
    expect(summary.expected).toBe(BANK_SIZE);
    expect(summary.range).toBe(' (A.01–H.16)');
    expect(summary.message).toBe('Fetch completado — 128/128 plazas (A.01–H.16)');
  });

  it('si el plugin tuviera menos memoria cargada, el aviso dice cuántas llegaron', async () => {
    const bridge = makeBridge();
    const transport = createSoftwareTransport({ contract, bridgeManager: bridge });

    const pending = transport.fetchAll();
    bridge.emit('bank.data', bankData(bridge.lastSent('bank.read').data.requestId, 3));
    const patches = await pending;

    const summary = summarizeBankFetch({
      patches,
      expected: contract.bankCapacity,
      getProgramAddress: contract.getProgramAddress,
    });
    expect(summary.level).toBe('warning');
    expect(summary.message).toBe('Fetch completado — 3/128 plazas (A.01–A.03): el resto no llegó');
  });

  it('un host que no conozca bank.read cae al verbo por slot, también sin MIDI', async () => {
    // El auto-contestador del host: cada `preset.read` recibe su `preset.data`.
    const bridge = makeBridge((action, data, self) => {
      if (action === 'preset.read') {
        self.emit('preset.data', {
          requestId: data.requestId,
          slot: data.slot,
          name: `MEM ${String(data.slot).padStart(3, '0')}`,
          payload: Buffer.from(nativeProgram(data.slot)).toString('base64'),
        });
      }
    });
    const transport = createSoftwareTransport({ contract, bridgeManager: bridge });

    const pending = transport.fetchAll();
    // El host no implementa el verbo de banco: contesta el error que lo dice y el
    // transporte cae al verbo por slot en vez de dar el Fetch por perdido.
    bridge.emit('preset.error', {
      requestId: bridge.lastSent('bank.read').data.requestId,
      code: 'unsupported-action',
      message: 'unknown action',
    });

    const patches = await pending;
    expect(patches).toHaveLength(BANK_SIZE);
    expect(patches[7].rawData).toEqual(nativeProgram(7));
    expect(bridge.countSent('bank.read')).toBe(1);
    expect(bridge.countSent('preset.read')).toBe(BANK_SIZE);
    expect(bridge.actions().some(action => action.startsWith('hardware.'))).toBe(false);

    const summary = summarizeBankFetch({
      patches,
      expected: contract.bankCapacity,
      getProgramAddress: contract.getProgramAddress,
    });
    expect(summary.level).toBe('success');
    expect(summary.message).toBe('Fetch completado — 128/128 plazas (A.01–H.16)');
  });
});
