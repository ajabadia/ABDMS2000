# -*- coding: utf-8 -*-
"""
Rungs nuevos sobre la plantilla de la escalera (2026-10-22/23).

Reutiliza `ladder_build.py` (misma plantilla INIT, mismos offsets) y agrega:

  r4b    FEG rediseñado: cutoff base 80, EG1 int +32, decay 30, sustain 0
  r5p8   LFO1 -> Pitch con int +8   (zona lineal en cents de la curva ROM)
  r5m32  LFO1 -> Pitch con int -32  (desambigua signo)
  r5p63  LFO1 -> Pitch con int +63  (fondo del rango)

Genera `rung_<name>.bin`, un exe ABD parcheado por rung
(`CalibrationTests_probe_<name>.exe`) y un home aislado del emu por rung
(`emu_home_<name>/`). No toca los rungs originales r1..r8.
"""
import os
import re
import shutil

import ladder_build as lb

HERE = os.path.dirname(os.path.abspath(__file__))
USER_BASE = 0x0A0400


def probes():
    base = lb.build_rungs()[0]          # R1: saw, filtro abierto, amp plano, sin FX
    out = {}

    # --- R4b: EG1 -> cutoff con base media, donde el barrido sí tiene recorrido ---
    b = bytearray(base)
    lb.set_name(b, 'DIAG R4b FEG')
    lb.setf(b, 'CUTOFF', 80)            # ~2 kHz leídos de la tabla ROM
    lb.setf(b, 'RESONANCE', 40)
    lb.setf(b, 'FILTER_EG1_INT', 96)    # +32
    lb.setf(b, 'EG1_A', 0)
    lb.setf(b, 'EG1_D', 30)             # decay audible dentro de los 24k frames
    lb.setf(b, 'EG1_S', 0)              # cae a cero: barrido completo
    lb.setf(b, 'EG1_R', 40)
    out['r4b'] = b

    # --- R5: LFO1 -> Pitch con distintas intensidades (mismo LFO que R5 original) ---
    #   LFO free-run (key sync off) = como el rung original; la fase inicial es arbitraria.
    for name, value in (('r5p8', 72), ('r5m32', 32), ('r5p63', 127)):
        b = bytearray(base)
        lb.set_name(b, f'DIAG {name.upper():6s}'[:12])
        lb.setf(b, 'PATCH1', 0x02)      # src LFO1 (2) -> dest Pitch (0)
        lb.setf(b, 'PATCH1_INT', value)  # 64 = 0; 72 = +8; 32 = -32; 127 = +63
        lb.setf(b, 'LFO1_FLAGS', 0x02)  # triangular
        lb.setf(b, 'LFO1_FREQ', 0x30)   # knob 48 = 0.79 Hz (tabla ROM 0x295E8)
        out[name] = b

    # --- R5k: igual que el R5 original (+32) pero con Key Sync = Voice ---
    #   Con key sync, ambos lados arrancan la fase en el note-on y la comparacion
    #   deja de depender del instante en que el LFO libre paso por el note-on.
    b = bytearray(base)
    lb.set_name(b, 'DIAG R5K SYNC')
    lb.setf(b, 'PATCH1', 0x02)
    lb.setf(b, 'PATCH1_INT', 96)        # +32
    lb.setf(b, 'LFO1_FLAGS', 0x22)      # triangular + key sync Voice (bits 4-5 = 2)
    lb.setf(b, 'LFO1_FREQ', 0x30)       # 0.79 Hz
    out['r5k'] = b

    return out


def patch_exe(name, rung):
    raw = bytearray(open(lb.EXE_ORIG, 'rb').read())
    occ = [m.start() for m in re.finditer(b'Stab Saw', raw)]
    if len(occ) != 2:
        raise SystemExit(f'Se esperaban 2 copias del banco, hay {len(occ)}')
    block = 128 * 254
    if raw[occ[0]:occ[0] + block] != raw[occ[1]:occ[1] + block]:
        raise SystemExit('Las dos copias del banco difieren; abortando')
    for base in occ:
        for slot in range(128):
            off = base + slot * 254
            raw[off:off + 254] = rung
    outp = os.path.join(os.path.dirname(lb.EXE_ORIG), f'CalibrationTests_probe_{name}.exe')
    open(outp, 'wb').write(bytes(raw))
    print(f'[exe] {os.path.basename(outp)}')
    return outp


def _force_rmtree(path):
    """rmtree tolerante a archivos de ROM marcados como solo-lectura."""

    def _handler(func, p, exc):
        os.chmod(p, 0o666)
        func(p)

    shutil.rmtree(path, onexc=_handler)


def build_roms(name, rung):
    home = os.path.join(HERE, f'emu_home_{name}')
    if os.path.isdir(home):
        _force_rmtree(home)
    os.makedirs(os.path.join(home, 'full FW'))
    orig_fw = os.path.join(lb.EMU_HOME_TPL, 'full FW', 'boot-362.ms2000.bin')
    if os.path.isfile(orig_fw):
        shutil.copy(orig_fw, os.path.join(home, 'full FW'))
    for fname in ('ms2000.nvram', 'x811v107.sys'):
        dst = os.path.join(home, fname)
        shutil.copy(os.path.join(lb.EMU_HOME_TPL, fname), dst)
        os.chmod(dst, 0o666)
    flash = bytearray(open(os.path.join(lb.EMU_HOME_TPL, 'flash.bin'), 'rb').read())
    off = USER_BASE + 2 * 254
    flash[off:off + 254] = rung
    open(os.path.join(home, 'flash.bin'), 'wb').write(bytes(flash))
    print(f'[rom] {os.path.basename(home)}: USER#2 = {name}')


def main():
    for name, rung in probes().items():
        open(os.path.join(HERE, f'rung_{name}.bin'), 'wb').write(bytes(rung))
        print(f'[rung] {name} {bytes(rung[0:12]).decode("ascii")} len={len(rung)}')
        patch_exe(name, rung)
        build_roms(name, rung)


if __name__ == '__main__':
    main()
