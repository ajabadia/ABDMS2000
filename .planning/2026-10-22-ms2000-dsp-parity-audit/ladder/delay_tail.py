# -*- coding: utf-8 -*-
"""Cola de delay tras el note-off: emu vs ABD sobre `cmp_r8t/`.

Ambas capturas son 216 000 frames (4.5 s); el note-off cae a los 24 000
(0.5 s). Se mide la envolvente RMS en ventanas de 20 ms, se buscan los picos
de repetición del delay y se reporta:

  - intervalo entre repeticiones (ms) y su equivalente en BPM
  - caída de nivel por repetición (dB) -> feedback aparente
  - nivel de la cola a 1 s y 2 s del note-off, relativo al pico

Uso: python delay_tail.py
"""
import os
import struct

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000.0
NOTE_OFF = 24000


def read_bin(path):
    b = open(path, 'rb').read()
    n = struct.unpack('<I', b[:4])[0]
    d = np.frombuffer(b[4:4 + n * 8], dtype='<f4').reshape(2, n)
    return d[0], d[1]


def envelope(mono, win=960):  # 20 ms
    n = len(mono) // win
    return np.sqrt(np.mean(mono[:n * win].reshape(n, win) ** 2, axis=1))


def peaks(env_db, t, t0=0.55, thresh_db=-45.0, min_gap=0.06):
    idx = []
    for i in range(1, len(env_db) - 1):
        if t[i] < t0 or env_db[i] < thresh_db:
            continue
        if env_db[i] >= env_db[i - 1] and env_db[i] > env_db[i + 1]:
            if not idx or (t[i] - t[idx[-1]]) >= min_gap:
                idx.append(i)
            elif env_db[i] > env_db[idx[-1]]:
                idx[-1] = i
    return idx


def report(side, mono):
    env = envelope(mono)
    t = np.arange(len(env)) * 0.02
    db = 20 * np.log10(env / max(env.max(), 1e-20) + 1e-20)
    note_off = NOTE_OFF / SR
    print(f'\n[{side}] pico global {db.max():.1f} dB (t={t[int(np.argmax(db))]:.2f} s)')
    for dt in (0.5, 1.0, 2.0, 3.0):
        tt = note_off + dt
        if tt < t[-1]:
            i = int(tt / 0.02)
            print(f'   +{dt:.1f} s tras note-off: {db[i]:6.1f} dB rel. pico')
    pk = peaks(db, t)
    print(f'   repeticiones detectadas: {len(pk)}')
    for i in pk[:8]:
        print(f'     t={t[i]:5.2f} s  {db[i]:6.1f} dB')
    if len(pk) >= 3:
        d = np.diff([t[i] for i in pk[:6]])
        print(f'   intervalo medio: {1000*np.mean(d):.0f} ms ({60/np.mean(d):.1f} BPM)')
        drop = np.diff([db[i] for i in pk[:6]])
        print(f'   caída por repetición: {np.mean(drop):+.1f} dB')


def main():
    for side in ('emu', 'abd'):
        path = os.path.join(HERE, 'cmp_r8t', f'{side}_preset_2.bin')
        if not os.path.exists(path):
            print(f'falta {path}')
            continue
        L, R = read_bin(path)
        report(side, (L + R) * 0.5)


if __name__ == '__main__':
    main()
