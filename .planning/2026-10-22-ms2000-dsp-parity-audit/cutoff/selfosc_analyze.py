# -*- coding: utf-8 -*-
"""
Analiza la cola de autoscilación de los WAV de selfosc.py y la compara con la
predicción del coeficiente Q23 que el firmware escribe en el DSP.

Predicciones por knob (de curve_compare.py):
  A) tan(pi*f/fs) en Q23   -> f = (fs/pi)*atan(word/2^23)
  B) lectura lineal 0..20k  -> f = 20000*word/0xFC9536   (mapeo ABD actual)

Uso: python selfosc_analyze.py
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


def peaks(x, nfft=1 << 17):
    seg = x * np.hanning(len(x))
    X = np.abs(np.fft.rfft(seg, nfft))
    f = np.fft.rfftfreq(nfft, 1 / SR)
    m = (f > 20) & (f < 20000)
    Xm, fm = X[m], f[m]
    idx = np.argsort(Xm)[::-1]
    out, used = [], []
    for i in idx:
        if any(abs(fm[i] - u) < 8 for u in used):
            continue
        # parábola log-espectral
        j = i
        if 0 < j < len(Xm) - 1:
            a, b, c = np.log(Xm[j - 1] + 1e-30), np.log(Xm[j] + 1e-30), np.log(Xm[j + 1] + 1e-30)
            dd = a - 2 * b + c
            off = 0.5 * (a - c) / dd if abs(dd) > 1e-12 else 0.0
        else:
            off = 0.0
        out.append(float(fm[j] + off * (fm[1] - fm[0])))
        used.append(fm[j])
        if len(out) == 4:
            break
    return out


def hz_tan(word):
    return (SR / math.pi) * math.atan((word & 0xFFFFFF) / 2.0 ** 23)


def hz_lin(word):
    return 20000.0 * (word & 0xFFFFFF) / 0xFC9536


def rom_words():
    rows = {}
    for line in open(os.path.join(HERE, 'curve_compare.csv'), encoding='utf-8'):
        p = line.strip().split(',')
        if p[0] == 'knob':
            continue
        rows[int(p[0])] = (int(p[1], 16), int(p[3], 16))
    return rows


def main():
    words = rom_words()
    for path in sorted(glob.glob(os.path.join(HERE, 'selfosc_k[0-9][0-9][0-9].wav'))):
        knob = int(re.search(r'k(\d{3})', os.path.basename(path)).group(1))
        x = read(path)
        a = np.abs(x)
        idx = np.nonzero(a > a.max() * 0.01)[0]
        if idx.size < 4096:
            print(f'knob {knob}: sin señal')
            continue
        hi, lo = int(idx[-1]), int(idx[0])
        print(f'\nknob {knob:3d}  dur={len(x) / SR:.2f}s  señal {lo / SR:.2f}..{hi / SR:.2f}s '
              f'pico {20 * math.log10(a.max() + 1e-30):.1f} dBFS')
        # varias ventanas: media, y cola
        for name, frac in (('media', 0.4), ('tardia', 0.7), ('cola', 0.85)):
            s = int(lo + frac * (hi - lo))
            seg = x[s:hi]
            if len(seg) < 20000:
                continue
            p = peaks(seg)
            print(f'  {name:>6} [{s / SR:5.2f}s] picos: ' +
                  ' '.join(f'{v:8.1f}' for v in p))
        if knob in words:
            hw = words[knob][0]
            print(f'  firmware word 0x{hw:06X}:  tan->{hz_tan(hw):8.1f} Hz   '
                  f'lin20k->{hz_lin(hw):8.1f} Hz')


if __name__ == '__main__':
    main()
