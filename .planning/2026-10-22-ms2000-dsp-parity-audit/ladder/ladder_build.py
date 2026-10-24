# -*- coding: utf-8 -*-
"""
Escalera de aislamiento MS2000 (2026-10-22).

Genera 8 programas hardware de 254 bytes a partir de la plantilla INIT exacta
extraida de MS2000HardwareProgram.h. Cada rung aísla UN subsistema; los rungs
1..8 se escriben en el slot USER #2 del ROM aislado del emulador y, a la vez,
parchean los arrays de fábrica contiguos (.rdata) de una COPIA nueva de
CalibrationTests.exe (CalibrationTests_diag.exe) para que el lado ABD del
comparador existente renderice EXACTAMENTE el mismo programa.

Uso:
  python ladder_build.py            # genera rungs/ + patch del exe + ROMs
"""
import hashlib
import os
import re
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
EXE_ORIG = os.path.join(REPO, 'build', 'Release', 'CalibrationTests.exe')
EXE_DIAG = os.path.join(REPO, 'build', 'Release', 'CalibrationTests_diag.exe')
EMU_EXE = r"D:\desarrollos\ABDSynths\_RESOURCES\ms2000\ms2000r-emu-main\ms2000r-emu-main\build_vst\Release\ms2000_emulator.exe"
EMU_HOME_TPL = r"C:\Users\ajaba\AppData\Local\Temp\abdms2k-render-final\home-original-user"

T1 = 0x26
NAME = 0x00

FIELD_OFF = {
    'MIDI_CH': 0, 'FLAGS': 1, 'UNISON_DETUNE': 2, 'TUNE': 3, 'BEND_RANGE': 4,
    'TRANSPOSE': 5, 'VIBRATO': 6, 'OSC1_WAVE': 7, 'OSC1_CTRL1': 8,
    'OSC1_CTRL2': 9, 'OSC1_DWGS': 10, 'OSC2_FLAGS': 12, 'OSC2_SEMITONE': 13,
    'OSC2_TUNE': 14, 'PORTAMENTO': 15, 'MIX_OSC1': 16, 'MIX_OSC2': 17,
    'MIX_NOISE': 18, 'FILTER_TYPE': 19, 'CUTOFF': 20, 'RESONANCE': 21,
    'FILTER_EG1_INT': 22, 'FILTER_VELO': 23, 'FILTER_KEYTRACK': 24,
    'AMP_LEVEL': 25, 'AMP_PAN': 26, 'AMP_FLAGS': 27, 'AMP_VELO': 28,
    'AMP_KEYTRACK': 29, 'EG1_A': 30, 'EG1_D': 31, 'EG1_S': 32, 'EG1_R': 33,
    'EG2_A': 34, 'EG2_D': 35, 'EG2_S': 36, 'EG2_R': 37, 'LFO1_FLAGS': 38,
    'LFO1_FREQ': 39, 'LFO1_FLAGS2': 40, 'LFO2_FLAGS': 41, 'LFO2_FREQ': 42,
    'LFO2_FLAGS2': 43, 'PATCH1': 44, 'PATCH1_INT': 45, 'PATCH2': 46,
    'PATCH2_INT': 47, 'PATCH3': 48, 'PATCH3_INT': 49, 'PATCH4': 50,
    'PATCH4_INT': 51, 'SEQ_FLAGS': 52, 'SEQ_FLAGS2': 53,
}

R1, R2 = 0x92, 0xFD  # timbre2 offsets en el bloque de 254


def load_init():
    p = os.path.join(HERE, 'init_base.bin')
    b = bytearray(open(p, 'rb').read())
    if len(b) != 254:
        raise SystemExit(f"init_base.bin tiene {len(b)} bytes")
    return b


def set_name(b, text):
    t = (text + ' ' * 12)[:12]
    for i, ch in enumerate(t):
        b[NAME + i] = ord(ch)


def setf(b, field, value, timbre=1):
    off = (T1 if timbre == 1 else R1) + FIELD_OFF[field]
    b[off] = value & 0xFF


