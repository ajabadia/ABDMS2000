# -*- coding: utf-8 -*-
"""Frecuencia de autoscilación del VCF del emulador por knob (ancla Hz).

Programa: sierra C4, resonancia 127 (autoscilación), release de EG2 largo para
que el anillo del filtro siga audible tras el note-off. Se mide la frecuencia
dominante de la cola (después del note-off), que es el codo del filtro.

Uso: python selfosc.py 20 60 100 127
"""
import os
import subprocess
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dsp_coeff_probe as P  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LADDER = os.path.normpath(os.path.join(HERE, '..', 'ladder'))
SR = 48000.0


def program(knob):
    b = P.build_program(knob, reso=127)
    # release largo de EG2 para escuchar el anillo tras el note-off
    b[P.T1 + 37] = 80     # EG2_R
    b[P.T1 + 36] = 127    # EG2_S
    return b


def render(knob):
    home = os.path.join(HERE, f'coeff_home_k{knob:03d}')
    wav = os.path.join(HERE, f'selfosc_k{knob:03d}.wav')
    prog = program(knob)
    syx = [0xF0, 0x42, 0x30, 0x58, 0x40] + list(P.pack_korg(prog)) + [0xF7]
    msg = ' '.join(f'{b:02X}' for b in syx)
    env = dict(os.environ, MS2K_HOME=os.path.abspath(home),
               MS2K_MIDIIN=f'3.6:{msg};4.0:90 3C 64;5.0:80 3C 00')
    cmd = [P.EMU_EXE, '--headless', '--render-test', wav, '--duration', '7']
    p = subprocess.run(cmd, env=env, cwd=home, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=900)
    if p.returncode != 0:
        print(f'[k{knob}] emu exit {p.returncode}')
        return None
    w = wave.open(wav, 'rb')
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    a = np.frombuffer(w.readframes(n), dtype='<i4' if sw == 4 else '<i2').astype(np.float32)
    w.close()
    a /= 2 ** 31 if sw == 4 else 2 ** 15
    a = a.reshape(-1, ch)
    return (a[:, 0] + a[:, 1]) * 0.5


def f0_tail(x):
    """Frecuencia dominante de la cola: último 40% con señal."""
    peak = np.abs(x)
    idx = np.nonzero(peak > peak.max() * 0.01)[0]
    if idx.size == 0:
        return None
    start = int(idx[0] + 0.75 * (idx[-1] - idx[0]))
    seg = x[start:idx[-1]]
    if len(seg) < 4096:
        return None
    seg = seg * np.hanning(len(seg))
    X = np.abs(np.fft.rfft(seg, 1 << 16))
    f = np.fft.rfftfreq(1 << 16, 1 / SR)
    m = (f > 20) & (f < 15000)
    i = np.argmax(X[m])
    # parábola sobre el log del espectro
    if 0 < i < len(X) - 1:
        a, b, c = np.log(X[m][i - 1] + 1e-30), np.log(X[m][i] + 1e-30), np.log(X[m][i + 1] + 1e-30)
        d = a - 2 * b + c
        off = 0.5 * (a - c) / d if abs(d) > 1e-12 else 0.0
    else:
        off = 0.0
    return float(f[m][i] + off * (f[1] - f[0]))


def main():
    for a in sys.argv[1:]:
        knob = int(a)
        x = render(knob)
        if x is None:
            continue
        f = f0_tail(x)
        print(f'knob {knob:3d}: autoscilación {f:7.1f} Hz' if f else f'knob {knob}: sin cola')


if __name__ == '__main__':
    main()
