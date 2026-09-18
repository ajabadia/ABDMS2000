/**
 * ABDMS2000 — embedded Bank Manager (WebUI/abdbank): **Send y Bulk** al plugin por el
 * **bridge de software**, con la **confirmación de escritura**.
 *
 * Es la dirección contraria al Fetch (`abdbankSm002BridgeFetchE2E.test.js`): el Banco
 * manda el preset y el host contesta. El cable son los verbos de
 * `SoftwarePresetProtocol.h` y el lado C++ es
 * `BridgeActions::handlePresetWrite` / `handleBankWrite`
 * (`Source/Plugin/BridgeActions.cpp`), con el payload en base64 **estándar**
 * (`juce::Base64::toBase64`, no `MemoryBlock::toBase64Encoding`) y el `requestId` en eco.
 *
 * Lo que se fija aquí:
 *
 *  - el `preset.write` de un patch (slot, nombre, 384 B nativos) y el `bank.write` del
 *    lote entero, con **cero** acciones `hardware.*`: escribir al synth anfitrión no
 *    necesita ningún puerto MIDI;
 *  - la **confirmación**: el transporte declara `softsynth`/`confirm`, la escritura **no**
 *    se da por buena hasta que el host contesta `preset.written`, y un rechazo o un
 *    silencio salen como error en vez de como «enviado». Sin esto la UI anunciaba
 *    «enviado» lo que solo había salido — y la rama de confirmación de
 *    `handleMidiSendPatch` era código muerto, porque el transporte no declaraba las dos
 *    capacidades que la activan (`core/transportCapabilities.js` → `awaitsWriteConfirmation`).
 *
 * Contrato del cable: `ABDBankManager/DOCS/BANK_MANAGER_TRANSPORT_MODEL.md` §6.2/§7.2.
 */

import { describe, it, expect, beforeAll } from 'vitest';
import { awaitsWriteConfirmation } from '../../abdbank/src/core/transportCapabilities.js';
import { getModelContract } from '../../abdbank/src/contracts/modelContracts.js';

const PROGRAM_SIZE = 384;  // bloque nativo v2 (v1 = 128 B se sigue aceptando al leer)
const BANK_SIZE = 128;
const TIMEOUT_MS = 80;     // el contrato real usa 3000; en un test no se espera eso

let createSoftwareTransport;

beforeAll(async () => {
  globalThis.window = {};
  globalThis.document = { contains: () => false };
  ({ createSoftwareTransport } = await import('../../abdbank/src/bridge/softwareTransport.js'));
});

/** Bloque nativo v2 con el nombre en 0x00, distinto en cada plaza. */
function nativeProgram(index) {
  const data = new Uint8Array(PROGRAM_SIZE);
  for (let i = 0; i < PROGRAM_SIZE; i++) data[i] = (i * 17 + index * 11 + 3) & 0xFF;
  data.set(new TextEncoder().encode(`SEND ${String(index).padStart(3, '0')}`.padEnd(12, ' ')), 0x00);
  return data;
}

/** `parentBridge` falso: registra lo que sale y deja contestar al host. */
function makeBridge() {
  const handlers = new Map();
  const sent = [];
  const bridge = {
    sent,
    on(action, cb) {
      if (!handlers.has(action)) handlers.set(action, new Set());
      handlers.get(action).add(cb);
      return () => handlers.get(action)?.delete(cb);
    },
    send(action, data) { sent.push({ action, data }); },
    emit(action, data) { handlers.get(action)?.forEach(cb => cb(data)); },
    actions() { return sent.map(message => message.action); },
    countSent(action) { return sent.filter(message => message.action === action).length; },
    lastSent(action) { return [...sent].reverse().find(message => message.action === action); },
  };
  return bridge;
}

const contract = getModelContract('abd-sm002');

/** Transporte del host sobre un bridge falso, con timeout corto. */
function transportOn(bridge) {
  return createSoftwareTransport({ contract, bridgeManager: bridge, timeoutMs: TIMEOUT_MS });
}