def build_rungs():
    rungs = []

    # --- R1: OSC -- oscilador solo, filtro abierto, amp plano, nada de FX ---
    b = load_init()
    set_name(b, 'DIAG R1 OSC ')
    setf(b, 'VIBRATO', 64)          # vibrato off
    setf(b, 'DELAY_DEPTH', 0) if 'DELAY_DEPTH' in FIELD_OFF else None
    # delay/modfx: bytes globales 0x13..0x19
    b[0x13] = 0x00                  # delay flags
    b[0x14] = 0x28                  # delay time (se reusa en R8)
    b[0x15] = 0x00                  # delay depth 0 -> delay seco
    b[0x16] = 0x00                  # delay type
    b[0x17] = 0x28                  # modfx speed
    b[0x18] = 0x00                  # modfx depth 0 -> apagado
    b[0x19] = 0x00                  # modfx type chorus
    # osc1 saw, sin osc2, sin noise
    setf(b, 'OSC1_WAVE', 0)         # saw
    setf(b, 'MIX_OSC1', 127)
    setf(b, 'MIX_OSC2', 0)
    setf(b, 'MIX_NOISE', 0)
    setf(b, 'OSC2_FLAGS', 0)
    # filtro LPF abierto sin reso
    setf(b, 'FILTER_TYPE', 0)       # LPF24
    setf(b, 'CUTOFF', 127)
    setf(b, 'RESONANCE', 0)
    setf(b, 'FILTER_EG1_INT', 64)   # +0
    setf(b, 'FILTER_VELO', 64)
    setf(b, 'FILTER_KEYTRACK', 64)
    # amp plano
    setf(b, 'AMP_LEVEL', 127)
    setf(b, 'AMP_PAN', 64)
    setf(b, 'AMP_FLAGS', 0)
    setf(b, 'AMP_VELO', 64)
    setf(b, 'AMP_KEYTRACK', 64)
    # EGs estaticos
    setf(b, 'EG1_A', 0); setf(b, 'EG1_D', 0); setf(b, 'EG1_S', 127); setf(b, 'EG1_R', 0)
    setf(b, 'EG2_A', 0); setf(b, 'EG2_D', 0); setf(b, 'EG2_S', 127); setf(b, 'EG2_R', 0)
    # timbre2 silencioso: mix osc1 = 0
    setf(b, 'MIX_OSC1', 0, timbre=2)
    rungs.append(b)

    # --- R2: ENV -- igual que R1, solo EG2 (amp) muestra dinamica ---
    b = bytearray(rungs[0]); set_name(b, 'DIAG R2 ENV ')
    setf(b, 'EG2_A', 2); setf(b, 'EG2_D', 10); setf(b, 'EG2_S', 80); setf(b, 'EG2_R', 20)
    rungs.append(b)

    # --- R3: FIL -- igual que R1, filtro cerrado con resonancia media ---
    b = bytearray(rungs[0]); set_name(b, 'DIAG R3 FIL ')
    setf(b, 'CUTOFF', 40)
    setf(b, 'RESONANCE', 100)
    rungs.append(b)

    # --- R4: FEG -- igual que R1, EG1 barrido de cutoff (+32) ---
    b = bytearray(rungs[0]); set_name(b, 'DIAG R4 FEG ')
    setf(b, 'FILTER_EG1_INT', 96)   # +32
    setf(b, 'EG1_A', 0); setf(b, 'EG1_D', 10); setf(b, 'EG1_S', 30); setf(b, 'EG1_R', 40)
    rungs.append(b)

    # --- R5: LFO -- igual que R1, virtual patch LFO1 -> Pitch, int +32 ---
    b = bytearray(rungs[0]); set_name(b, 'DIAG R5 LFO ')
    setf(b, 'PATCH1', 0x02)         # src LFO1(2), dest Pitch(0)
    setf(b, 'PATCH1_INT', 96)       # +32
    # LFO1 triangle, ~5.77 Hz (tabla hardware, knob 110)
    setf(b, 'LFO1_FLAGS', 0x02 | 0x00)  # wave bits=2 (ver Decisions)
    setf(b, 'LFO1_FREQ', 0x30)      # ~48 ->约5.8 Hz segundo LUT firmware
    rungs.append(b)

    # --- R6: LAY -- R1 + layer con timbre2 a la derecha ---
    b = bytearray(rungs[0]); set_name(b, 'DIAG R6 LAY ')
    b[0x10] = 0x40 | (2 << 4)       # voice mode Layer
    setf(b, 'MIX_OSC1', 127, timbre=2)
    setf(b, 'AMP_PAN', 96, timbre=2)     # timbre2 a la derecha (+32)
    setf(b, 'AMP_PAN', 32, timbre=1)     # timbre1 a la izquierda (-32)
    rungs.append(b)

    # --- R7: MFX -- R1 + mod fx chorus encendido ---
    b = bytearray(rungs[0]); set_name(b, 'DIAG R7 MFX ')
    b[0x18] = 80                    # modfx depth 80 -> encendido
    b[0x17] = 40                    # speed 40
    rungs.append(b)

    # --- R8: DEL -- R1 + delay stereo encendido ---
    b = bytearray(rungs[0]); set_name(b, 'DIAG R8 DEL ')
    b[0x14] = 40                    # time 40
    b[0x15] = 80                    # depth 80 -> encendido
    b[0x16] = 0                     # tipo stereo
    rungs.append(b)

    return rungs


