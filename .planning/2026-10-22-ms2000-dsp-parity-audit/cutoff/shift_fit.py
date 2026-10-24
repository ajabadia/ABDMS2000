# -*- coding: utf-8 -*-
"""
Factor de frecuencia emu/ABD por knob.

Toma |H(f)| del emu medida con la sierra (armonicos de C2 normalizados contra
la corrida de knob 127, que cancela la forma del oscilador) y la |H(f)| exacta
de ABD (respuesta al impulso). Para cada knob busca el factor s que mejor
superpone ambas curvas (RMS en dB) y reporta s con su residuo.

  s < 1  -> el filtro del emu corta MAS BAJO que ABD con el mismo knob
  s > 1  -> el filtro del emu corta MAS ALTO

Uso: python shift_fit.py
"""
import os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000
KNOBS = [20, 40, 60, 80, 100, 127]
NOMINAL = {20: 41.39, 40: 131.39, 60: 417.14, 80: 1324.33, 100: 4204.48, 127: 20000.0}


def abd_db(f, k, mode='cur'):
    tag = 'ir_coeff' if mode == 'coeff' else 'ir'
    names = [f'abd_{tag}_k{k:03d}_r000.f32', f'abd_{tag}_{k:03d}_r000.f32']
    path = None
    for n in names:
        q = os.path.join(HERE, 'abd_raw', n)
        if os.path.exists(q):
            path = q
            break
    h = np.fromfile(path, dtype='<f4').astype(np.float64)
    H = np.abs(np.fft.rfft(h * np.hanning(len(h))))
    fa = np.fft.rfftfreq(len(h), 1.0 / SR)
    db = 20 * np.log10(np.maximum(H, 1e-12))
    ref = np.median(db[(fa >= 30) & (fa <= 45)])
    db = db - ref
    return fa[1:], db[1:]


def emu_harms(npz, k):
    f = npz['f']
    db = npz[k]
    ok = np.isfinite(db)
    return f[ok], db[ok]


def fit_shift(fe, de, fa, da, lo=-46.0):
    """Busca s tal que de(f) ~ da(f/s). Devuelve (s, rms, n)."""
    m = de > lo
    fe, de = fe[m], de[m]
    if fe.size < 8:
        return float('nan'), float('nan'), 0
    best = (float('nan'), 1e9, 0)
    for s in np.geomspace(0.25, 4.0, 241):
        da_i = np.interp(np.log2(fe / s), np.log2(fa), da, left=-400, right=-400)
        sel = da_i > lo
        if np.count_nonzero(sel) < 8:
            continue
        d = de[sel] - da_i[sel]
        d = d - np.median(d)   # tolera diferencias de ganancia global
        r = float(np.sqrt(np.mean(d ** 2)))
        if r < best[1]:
            best = (s, r, int(np.count_nonzero(sel)))
    return best


def main():
    mode = 'coeff' if '--coeff' in os.sys.argv else 'cur'
    npz = np.load(os.path.join(HERE, 'harm_curves.npz'))
    print(f"{'knob':>4} {'fc_abd':>8} {'s=emu/abd':>10} {'fc_emu~':>9} {'rms':>6} {'n':>4} "
          f"{'s_atan':>7}")
    rows = []
    for k in KNOBS:
        if str(k) not in npz and k not in npz:
            continue
        key = str(k) if str(k) in npz else k
        fe, de = emu_harms(npz, key)
        fa, da = abd_db(fe, k, mode)
        s, r, n = fit_shift(fe, de, fa, da)
        # residuo sin corrimiento (s=1): cuanto se aparta ABD tal como esta
        da1 = np.interp(np.log2(fe), np.log2(fa), da, left=-400, right=-400)
        sel = (de > -46.0) & (da1 > -46.0)
        if np.count_nonzero(sel) > 8:
            d1 = de[sel] - da1[sel]
            r1 = float(np.sqrt(np.mean((d1 - np.median(d1)) ** 2)))
        else:
            r1 = float('nan')
        # prediccion de la hipotesis "el word ROM es el coeficiente g"
        w = {20: 0x0085CD, 40: 0x01A8CB, 60: 0x0544A1, 80: 0x10B9A3,
             100: 0x351958, 127: 0xFC9536}[k]
        s_atan = (np.arctan(w / 2 ** 23) * SR / np.pi) / NOMINAL[k]
        rows.append((k, NOMINAL[k], s, NOMINAL[k] * s, r, n, s_atan))
        print(f'{k:4d} {NOMINAL[k]:8.1f} {s:10.3f} {NOMINAL[k]*s:9.1f} {r:6.1f} {n:4d} '
              f'{s_atan:7.3f}  rms(s=1)={r1:5.1f}')
    a = np.array(rows, dtype=float)
    np.save(os.path.join(HERE, 'shift_rows.npy'), a)
    ok = a[np.isfinite(a[:, 2])]
    if ok.size:
        print(f'\ns medio (20..100): {np.mean(ok[ok[:,0]<=100,2]):.3f}  '
              f'dispersion {np.std(ok[ok[:,0]<=100,2]):.3f}')
        print('semitonos de corrimiento por knob (12*log2(s)): ' +
              ' '.join(f'{k:.0f}:{12*np.log2(s):+.1f}' for k, _, s, *_ in ok))


if __name__ == '__main__':
    main()
