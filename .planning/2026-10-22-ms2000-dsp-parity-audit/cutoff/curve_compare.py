# -*- coding: utf-8 -*-
"""
Curva de cutoff del VCF: coeficiente que el firmware escribe en el DSP vs tablas ROM.

Medición
--------
`dspdump_kNNN.txt` son volcados de la X: del DSP (t=4.3 s) hechos por el emulador
ms2000r-emu con `MS2K_DSPDUMP`, tras inyectar el programa por SysEx al edit buffer.
El bloque por voz arranca en X:0x4B0 con stride 0x0A (4 voces):

    X[0x4B0 + 0x0A*i] = coeficiente de cutoff  (Q23, 24-bit)
    X[0x4B1 + 0x0A*i] = coeficiente de resonancia

Uso
---
    python curve_compare.py          # tabla resumida + CSV de los 128 knobs
"""
import glob
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
HEADER = os.path.join(ROOT, 'Source', 'Core', 'MS2000FirmwareTables.h')

MASK = 0xFFFFFF
FS = 48000.0
BASE = 0x4B0
STRIDE = 0x0A
VOICES = 4
FC_BASE_IDX = 47        # ABD: kRomFilterCutoffQ23[47 + knob]
MP_BASE_IDX = 128       # firmware: kRomMasterPitchQ23[128 + knob]


def s24(v):
    v &= MASK
    return v - (1 << 24) if v & 0x800000 else v


def parse_dump(path):
    xs, t0 = {}, None
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'\[DSPDUMP\] t=([\d.]+) X:([0-9A-F]{6}) (.*)', line.strip())
        if not m:
            continue
        t = m.group(1)
        if t0 is None:
            t0 = t
        if t != t0:
            continue
        a = int(m.group(2), 16)
        for i, w in enumerate(m.group(3).split()):
            xs[a + i] = int(w, 16)
    return xs


def rom_table(name):
    txt = open(HEADER, encoding='utf-8', errors='replace').read()
    m = re.search(name + r'\[256\]\s*=\s*\{(.*?)\};', txt, re.S)
    if not m:
        sys.exit(f'no encontré {name} en {HEADER}')
    vals = [int(w, 16) for w in re.findall(r'0x([0-9A-Fa-f]{6,8})[uU]?', m.group(1))]
    if len(vals) != 256:
        sys.exit(f'{name}: {len(vals)} entradas, esperaba 256')
    return vals


def hz_tan(word):
    """frecuencia si el word es tan(pi f / fs) en Q23 (lectura pre-warp)."""
    return (FS / math.pi) * math.atan((word & MASK) / 2.0 ** 23)


def hz_abd(word):
    """mapeo que usa hoy ABDMS2000::getHardwareFilterCutoffHz."""
    return 20000.0 * (word & MASK) / 0xFC9536


def measured():
    out = {}
    for path in sorted(glob.glob(os.path.join(HERE, 'dspdump_k[0-9][0-9][0-9].txt'))):
        knob = int(re.search(r'k(\d{3})', os.path.basename(path)).group(1))
        xs = parse_dump(path)
        if BASE not in xs:
            continue
        out[knob] = dict(
            coeff=xs[BASE],
            voices=[xs.get(BASE + STRIDE * i) for i in range(VOICES)],
            reso=xs.get(BASE + 1),
        )
    return out


def main():
    fc = rom_table('kRomFilterCutoffQ23')
    mp = rom_table('kRomMasterPitchQ23')
    meas = measured()

    lines = []
    add = lines.append
    add('== Coeficiente medido vs tabla ROM del firmware (kRomMasterPitchQ23[128+knob]) ==')
    add(f"{'knob':>4} {'HW coeff':>9} {'idx':>4} {'prediccion':>11} {'match':>6} {'voces iguales':>14}")
    ok_all = True
    for k in sorted(meas):
        m = meas[k]
        pred = mp[min(255, MP_BASE_IDX + k)]
        same = len(set(m['voices'])) == 1
        ok = m['coeff'] == pred
        ok_all &= ok
        add(f"{k:>4} 0x{m['coeff']:06X} {MP_BASE_IDX + k:>4} 0x{pred:06X} "
            f"{'OK' if ok else 'NO':>6} {str(same):>14}")
    add(f"\nregla hw(knob) = kRomMasterPitchQ23[128+knob]: "
        f"{'confirmada' if ok_all else 'NO confirmada'} en {len(meas)} knobs medidos")

    add('\n== Curva completa (128 knobs) ==')
    hdr = (f"{'knob':>4} {'HW coeff':>9} {'ABD word':>9} {'ratio':>7} {'dSemis':>7} "
           f"{'Hz HW*':>8} {'Hz ABD':>8}")
    add(hdr)
    add('-' * len(hdr))
    rows = []
    for k in range(128):
        hw = mp[min(255, MP_BASE_IDX + k)]
        abd = fc[min(255, FC_BASE_IDX + k)]
        ratio = hw / abd
        d = 12.0 * math.log2(ratio)
        rows.append((k, hw, abd, ratio, d))
        add(f"{k:>4} 0x{hw:06X} 0x{abd:06X} {ratio:7.4f} {d:+7.2f} "
            f"{hz_tan(hw):8.1f} {hz_abd(abd):8.1f}")
    add('\n* Hz HW = lectura tan(pi*f/fs) del word Q23 (pre-warp), con fs=48 kHz.')

    # resumen por tramos
    add('\n== Resumen del error ABD vs firmware (semitonos, + = ABD cerrado de mas) ==')
    bands = [(0, 15), (16, 39), (40, 63), (64, 87), (88, 111), (112, 127)]
    for a, b in bands:
        seg = [r[4] for r in rows if a <= r[0] <= b]
        # error del ABD en semitonos respecto al hardware: cuantos semitonos hay que
        # mover el cutoff del ABD para llegar al word del firmware
        neg = [-s for s in seg]
        add(f"  knob {a:>3}-{b:>3}: dSemis hw/abd {min(neg):+6.2f} .. {max(neg):+6.2f}  "
            f"media {sum(neg) / len(neg):+6.2f}")

    txt = '\n'.join(lines)
    print(txt)
    open(os.path.join(HERE, 'curve_compare.txt'), 'w', encoding='utf-8').write(txt + '\n')

    csv_path = os.path.join(HERE, 'curve_compare.csv')
    with open(csv_path, 'w', encoding='utf-8') as f:
        f.write('knob,hw_coeff,hw_idx_mp,abd_word,abd_idx_fc,ratio,d_semitones,'
                'hz_hw_tan,hz_abd\n')
        for k, hw, abd, ratio, d in rows:
            f.write(f'{k},0x{hw:06X},{MP_BASE_IDX + k},0x{abd:06X},{FC_BASE_IDX + k},'
                    f'{ratio:.6f},{d:+.4f},{hz_tan(hw):.3f},{hz_abd(abd):.3f}\n')
    print(f'\n-> {os.path.basename(csv_path)}')
    if meas:
        add_w = ', '.join(f'{k}:0x{meas[k]["coeff"]:06X}' for k in sorted(meas))
        print(f'medidos: {add_w}')


if __name__ == '__main__':
    main()