def patch_exe(rungs):
    if not os.path.exists(EXE_ORIG):
        raise SystemExit(f"No existe {EXE_ORIG}")
    raw = bytearray(open(EXE_ORIG, 'rb').read())
    occ = [m.start() for m in re.finditer(b'Stab Saw', raw)]
    if len(occ) != 2:
        raise SystemExit(f"Se esperaban 2 copias del banco, hay {len(occ)}")
    BLOCK = 128 * 254
    a, c = occ
    if raw[a:a + BLOCK] != raw[c:c + BLOCK]:
        raise SystemExit("Las dos copias del banco difieren; abortando")

    # A01..A08 (slots 0..7) <- rungs; el resto queda intacto pero las 8 copias
    # de cada rung se escriben por replicate: TODAS las 128 posiciones reciben
    # el mismo programa del rung para que cualquier indice comparado sea el rung.
    for rung_idx, rung in enumerate(rungs):
        exe = bytearray(open(EXE_ORIG, 'rb').read())
        for base in (a, c):
            for slot in range(128):
                exe[base + slot * 254: base + slot * 254 + 254] = rung
        outp = EXE_DIAG.replace('.exe', f"_r{rung_idx + 1}.exe")
        open(outp, 'wb').write(exe)
        print(f"[exe] {os.path.basename(outp)} -- rung {rung_idx + 1}")
    return [EXE_DIAG.replace('.exe', f"_r{i + 1}.exe") for i in range(len(rungs))]


def build_roms(rungs):
    """ROM aislado por rung, con USER slot 2 = rung."""
    orig_fw = os.path.join(EMU_HOME_TPL, 'full FW', 'boot-362.ms2000.bin')
    if not os.path.isfile(orig_fw):
        orig_fw = None
    orig_flash = os.path.join(EMU_HOME_TPL, 'flash.bin')
    ref_sha = hashlib.sha256(open(orig_flash, 'rb').read()).hexdigest()
    print(f"[rom] flash original SHA256={ref_sha[:16]}...")

    homes = []
    for i, rung in enumerate(rungs, 1):
        home = os.path.join(HERE, 'emu_home_r%d' % i)
        if os.path.isdir(home):
            shutil.rmtree(home)
        os.makedirs(home)
        os.makedirs(os.path.join(home, 'full FW'))
        if orig_fw:
            shutil.copy(orig_fw, os.path.join(home, 'full FW'))
        for name in ('ms2000.nvram', 'x811v107.sys'):
            shutil.copy(os.path.join(EMU_HOME_TPL, name), home)
        flash = bytearray(open(orig_flash, 'rb').read())
        USER_BASE = 0x0A0400
        slot = USER_BASE + 2 * 254
        flash[slot:slot + 254] = rung
        open(os.path.join(home, 'flash.bin'), 'wb').write(bytes(flash))
        print(f"[rom] {os.path.basename(home)}: USER#2 = DIAG R{i}")
        homes.append(home)
    return homes


def main():
    os.makedirs(HERE, exist_ok=True)
    rungs = build_rungs()
    for i, r in enumerate(rungs, 1):
        open(os.path.join(HERE, f'rung_r{i}.bin'), 'wb').write(bytes(r))
        print(f"[rung] R{i} {bytes(r[0:12]).decode('ascii')} len={len(r)}")
    patch_exe(rungs)
    build_roms(rungs)
    print("[ok] Ladder generada")


if __name__ == '__main__':
    main()
