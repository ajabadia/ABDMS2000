# -*- coding: utf-8 -*-
"""
Curva de filtro CUTOFF(knob): emu (WAV) vs ABD (f32 del harness).

Metrica comun:
 - Welch 4096 (Hann, 50% solape) sobre el tramo estable.
 - Normalizacion de cada curva a su maximo suavizado (pasante = 0 dB).
 - Esquina por interseccion de asintotas (pasante plana vs -24 dB/oct).
 - Desviacion RMS en dB entre las dos curvas en 20 Hz..18 kHz, que es la
   medida directa de paridad y no depende del estimador de esquina.

Uso:
  python cutoff_curve.py            # knobs 20/40/60/80/100/127
  python cutoff_curve.py 40 60
"""
import os
import struct
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000
NPER = 8192
ANALYZE = (20.0, 18000.0)
KNOBS = [20, 40, 60, 80, 100, 127]


def read_wav(path):
    d = open(path, 'rb').read()
    i, dpos, sr = 12, None, SR
    while i < len(d) - 8:
        cid = d[i:i + 4]
        sz = struct.unpack('<I', d[i + 4:i + 8])[0]
        if cid == b'fmt ':
            sr = struct.unpack('<I', d[i + 12:i + 16])[0]
        if cid == b'data':
            dpos = i + 8
            break
        i += 8 + sz
    x = np.frombuffer(d[dpos:], dtype='<i4').astype(np.float64) / 2.0 ** 31
    return 0.5 * (x[0::2] + x[1::2]), sr


def emu_signal(tag):
    m, sr = read_wav(os.path.join(HERE, f'wave_{tag}.wav'))
    assert sr == SR, sr
    idx = np.nonzero(np.abs(m) > 1e-4)[0]
    if idx.size == 0:
        raise SystemExit(f'wave_{tag}.wav silencioso')
    onset = int(idx[0])
    start = onset + 2000
    if m.size - start < 20000:
        raise SystemExit(f'wave_{tag}: tramo corto ({m.size - onset})')
    return m[start:start + 20000]


def ir_response(knob):
    """|H(f)|^2 exacta del filtro ABD desde su respuesta al impulso."""
    p = os.path.join(HERE, 'abd_raw', f'abd_ir_k{knob:03d}_r000.f32')
    h = np.fromfile(p, dtype='<f4').astype(np.float64)
    H = np.abs(np.fft.rfft(h * np.hanning(len(h)))) ** 2
    f = np.fft.rfftfreq(len(h), 1.0 / SR)
    return f, H


def abd_signal(knob):
    p = os.path.join(HERE, 'abd_raw', f'abd_k{knob:03d}_r000.f32')
    x = np.fromfile(p, dtype='<f4').astype(np.float64)
    return x[1000:1000 + 400000]