describe('Banco embebido — Send/Bulk al plugin por el bridge', () => {
  it('el transporte del host declara que puede confirmar una escritura', () => {
    const transport = transportOn(makeBridge());

    expect(transport.capabilities.softsynth).toBe(true);
    expect(transport.capabilities.confirm).toBe(true);
    expect(awaitsWriteConfirmation(transport.capabilities)).toBe(true);
  });

  it('manda un patch con preset.write: slot, nombre y los 384 B en base64 estándar', async () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);
    const program = nativeProgram(7);

    const pending = transport.sendPatch({ rawData: program, name: 'SEND 007' }, 7, 1, { confirm: true });

    const request = bridge.lastSent('preset.write');
    expect(request.data.system).toBe('native');
    expect(request.data.slot).toBe(7);
    expect(request.data.name).toBe('SEND 007');
    expect(typeof request.data.requestId).toBe('string');
    expect(request.data.audition).toBeUndefined();

    // base64 estándar y byte a byte el bloque nativo (lo que decodifica `decodePayload`).
    const decoded = new Uint8Array(Buffer.from(request.data.payload, 'base64'));
    expect(decoded.length).toBe(PROGRAM_SIZE);
    expect(decoded).toEqual(program);

    bridge.emit('preset.written', { requestId: request.data.requestId, slot: 7, name: 'SEND 007' });
    await expect(pending).resolves.toMatchObject({ slot: 7, name: 'SEND 007' });

    // Escribir al synth anfitrión no necesita MIDI: ni una acción del puente de hardware.
    expect(bridge.actions()).toEqual(['preset.write']);
  });

  it('la confirmación espera de verdad: no resuelve hasta que el host contesta', async () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);

    const pending = transport.sendPatch({ rawData: nativeProgram(0) }, 0, 1, { confirm: true });
    const { requestId } = bridge.lastSent('preset.write').data;

    let settled = false;
    const observed = pending.then(() => { settled = true; });
    await Promise.resolve();
    expect(settled).toBe(false); // devolver «enviado» aquí sería mentir

    bridge.emit('preset.written', { requestId });
    await observed;
    expect(settled).toBe(true);
  });

  it('si el host no puede guardarlo, la promesa rechaza con su motivo', async () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);

    const pending = transport.sendPatch({ rawData: nativeProgram(1) }, 1, 1, { confirm: true });
    bridge.emit('preset.error', {
      requestId: bridge.lastSent('preset.write').data.requestId,
      code: 'invalid-slot',
      message: 'slot 1 is outside the synth memory (0..127)',
    });

    await expect(pending).rejects.toThrow('slot 1 is outside the synth memory');
  });

  it('sin respuesta, la confirmación caduca en vez de quedarse colgada', async () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);

    const pending = transport.sendPatch({ rawData: nativeProgram(2) }, 2, 1, { confirm: true });
    await expect(pending).rejects.toThrow(new RegExp(`preset\\.write timed out after ${TIMEOUT_MS} ms`));
  });

  it('manda el banco entero con un solo bank.write que el host acusa', async () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);

    const patches = Array.from({ length: BANK_SIZE }, (_, slot) => ({ rawData: nativeProgram(slot), slot }));
    const pending = transport.sendBulk(patches, 1, { confirm: true });

    const request = bridge.lastSent('bank.write');
    expect(request.data.system).toBe('native');
    expect(request.data.slots).toHaveLength(BANK_SIZE);
    expect(new Uint8Array(Buffer.from(request.data.slots[42].payload, 'base64'))).toEqual(nativeProgram(42));
    expect(request.data.slots.reduce((total, slot) => total + Buffer.from(slot.payload, 'base64').length, 0))
      .toBe(BANK_SIZE * PROGRAM_SIZE);

    bridge.emit('bank.written', { requestId: request.data.requestId, count: BANK_SIZE });
    await expect(pending).resolves.toMatchObject({ count: BANK_SIZE });

    expect(bridge.countSent('bank.write')).toBe(1);
    expect(bridge.countSent('preset.write')).toBe(0);
    expect(bridge.actions().some(action => action.startsWith('hardware.'))).toBe(false);
  });

  it('un host sin bank.write recibe el lote slot a slot, y el acuse espera a los 128', async () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);

    const patches = Array.from({ length: BANK_SIZE }, (_, slot) => ({ rawData: nativeProgram(slot), slot }));
    const pending = transport.sendBulk(patches, 1, { confirm: true });

    // El host no conoce el verbo de lote.
    bridge.emit('preset.error', {
      requestId: bridge.lastSent('bank.write').data.requestId,
      code: 'unsupported-action',
      message: 'unknown action bank.write',
    });
    await Promise.resolve();

    const writes = bridge.sent.filter(message => message.action === 'preset.write');
    expect(writes).toHaveLength(BANK_SIZE);
    expect(new Uint8Array(Buffer.from(writes[3].data.payload, 'base64'))).toEqual(nativeProgram(3));

    // Se acusa todo, menos la cuarta plaza: el lote no puede resolverse como enviado.
    for (const [index, write] of writes.entries()) {
      if (index === 3) continue;
      bridge.emit('preset.written', { requestId: write.data.requestId, slot: write.data.slot });
    }
    let settled = false;
    const observed = pending.then(() => { settled = true; });
    await Promise.resolve();
    expect(settled).toBe(false);

    bridge.emit('preset.written', { requestId: writes[3].data.requestId, slot: 3 });
    await observed;
    expect(settled).toBe(true);
    expect(bridge.actions().some(action => action.startsWith('hardware.'))).toBe(false);
  });

  it('audition viaja como tal: el host aplica el buffer de edición en vez de escribir memoria', async () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);

    const pending = transport.sendPatch({ rawData: nativeProgram(9), name: 'SEND 009', audition: true }, 9, 1, { confirm: true });

    expect(bridge.lastSent('preset.write').data.audition).toBe(true);
    bridge.emit('preset.written', { requestId: bridge.lastSent('preset.write').data.requestId, slot: 9, audition: true });
    await expect(pending).resolves.toMatchObject({ audition: true });
  });

  it('sin confirm el envío sigue siendo dispara y olvida (no cambia el flujo actual)', () => {
    const bridge = makeBridge();
    const transport = transportOn(bridge);

    const result = transport.sendPatch({ rawData: nativeProgram(0) }, 0, 1);

    expect(typeof result).toBe('number');
    expect(bridge.countSent('preset.write')).toBe(1);
  });
});
