# -*- coding: utf-8 -*-
"""
Elige el factor de escala de cutoff que mejor hace coincidir el filtro de ABD
con el del emu medido.

Metrica principal: similitud coseno de 24 bandas log (la misma que usa
CalibrationTests::compareIdenticalPresetsWithEmu para su "Similitud Espectral"),
calculada sobre el mismo programa de prueba (sierra C2 con el mismo filtro).

Uso: python fit_scale.py [escala ...]
"""
import os
import subprocess
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000
KNOBS = [20, 40, 60, 80, 100, 127]
HARNESS = os.path.join(HERE, 'build_harness', 'Release', 'abd_filter_harness.exe')
OUT = os.path.join(HERE, 'abd_scale')
F0 = 65.406391325149658
EDGES = np.geomspace(20.0, 20000.0, 25)
_cache = {}


def abd_ir(knob, scale):
    key = (knob, scale)
    if key in _cache:
        return _cache[key]
    path = os.path.join(OUT, f'abd_ir_{knob:03d}_r000_s{int(round(scale*100)):04d}.f32')
    h = np.fromfile(path, dtype='<f4').astype(np.float64)
    H = np.abs(np.fft.rfft(h * np.hanning(len(h))))
    fa = np.fft.rfftfreq(len(h), 1.0 / SR)
    db = 20 * np.log10(np.maximum(H, 1e-12))
    db -= np.median(db[(fa >= 30) & (fa <= 45)])
    out = (fa[1:], db[1:])
    _cache[key] = out
    return out


def band_energy(freqs, power_db):
    e = 10 ** (power_db / 10.0)
    out = np.zeros(len(EDGES) - 1)
    for i in range(len(EDGES) - 1):
        m = (freqs >= EDGES[i]) & (freqs < EDGES[i + 1])
        out[i] = np.sum(e[m])
    return out


def saw_weight(freqs):
    n = np.maximum(1.0, np.round(freqs / F0))
    return 20 * np.log10(1.0 / n ** 2)


def similarity(npz, scale):
    sims = []
    for k in KNOBS:
        key = str(k)
        if key not in npz or k == 127:
            continue
        fe, de = npz['f'], npz[key]
        ok = np.isfinite(de) & (de > -40.0)
        fe, de = fe[ok], de[ok]
        e_emu = band_energy(fe, de + saw_weight(fe))
        fa, da = abd_ir(k, scale)
        fa, da = fa[fa < 18000], da[fa < 18000]
        e_abd = band_energy(fa, da + saw_weight(fa))
        a, b = np.sqrt(e_emu), np.sqrt(e_abd)
        na, nb = np.linalg.norm(a), np.linalg.norm(b)
        if na < 1e-12 or nb < 1e-12:
            continue
        sims.append((k, 100.0 * float(np.dot(a, b) / (na * nb))))
    return sims


def main():
    npz = np.load(os.path.join(HERE, 'harm_curves.npz'))
    scales = [float(a) for a in sys.argv[1:]] or list(np.round(np.arange(0.8, 4.01, 0.05), 2))
    os.makedirs(OUT, exist_ok=True)
    for s in scales:
        if not all(os.path.exists(os.path.join(
                OUT, f'abd_ir_{k:03d}_r000_s{int(round(s*100)):04d}.f32')) for k in KNOBS):
            subprocess.run([HARNESS, OUT, '0.0', '--ir', '--scale', f'{s:.2f}'] +
                           [str(k) for k in KNOBS], check=True, capture_output=True)
    rows = []
    for s in scales:
        sims = similarity(npz, s)
        if not sims:
            continue
        rows.append((s, float(np.mean([v for _, v in sims])), sims))
    rows.sort(key=lambda t: -t[1])
    print(f"{'escala':>7} {'sim%':>6}   por knob")
    for s, m, sims in rows[:14]:
        print(f'{s:7.2f} {m:6.1f}   ' + '  '.join(f'{k}:{v:5.1f}' for k, v in sims))
    s, m, sims = rows[0]
    print(f'\nmejor escala = {s:.2f}  sim={m:.1f}%  (semitonos {12 * np.log2(s):+.2f})')
    np.save(os.path.join(HERE, 'scale_rows.npy'), np.array([(a, b) for a, b, _ in rows]))


if __name__ == '__main__':
    main()
