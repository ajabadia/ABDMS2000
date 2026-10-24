# -*- coding: utf-8 -*-
"""
Sonda de curva de cutoff: lee los .bin de 24000 frames (header u32 + L + R)
que escribe el comparador y estima la frecuencia de corte del filtro por
interseccion de asintotas (pasante plana vs -24 dB/oct), robusta al damping.

Uso:
  python cutoff_probe.py <bin> [<bin> ...]
  python cutoff_probe.py --ratio ref.bin test.bin   # transfer function test/ref
"""
import os
import struct
import sys
import numpy as np

SR = 48000
C4 = 261.625565


def read_bin(path):
    b = open(path, 'rb').read()
    n = struct.unpack('<I', b[:4])[0]
    x = np.frombuffer(b[4:4 + 4 * n], dtype='<f4').astype(np.float64)
    y = np.frombuffer(b[4 + 4 * n:4 + 8 * n], dtype='<f4').astype(np.float64)
    return x, y


def magnitude(x, nper=4096, skip=1024):
    """Welch: media de |FFT|^2, ventana Hann, 50% solape."""
    x = x[skip:]
    w = np.hanning(nper)
    acc = np.zeros(nper // 2 + 1)
    cnt = 0
    for s in range(0, len(x) - nper, nper // 2):
        seg = x[s:s + nper]
        acc += np.abs(np.fft.rfft(seg * w)) ** 2
        cnt += 1
    if cnt == 0:
        raise SystemExit('segmento demasiado corto')
    acc = acc / cnt
    f = np.fft.rfftfreq(nper, 1.0 / SR)
    return f, acc


def smooth_db(f, p, oct_frac=6):
    """Suavizado en log-frecuencia (1/oct_frac de octava)."""
    db = 10 * np.log10(np.maximum(p, 1e-30))
    out = np.empty_like(db)
    lf = np.log2(np.maximum(f, 1.0))
    for i in range(len(f)):
        m = np.abs(lf - lf[i]) <= (1.0 / oct_frac)
        out[i] = np.median(db[m]) if np.any(m) else db[i]
    return out


def estimate_corner(f, db, fmin=20.0, fmax=18000.0):
    """Corte = interseccion de la asintota pasante con la asintota -24 dB/oct.

    Iterativo: pasante = mediana de dB en [fmin, fc/1.6]; pendiente ajustada
    sobre la zona entre (pasante-15) y (pasante-45) dB.
    """
    sel = (f >= fmin) & (f <= fmax)
    ff, dd = f[sel], db[sel]
    if dd.size < 8:
        return float('nan'), float('nan'), float('nan')
    fc = 1000.0
    slope = float('nan')
    level = float('nan')
    for _ in range(6):
        m_pass = (ff >= fmin) & (ff <= max(fmin * 1.5, fc / 1.6))
        if not np.any(m_pass):
            m_pass = ff <= ff[0] * 1.5
        level = float(np.median(dd[m_pass]))
        m_slope = (dd < level - 15.0) & (dd > level - 45.0)
        if np.count_nonzero(m_slope) < 5:
            break
        lx = np.log2(ff[m_slope])
        A = np.vstack([lx, np.ones_like(lx)]).T
        slope, b = np.linalg.lstsq(A, dd[m_slope], rcond=None)[0]
        if slope >= -0.5:      # sin rodilla visible en la ventana
            fc = float('nan')
            break
        fc_new = 2.0 ** ((level - b) / slope)
        fc = float(min(max(fc_new, 10.0), 24000.0))
    slope_per_oct = float(slope * 1.0) if slope == slope else float('nan')
    return fc, level, slope_per_oct


def report(path):
    x, y = read_bin(path)
    m = 0.5 * (x + y)
    f, p = magnitude(m)
    db = smooth_db(f, p)
    fc, level, slope = estimate_corner(f, db)
    rms = 20 * np.log10(max(1e-12, np.sqrt(np.mean(m ** 2))))
    # pico espectral por encima de 30 Hz
    sel = (f > 30) & (f < 18000)
    fpk = f[sel][np.argmax(db[sel])]
    print(f'{os.path.basename(os.path.dirname(path))}/{os.path.basename(path)}')
    print(f'  RMS={rms:6.1f} dBFS  pico={fpk:7.1f} Hz  fc_est={fc:7.1f} Hz  '
          f'pendiente={slope:6.1f} dB/oct  pasante={level:6.1f} dB')
    # niveles de armonicos de C4
    h = []
    for n in range(1, 12):
        fh = n * C4
        if fh >= 18000:
            break
        i = int(np.argmin(np.abs(f - fh)))
        h.append(f'{fh:6.0f}:{db[i]:6.1f}')
    print('  armonicos dB: ' + ' '.join(h))
    return fc, level, slope


def report_ratio(ref_path, path):
    xr, yr = read_bin(ref_path)
    xt, yt = read_bin(path)
    fr, pr = magnitude(0.5 * (xr + yr))
    ft, pt = magnitude(0.5 * (xt + yt))
    db = smooth_db(fr, pt) - smooth_db(fr, pr)
    fc, level, slope = estimate_corner(fr, db)
    print(f'{os.path.basename(path)} / {os.path.basename(ref_path)}')
    print(f'  fc={fc:7.1f} Hz  pendiente={slope:6.1f} dB/oct  pasante={level:6.1f} dB')
    return fc, level, slope


if __name__ == '__main__':
    args = sys.argv[1:]
    if args and args[0] == '--ratio':
        report_ratio(args[1], args[2])
    else:
        for a in args:
            report(a)
