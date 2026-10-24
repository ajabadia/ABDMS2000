# -*- coding: utf-8 -*-
"""
Ajuste de la ley coeficiente->Hz del VCF del MS2000.

Datos: frecuencia de autoscilación medida en el emulador (ms2000r-emu, reso 127,
nota C4 liberada) para el word que el firmware escribe por knob:
    w = kRomMasterPitchQ23[128 + knob] / 2^23

Se prueban tres leyes candidatas y se reporta el error de cada una:
  A) fc = (fs/pi) * asin(min(2w, 0.9297))     [coeficiente tipo Chamberlin]
  B) fc = (fs/pi) * atan(2w)                  [TPT con g = 2w]
  C) fc = (2*fs/pi) * atan(w)                 [TPT con g = tan(pi f/(2fs))]

Uso: python law_fit.py
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
FS = 48000.0
CLAMP = 0.9297

# medida de autoscilación (Hz) por knob
MEAS = {20: 207.5, 60: 2090.0, 90: 9348.2, 100: 13696.5, 127: 18186.4}


def words():
    out = {}
    for line in open(os.path.join(HERE, 'curve_compare.csv'), encoding='utf-8'):
        p = line.strip().split(',')
        if p[0] == 'knob':
            continue
        out[int(p[0])] = int(p[1], 16) / float(1 << 23)
    return out


def law_a(w):
    return (FS / math.pi) * math.asin(min(2.0 * w, CLAMP))


def law_b(w):
    return (FS / math.pi) * math.atan(2.0 * w)


def law_c(w):
    return (2.0 * FS / math.pi) * math.atan(w)


def main():
    ws = words()
    print(f"{'knob':>4} {'word':>9} {'2w':>8} {'medido':>9} "
          f"{'A asin':>9} {'errA%':>7} {'B atan':>9} {'errB%':>7} {'C 2atan':>9} {'errC%':>7}")
    for k in sorted(MEAS):
        w = ws[k]
        a, b, c = law_a(w), law_b(w), law_c(w)
        m = MEAS[k]
        print(f'{k:>4} 0x{int(round(ws[k] * (1 << 23))):07X} {2 * w:8.5f} {m:9.1f} '
              f'{a:9.1f} {100 * (a / m - 1):+7.2f} {b:9.1f} {100 * (b / m - 1):+7.2f} '
              f'{c:9.1f} {100 * (c / m - 1):+7.2f}')

    print('\nCoeficiente implícito c = sin(pi*f/fs) frente a 2w:')
    for k in sorted(MEAS):
        w = ws[k]
        c = math.sin(math.pi * MEAS[k] / FS)
        print(f'  knob {k:3d}: c={c:.6f}  2w={2 * w:.6f}  2w/c={2 * w / c:.4f}  '
              f'{"CLAMP 0.9297" if 2 * w > CLAMP else ""}')


if __name__ == '__main__':
    main()
