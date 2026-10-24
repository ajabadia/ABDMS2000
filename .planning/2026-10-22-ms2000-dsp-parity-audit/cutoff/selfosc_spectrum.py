# -*- coding: utf-8 -*-
"""Espectro detallado de la cola de autoscilación: ¿dónde está el pico real?

Para cada WAV imprime los 8 picos dominantes con su nivel relativo y el nivel en
las frecuencias candidatas (tal cual, 2x, y las predichas por tan con fs=48k/96k).
"""
import glob
import math
import os
import re
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000.0


def read(path):
    w = wave.open(path, 'rb')
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    a = np.frombuffer(w.readframes(n), dtype='<i4' if sw == 4 else '<i2').astype(np.float32)
    w.close()
    a /= 2 ** 31 if sw == 4 else 2 ** 15
    a = a.reshape(-1, ch)
    return (a[:, 0] + a[:, 1]) * 0.5


def spectrum(x, nfft=1 << 18):
    seg = x * np.hanning(len(x))
    X = np.abs(np.fft.rfft(seg, nfft))
    f = np.fft.rfftfreq(nfft, 1 / SR)
    return f, X


def level_at(f, X, f0):
    i = int(round(f0 / (f[1] - f[0])))
    lo, hi = max(0, i - 3), min(len(X), i + 4)
    return 20 * math.log10(max(X[lo:hi].max(), 1e-30))


def hz_tan(word, fs=48000.0):
    return (fs / math.pi) * math.atan((word & 0xFFFFFF) / 2.0 ** 23)


def main():
    words = {}
    for line in open(os.path.join(HERE, 'curve_compare.csv'), encoding='utf-8'):
        p = line.strip().split(',')
        if p[0] == 'knob':
            continue
        words[int(p[0])] = int(p[1], 16)

    for path in sorted(glob.glob(os.path.join(HERE, 'selfosc_k[0-9][0-9][0-9].wav'))):
        knob = int(re.search(r'k(\d{3})', os.path.basename(path)).group(1))
        x = read(path)
        a = np.abs(x)
        idx = np.nonzero(a > a.max() * 0.02)[0]
        hi, lo = int(idx[-1]), int(idx[0])
        s = int(lo + 0.6 * (hi - lo))
        seg = x[s:hi]
        f, X = spectrum(seg)
        X /= X.max()
        m = (f > 40) & (f < 20000)
        fm, Xm = f[m], X[m]

        # picos locales
        pk = [i for i in range(1, len(Xm) - 1) if Xm[i] > Xm[i - 1] and Xm[i] > Xm[i + 1]]
        pk.sort(key=lambda i: -Xm[i])
        tops, used = [], []
        for i in pk:
            if any(abs(fm[i] - u) < 15 for u in used):
                continue
            tops.append((fm[i], 20 * math.log10(max(Xm[i], 1e-12))))
            used.append(fm[i])
            if len(tops) >= 8:
                break

        w = words.get(knob)
        print(f'\n=== knob {knob}  (Ventana {s / SR:.2f}-{hi / SR:.2f}s) ===')
        print('  picos: ' + '  '.join(f'{fr:7.1f}Hz {db:6.1f}dB' for fr, db in tops))
        if w is not None:
            for fs in (48000.0, 96000.0):
                c = hz_tan(w, fs)
                print(f'  tan fs={int(fs):5d}: fc={c:7.1f}  L(fc)={level_at(f, X, c):6.1f}  '
                      f'L(2fc)={level_at(f, X, 2 * c):6.1f}')


if __name__ == '__main__':
    main()
