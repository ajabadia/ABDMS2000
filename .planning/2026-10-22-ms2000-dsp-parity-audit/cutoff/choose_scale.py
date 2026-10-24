# -*- coding: utf-8 -*-
"""
Elige el factor de escala de cutoff midiendo el residuo RMS (dB) entre la
envolvente armónica del emu y la de ABD, evaluada en las mismas frecuencias.

A diferencia de fit_scale.py (similitud coseno por bandas, dominada por los
armónicos graves) esto compara armónico por armónico y reporta el error medio
en dB, que es la métrica útil para decidir el factor.

Uso: python choose_scale.py
"""
import os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000
KNOBS = [20, 40, 60, 80, 100]
SCALES = np.round(np.arange(0.80, 5.01, 0.05), 2)
BANDS = [(100.0, 8000.0), (100.0, 14000.0)]


def abd_db(knob, scale):
    path = os.path.join(HERE, 'abd_scale',
                        f'abd_ir_{knob:03d}_r000_s{int(round(scale * 100)):04d}.f32')
    if not os.path.exists(path):
        return None
    h = np.fromfile(path, dtype='<f4').astype(np.float64)
    H = np.abs(np.fft.rfft(h * np.hanning(len(h))))
    fa = np.fft.rfftfreq(len(h), 1.0 / SR)
    db = 20 * np.log10(np.maximum(H, 1e-12))
    db -= np.median(db[(fa >= 30) & (fa <= 45)])
    return fa, db


def main():
    npz = np.load(os.path.join(HERE, 'harm_curves.npz'))
    fe_all = npz['f']
    rows = []
    for s in SCALES:
        per_band = {b: [] for b in BANDS}
        per_knob = {}
        for k in KNOBS:
            de_all = npz[str(k)]
            got = abd_db(k, s)
            if got is None:
                continue
            fa, da = got
            # |H| de ABD en las frecuencias armónicas del emu
            da_i = np.interp(np.log2(fe_all), np.log2(fa), da, left=-400, right=-400)
            for lo, hi in BANDS:
                m = (fe_all >= lo) & (fe_all <= hi) & np.isfinite(de_all) & (de_all > -40.0) & (da_i > -40.0)
                if np.count_nonzero(m) < 8:
                    continue
                d = de_all[m] - da_i[m]
                d -= np.median(d)
                per_band[(lo, hi)].append(float(np.sqrt(np.mean(d ** 2))))
            per_knob[k] = per_band
        if not per_band[(100.0, 8000.0)]:
            continue
        rows.append((s,
                     float(np.mean(per_band[(100.0, 8000.0)])),
                     float(np.mean(per_band[(100.0, 14000.0)])) if per_band[(100.0, 14000.0)] else float('nan')))

    print(f"{'escala':>7} {'semis':>6} {'rms 100-8k':>11} {'rms 100-14k':>12}")
    for s, r8, r14 in rows:
        print(f'{s:7.2f} {12 * np.log2(s):+6.2f} {r8:11.2f} {r14:12.2f}')

    best8 = min(rows, key=lambda t: t[1])
    best14 = min(rows, key=lambda t: t[2] if np.isfinite(t[2]) else 1e9)
    base = [r for r in rows if abs(r[0] - 1.0) < 1e-6]
    print(f'\nmejor (100Hz-8kHz)  = {best8[0]:.2f} ({12 * np.log2(best8[0]):+.2f} semitonos) rms={best8[1]:.2f} dB')
    print(f'mejor (100Hz-14kHz) = {best14[0]:.2f} ({12 * np.log2(best14[0]):+.2f} semitonos) rms={best14[2]:.2f} dB')
    if base:
        print(f'sin correccion (1.00): rms={base[0][1]:.2f} dB / {base[0][2]:.2f} dB')


if __name__ == '__main__':
    main()
