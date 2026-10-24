# -*- coding: utf-8 -*-
"""Codo real del LPF24: emulador vs ABD, ruido blanco, resonancia 0.

A diferencia de `fit_scale.py`, no se normaliza contra la corrida de knob 127
(que supone que ahi el filtro esta abierto: si no lo esta, el ajuste se sesga
hacia arriba). Aqui se mide el codo absoluto de cada lado por separado:
nivel de pasabanda (100-200 Hz) y frecuencia a la que la respuesta cae 3 dB.

Uso: python corner_compare.py
"""
import os
import re
import struct
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
SR = 48000.0
KNOBS = [20, 40, 60, 80, 100, 127]


def rom_table():
    src = open(os.path.join(REPO, 'Source', 'Core', 'MS2000FirmwareTables.h'),
               encoding='utf-8', errors='replace').read()
    m = re.search(r'kRomFilterCutoffQ23\[256\]\s*=\s*\{(.*?)\};', src, re.S)
    return [int(x, 0) for x in re.findall(r'0x[0-9A-Fa-f]+', m.group(1))]


ROM = rom_table()


def rom_hz(knob):
    idx = min(255, 47 + knob)
    g = ROM[idx] / 2.0 ** 23
    return (SR / np.pi) * np.arctan(g)


def read_wav_mono(path):
    w = wave.open(path, 'rb')
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(n)
    w.close()
    a = np.frombuffer(raw, dtype='<i4' if sw == 4 else '<i2').astype(np.float32)
    a /= 2 ** 31 if sw == 4 else 2 ** 15
    a = a.reshape(-1, ch)
    return (a[:, 0] + a[:, 1]) * 0.5


def read_f32(path):
    return np.fromfile(path, dtype='<f4')


def psd(x, n=8192):
    win = np.hanning(n)
    acc = np.zeros(n // 2 + 1)
    cnt = 0
    for i in range(0, len(x) - n, n // 2):
        acc += np.abs(np.fft.rfft(x[i:i + n] * win)) ** 2
        cnt += 1
    return np.fft.rfftfreq(n, 1 / SR), acc / max(cnt, 1)


def smooth_log(f, p, fmin=60.0, fmax=20000.0, ppv=12):
    """Suaviza el PSD en escala log y devuelve una grilla uniforme en log f."""
    grid = np.geomspace(fmin, fmax, 400)
    out = np.empty_like(grid)
    lf = np.log(f)
    for i, fc in enumerate(grid):
        band = np.abs(lf - np.log(fc)) <= (np.log(2 ** (1 / ppv)) / 2)
        out[i] = 10 * np.log10(max(1e-30, p[band].mean())) if band.any() else np.nan
    return grid, out


def corner(grid, db, drop=3.0):
    """Frecuencia donde la respuesta cae `drop` dB por debajo del pasabanda."""
    ref = np.nanmean(db[(grid >= 100) & (grid <= 200)])
    rel = db - ref
    # ultima frecuencia (barriendo hacia arriba desde 200 Hz) por encima del umbral
    idx = np.where((grid >= 200) & (rel >= -drop))[0]
    if idx.size == 0:
        return float('nan'), ref
    return float(grid[idx[-1]]), ref


def main():
    print(f"{'knob':>4} {'ROM Hz':>9} | {'emu codo':>9} {'ABD codo':>9} {'ratio':>6} | "
          f"{'emu -6dB':>9} {'ABD -6dB':>9} {'ratio':>6}")
    for k in KNOBS:
        wav = os.path.join(HERE, f'wave_k{k}.wav')
        abd_path = os.path.join(HERE, 'abd_raw', f'abd_k{k:03d}_r000.f32')
        if not (os.path.exists(wav) and os.path.exists(abd_path)):
            print(f'{k:>4} faltan datos ({os.path.exists(wav)}, {os.path.exists(abd_path)})')
            continue
        e = read_wav_mono(wav)
        a = read_f32(abd_path)
        fe, pe = psd(e)
        fa, pa = psd(a)
        ge, de = smooth_log(fe, pe)
        ga, da = smooth_log(fa, pa)
        ce3, _ = corner(ge, de, 3.0)
        ca3, _ = corner(ga, da, 3.0)
        ce6, _ = corner(ge, de, 6.0)
        ca6, _ = corner(ga, da, 6.0)
        print(f'{k:>4} {rom_hz(k):9.1f} | {ce3:9.1f} {ca3:9.1f} {ce3/ca3:6.2f} | '
              f'{ce6:9.1f} {ca6:9.1f} {ce6/ca6:6.2f}')


if __name__ == '__main__':
    main()
