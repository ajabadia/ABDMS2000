# -*- coding: utf-8 -*-
"""
Cociente de las respuestas de ruido del emulador entre dos knobs de cutoff.

Si la fuente de ruido es la misma secuencia en todas las corridas (LCG
determinista del DSP), S_k(f)/S_ref(f) cancela la coloración de la fuente y deja
solo el cociente de las respuestas del VCF. Ajustando

    R(f) = [ (1 + (f/f_ref)^2) / (1 + (f/f_k)^2) ]^4

(cascada de dos cámaras críticamente amortiguadas = LPF24 con reso 0) se obtiene
f_k / f_ref sin conocer la fuente.

Sirve para discriminar la LEY de cutoff: la ley vieja (0x26154 lineal a 20 kHz)
predice f100/f20 = 101.6, la nueva (asin(2w)) predice 66.0.

Uso: python noise_ratio.py 20 40 60 80 100
"""
import math
import os
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000.0


def load(knob):
    path = os.path.join(HERE, f'wave_k{knob}.wav')
    if not os.path.isfile(path):
        return None
    w = wave.open(path, 'rb')
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    a = np.frombuffer(w.readframes(n), dtype='<i4' if sw == 4 else '<i2').astype(np.float32)
    w.close()
    a /= 2 ** 31 if sw == 4 else 2 ** 15
    a = a.reshape(-1, ch)
    return (a[:, 0] + a[:, 1]) * 0.5


def psd(x, nfft=4096):
    n = len(x) // nfft
    if n < 2:
        return None, None
    w = np.hanning(nfft)
    acc = np.zeros(nfft // 2 + 1)
    for i in range(n):
        seg = x[i * nfft:(i + 1) * nfft] * w
        acc += np.abs(np.fft.rfft(seg)) ** 2
    f = np.fft.rfftfreq(nfft, 1 / SR)
    return f, acc / n


def main():
    knobs = [int(a) for a in sys.argv[1:]] or [20, 40, 60, 80, 100]
    ref = knobs[0]
    xr = load(ref)
    if xr is None:
        sys.exit(f'falta wave_k{ref}.wav')
    f, pr = psd(xr)
    print(f'referencia knob {ref}')
    for k in knobs[1:]:
        xk = load(k)
        if xk is None:
            print(f'  knob {k}: falta wave_k{k}.wav')
            continue
        _, pk = psd(xk)
        m = (f > 150) & (f < 16000)
        fm = f[m]
        ratio = pk[m] / np.maximum(pr[m], 1e-30)
        # suavizado en log-frecuencia (bins ~ 1/6 octava)
        best, beste = None, None
        for r in np.linspace(2.0, 400.0, 4000):
            model = ((1 + (fm / 1000.0) ** 2) / (1 + (fm / (1000.0 * r)) ** 2)) ** 4
            g = np.exp(np.mean(np.log(ratio) - np.log(model)))     # ganancia global
            err = np.mean((np.log(ratio) - np.log(model) - np.log(g)) ** 2)
            if beste is None or err < beste:
                best, beste = r, err
        print(f'  knob {k:3d}: f_k/f_ref = {best:7.2f}  (RMS log-err {math.sqrt(beste):.3f})')


if __name__ == '__main__':
    main()
