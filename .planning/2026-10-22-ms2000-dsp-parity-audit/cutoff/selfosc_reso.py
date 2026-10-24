# -*- coding: utf-8 -*-
"""
¿La frecuencia de autoscilación del VCF del emulador depende de la resonancia?

Si el firmware compensa el cutoff según la resonancia (tablas 0x26554..0x26D54),
el anillo debe moverse al cambiar RESONANCE con el mismo CUTOFF.

Uso: python selfosc_reso.py 20:127 20:110 20:100 20:80 60:127 60:110
"""
import os
import subprocess
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dsp_coeff_probe as P  # noqa: E402
import selfosc as S  # noqa: E402


def render(knob, reso):
    home = os.path.join(S.HERE, f'coeff_home_k{knob:03d}')
    wav = os.path.join(S.HERE, f'resosc_k{knob:03d}_r{reso:03d}.wav')
    b = P.build_program(knob, reso=reso)
    b[P.T1 + 37] = 80     # EG2_R: cola larga
    b[P.T1 + 36] = 127    # EG2_S
    syx = [0xF0, 0x42, 0x30, 0x58, 0x40] + list(P.pack_korg(b)) + [0xF7]
    msg = ' '.join(f'{x:02X}' for x in syx)
    env = dict(os.environ, MS2K_HOME=os.path.abspath(home),
               MS2K_MIDIIN=f'3.6:{msg};4.0:90 3C 64;5.0:80 3C 00')
    cmd = [P.EMU_EXE, '--headless', '--render-test', wav, '--duration', '7']
    p = subprocess.run(cmd, env=env, cwd=home, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=900)
    if p.returncode != 0:
        print(f'[k{knob} r{reso}] emu exit {p.returncode}')
        return None
    w = wave.open(wav, 'rb')
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    a = np.frombuffer(w.readframes(n), dtype='<i4' if sw == 4 else '<i2').astype(np.float32)
    w.close()
    a /= 2 ** 31 if sw == 4 else 2 ** 15
    a = a.reshape(-1, ch)
    return (a[:, 0] + a[:, 1]) * 0.5


def main():
    for arg in sys.argv[1:]:
        knob, reso = (int(v) for v in arg.split(':'))
        x = render(knob, reso)
        if x is None:
            continue
        f = S.f0_tail(x)
        peak = 20 * np.log10(np.abs(x).max() + 1e-30)
        print(f'knob {knob:3d} reso {reso:3d}: anillo ' +
              (f'{f:8.1f} Hz' if f else 'sin cola') + f'   pico {peak:6.1f} dBFS')


if __name__ == '__main__':
    main()
