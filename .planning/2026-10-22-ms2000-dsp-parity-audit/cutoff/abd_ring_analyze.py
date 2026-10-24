# -*- coding: utf-8 -*-
"""¿A qué frecuencia resuena (anillo) la cadena ABD de 2 SVF en cascada?

Lee los .f32 de `abd_filter_harness --ir` con reso alto y estima el pico
espectral del IR (autoscilación del par de SVF). Sirve para saber si el
topología duplica la frecuencia de corte o no, y así poder interpretar la
autoscilación medida en el emulador.
"""
import glob
import math
import os
import re

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000.0


def peaks(x, nfft=1 << 17):
    seg = x * np.hanning(len(x))
    X = np.abs(np.fft.rfft(seg, nfft))
    f = np.fft.rfftfreq(nfft, 1 / SR)
    m = (f > 5) & (f < 23000)
    Xm, fm = X[m], f[m]
    idx = np.argsort(Xm)[::-1]
    out, used = [], []
    for i in idx:
        if any(abs(fm[i] - u) < 10 for u in used) or Xm[i] <= 0:
            continue
        out.append((fm[i], 20 * math.log10(Xm[i] / Xm.max())))
        used.append(fm[i])
        if len(out) == 3:
            break
    return out


def main():
    for path in sorted(glob.glob(os.path.join(HERE, 'abd_ring', '*.f32'))):
        knob = int(re.search(r'_(\d{3})_', os.path.basename(path)).group(1))
        x = np.fromfile(path, dtype='<f4')
        a = np.abs(x)
        nz = np.nonzero(a > a.max() * 1e-4)[0]
        s = int(nz[0] + 0.3 * (nz[-1] - nz[0])) if nz.size else 0
        p = peaks(x[s:])
        print(f'knob {knob:3d}: pico del IR ' +
              '  '.join(f'{fr:8.1f} Hz ({db:5.1f} dB)' for fr, db in p))


if __name__ == '__main__':
    main()