def welch(x):
    w = np.hanning(NPER)
    acc = np.zeros(NPER // 2 + 1)
    cnt = 0
    for s in range(0, len(x) - NPER, NPER // 2):
        acc += np.abs(np.fft.rfft(x[s:s + NPER] * w)) ** 2
        cnt += 1
    f = np.fft.rfftfreq(NPER, 1.0 / SR)
    return f, acc / max(1, cnt)


def smooth(f, p, oct_frac=8):
    db = 10 * np.log10(np.maximum(p, 1e-30))
    lf = np.log2(np.maximum(f, 1.0))
    out = np.empty_like(db)
    for i in range(len(f)):
        m = np.abs(lf - lf[i]) <= (1.0 / oct_frac)
        out[i] = np.median(db[m]) if np.any(m) else db[i]
    return out


REF_BAND = (26.0, 34.0)  # pasante de referencia (todos los cortes medidos son > 40 Hz)


def normalized(f, p):
    db = smooth(f, p)
    ref = np.median(db[(f >= REF_BAND[0]) & (f <= REF_BAND[1])])
    return db - ref


def corner(f, db):
    lo = (f >= ANALYZE[0]) & (f <= ANALYZE[1])
    ff, dd = f[lo], db[lo]
    if dd.size < 8:
        return float('nan'), float('nan')
    fc = 800.0
    slope = float('nan')
    for _ in range(8):
        m_pass = (ff <= max(ANALYZE[0] * 1.5, fc / 1.7))
        if np.count_nonzero(m_pass) < 3:
            m_pass = ff <= ANALYZE[0] * 2.5
        level = float(np.median(dd[m_pass]))
        m_sl = (dd < level - 12.0) & (dd > level - 40.0)
        if np.count_nonzero(m_sl) < 5:
            break
        lx = np.log2(ff[m_sl])
        A = np.vstack([lx, np.ones_like(lx)]).T
        slope, b = np.linalg.lstsq(A, dd[m_sl], rcond=None)[0]
        if slope > -0.5:
            return float('nan'), float(slope)
        fc = float(2.0 ** ((level - b) / slope))
    return fc, slope


def level_freq(f, db, target_db):
    """Frecuencia donde la curva cruza target_db (interpolacion log)."""
    lo = (f >= ANALYZE[0]) & (f <= ANALYZE[1])
    ff, dd = f[lo], db[lo]
    idx = np.nonzero(dd <= target_db)[0]
    if idx.size == 0 or idx[0] == 0:
        return float('nan')
    i = int(idx[0])
    x0, x1 = np.log2(ff[i - 1]), np.log2(ff[i])
    y0, y1 = dd[i - 1], dd[i]
    t = (target_db - y0) / (y1 - y0) if y1 != y0 else 0.0
    return float(2.0 ** (x0 + t * (x1 - x0)))


def main(knobs):
    # referencia emu: espectro de la fuente sin filtrar. Preferimos la corrida
    # HPF12 (--ref); si no existe, cae a knob 127 (LPF casi abierto, sesga el
    # extremo alto).
    ref_tag = 'k127' if '--ref127' in sys.argv or not os.path.exists(os.path.join(HERE, 'wave_ref.wav')) else 'ref'
    f, p = welch(emu_signal(ref_tag))
    ref_emu = smooth(f, p)
    print(f'[ref] {ref_tag}  (curvas emu normalizadas contra la fuente)\n')
    rows = []
    for k in knobs:
        f, p = welch(emu_signal(f'k{k}'))
        dbE = smooth(f, p) - ref_emu
        dbE -= np.median(dbE[(f >= REF_BAND[0]) & (f <= REF_BAND[1])])
        fa, pa = ir_response(k)
        dbA = normalized(fa, pa)

        fcE, slE = corner(f, dbE)
        fcA, slA = corner(fa, dbA)

        # comparacion en la grilla del emu, solo donde ambas curvas estan por
        # encima del piso de ruido (-45 dB); mas abajo el emu mide su propio floor
        m = (f >= ANALYZE[0]) & (f <= ANALYZE[1])
        ab = np.interp(np.log2(f[m]), np.log2(np.maximum(fa, 1.0)), dbA)
        both = (dbE[m] > -45.0) & (ab > -45.0)
        diff = (dbE[m] - ab)[both]
        rms = float(np.sqrt(np.mean(diff ** 2))) if diff.size else float('nan')
        iw = int(np.argmax(np.abs(diff))) if diff.size else 0
        fm = f[m][both]
        fmax = fm[iw] if diff.size else float('nan')
        dmax = diff[iw] if diff.size else float('nan')
        rows.append((k, fcE, slE, fcA, slA, level_freq(f, dbE, -6.0),
                     level_freq(fa, dbA, -6.0), rms, fmax, dmax))
        print(f'knob {k:3d} | emu fc={fcE:7.1f} Hz (slope {slE:5.1f} dB/oct) '
              f'f-6={level_freq(f, dbE, -6.0):7.1f} | '
              f'ABD fc={fcA:7.1f} Hz (slope {slA:5.1f}) f-6={level_freq(fa, dbA, -6.0):7.1f} | '
              f'RMS dev={rms:5.1f} dB (max {dmax:+5.1f} dB @ {fmax:6.0f} Hz)')
    print()
    print('k,emu_fc,abd_fc,ratio,emu_f6,abd_f6,rms_dev,max_dev_hz,max_dev_db')
    for k, fcE, slE, fcA, slA, f6E, f6A, rms, fmax, dmax in rows:
        r = fcE / fcA if fcA == fcA and fcA else float('nan')
        print(f'{k},{fcE:.1f},{fcA:.1f},{r:.3f},{f6E:.1f},{f6A:.1f},{rms:.1f},{fmax:.0f},{dmax:+.1f}')
    np.save(os.path.join(HERE, 'curve_rows.npy'), np.array(rows))


if __name__ == '__main__':
    ks = [int(a) for a in sys.argv[1:] if a.isdigit()]
    main(ks if ks else KNOBS)
