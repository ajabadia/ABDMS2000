# -*- coding: utf-8 -*-
"""
Extrae el coeficiente de cutoff que el firmware H8S escribe al DSP.

El firmware no usa 0xFFFFA0 (docs): en esta ROM el puerto HDI08 del H8S está en
0xFFFF60-0xFFFF64, y los parámetros salen por 0xFFFF61 con handshake. El log del
emulador imprime cada write con su PC:

    [EXECUTE] 0x00xxxx: MOV.B R0L=0x1A -> @0x00FFFF61

Para cada knob se siembra el mismo programa en TODOS los slots USER y PRELOAD de
una copia desechable del flash, de modo que el programa que el firmware carga en
el boot sea el probe (no hace falta MIDI ni nota). Se corre el emulador ~1.5 s y
se guarda la secuencia de (PC, valor) de los writes a 0xFFFF60..0xFFFF64.

Uso:
  python dsp_coeff_probe.py 0 20 40 60 80 100 127
"""
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LADDER = os.path.normpath(os.path.join(HERE, '..', 'ladder'))
EMU_EXE = r"D:\desarrollos\ABDSynths\_RESOURCES\ms2000\ms2000r-emu-main\ms2000r-emu-main\build_vst\Release\ms2000_emulator.exe"
HOME_TPL = os.path.join(LADDER, 'emu_home_r1')      # flash original + nvram
USER_BASE = 0x0A0400
PRELOAD_BASE = 0x0B0000
T1 = 0x26
WRITE_RE = re.compile(r'\[EXECUTE\] (0x[0-9A-Fa-f]+):.*?-> @0x00(FFFF[0-9A-F]{2})')
VAL_RE = re.compile(r'=\s*(0x[0-9A-Fa-f]+)')


def build_program(knob, reso=0):
    b = bytearray(open(os.path.join(LADDER, 'init_base.bin'), 'rb').read())
    name = (f'COEFF {knob:3d}' + ' ' * 12)[:12]
    for i, ch in enumerate(name):
        b[i] = ord(ch)
    b[T1 + 20] = knob & 0x7F          # CUTOFF
    b[T1 + 21] = reso & 0x7F          # RESONANCE
    return b


def pack_korg(src):
    out = bytearray()
    for g in range(0, len(src), 7):
        grp = src[g:g + 7]
        coll = 0
        for j, b in enumerate(grp):
            coll |= (b >> 7) << (6 - j)
        out.append(coll)
        out.extend(b & 0x7F for b in grp)
    return bytes(out)


def midiin(knob, reso=0):
    prog = build_program(knob, reso)
    syx = [0xF0, 0x42, 0x30, 0x58, 0x40] + list(pack_korg(prog)) + [0xF7]
    msg = ' '.join(f'{b:02X}' for b in syx)
    return f'3.6:{msg};4.0:90 3C 64'


def run(knob, seconds=3.0):
    home = os.path.join(HERE, f'coeff_home_k{knob:03d}')
    if os.path.isdir(home):
        shutil.rmtree(home, onexc=lambda f, p, e: (os.chmod(p, 0o666), f(p)))
    os.makedirs(os.path.join(home, 'full FW'), exist_ok=True)
    fw = os.path.join(HOME_TPL, 'full FW', 'boot-362.ms2000.bin')
    if os.path.isfile(fw):
        shutil.copy(fw, os.path.join(home, 'full FW'))
    for fname in ('ms2000.nvram', 'x811v107.sys'):
        dst = os.path.join(home, fname)
        shutil.copy(os.path.join(HOME_TPL, fname), dst)
        os.chmod(dst, 0o666)

    # flash original sin tocar: el programa se inyecta al edit buffer por SysEx
    shutil.copy(os.path.join(HOME_TPL, 'flash.bin'), os.path.join(home, 'flash.bin'))

    env = dict(os.environ, MS2K_HOME=home, MS2K_MIDIIN=midiin(knob))
    cmd = [EMU_EXE, '--headless', '--duration', str(seconds)]
    out = os.path.join(HERE, f'coeff_writes_k{knob:03d}.txt')
    with open(out, 'w', encoding='utf-8', errors='replace') as f:
        p = subprocess.Popen(cmd, env=env, cwd=home, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors='replace',
                             bufsize=1)
        try:
            for line in p.stdout:
                if '0x00FFFF6' in line and '-> @0x00FFFF6' in line:
                    f.write(line if line.endswith('\n') else line + '\n')
        finally:
            p.wait(timeout=120)
    print(f'[k{knob:03d}] exit={p.returncode} writes={sum(1 for _ in open(out, encoding="utf-8", errors="replace"))}')


def main():
    knobs = [int(a) for a in sys.argv[1:]] or [20, 60, 100]
    print(f'{len(knobs)} knobs, writes a 0xFFFF60..64 con dump SysEx a 3.6 s')
    for k in knobs:
        run(k)


if __name__ == '__main__':
    main()
