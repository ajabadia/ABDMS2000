# -*- coding: utf-8 -*-
"""
Curva CUTOFF(knob) del MS2000 emulado con excitacion de diente de sierra C1.

Por que no ruido: el generador de ruido del emu esta limitado en banda (~3 kHz),
asi que su espectro se confunde con el filtro. Con una sierra C1 (32.70 Hz) el
espectro de entrada es una serie armonica densa y conocida; dividiendo cada
corrida por la de knob 127 (filtro practicamente abierto) se cancela por
completo la forma del oscilador y queda |H(f)| del filtro.

Una sola corrida mide los 6 knobs: se encadenan dumps SysEx + notas de 0.4 s.

Uso:
  python cutoff_multinote.py            # render + extraccion
  python cutoff_multinote.py --analyze  # solo analisis
"""
import os
import shutil
import struct
import subprocess
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
LADDER = os.path.join(HERE, '..', 'ladder')
EMU_EXE = r"D:\desarrollos\ABDSynths\_RESOURCES\ms2000\ms2000r-emu-main\ms2000r-emu-main\build_vst\Release\ms2000_emulator.exe"
EMU_HOME_TPL = os.path.join(LADDER, 'emu_home_r1')
USER_BASE = 0x0A0400
T1 = 0x26
NAME = 0x00
SR = 48000
F0 = 65.406391325149658  # C2
NOTE = 36

KNOBS = [20, 40, 60, 80, 100, 127]
T_START = 3.6   # tiempo MCU del primer dump
T_STEP = 0.62   # separacion entre programas (el dump tarda ~93 ms en transmitirse)
# audio = tiempo MCU - 1.024 s (el render arranca con 1 s de arranque del MCU)

FIELD_OFF = {
    'VIBRATO': 6, 'OSC1_WAVE': 7, 'OSC2_FLAGS': 12, 'MIX_OSC1': 16,
    'MIX_OSC2': 17, 'MIX_NOISE': 18, 'FILTER_TYPE': 19, 'CUTOFF': 20,
    'RESONANCE': 21, 'FILTER_EG1_INT': 22, 'FILTER_VELO': 23,
    'FILTER_KEYTRACK': 24, 'AMP_LEVEL': 25, 'AMP_PAN': 26, 'AMP_FLAGS': 27,
    'AMP_VELO': 28, 'AMP_KEYTRACK': 29, 'EG1_A': 30, 'EG1_D': 31,
    'EG1_S': 32, 'EG1_R': 33, 'EG2_A': 34, 'EG2_D': 35, 'EG2_S': 36,
    'EG2_R': 37,
}


def setf(b, field, value, timbre=1):
    b[T1 + FIELD_OFF[field]] = value & 0xFF


def build_program(knob, reso=0, over=None):
    b = bytearray(open(os.path.join(LADDER, 'init_base.bin'), 'rb').read())
    assert len(b) == 254
    name = (f'DIAG SAW {knob:3d}' + ' ' * 12)[:12]
    for i, ch in enumerate(name):
        b[NAME + i] = ord(ch)
    b[0x13] = 0x00   # delay off
    b[0x14] = 0x28
    b[0x15] = 0x00
    b[0x16] = 0x00
    b[0x17] = 0x28   # modfx off
    b[0x18] = 0x00
    b[0x19] = 0x00
    setf(b, 'VIBRATO', 64)
    setf(b, 'OSC1_WAVE', 0)   # sierra
    setf(b, 'OSC2_FLAGS', 0)
    setf(b, 'MIX_OSC1', 127)
    setf(b, 'MIX_OSC2', 0)
    setf(b, 'MIX_NOISE', 0)
    setf(b, 'FILTER_TYPE', 0)  # LPF24
    setf(b, 'CUTOFF', knob)
    setf(b, 'RESONANCE', reso)
    setf(b, 'FILTER_EG1_INT', 64)
    setf(b, 'FILTER_VELO', 64)
    setf(b, 'FILTER_KEYTRACK', 64)
    setf(b, 'AMP_LEVEL', 127)
    setf(b, 'AMP_PAN', 64)
    setf(b, 'AMP_FLAGS', 0)
    setf(b, 'AMP_VELO', 64)
    setf(b, 'AMP_KEYTRACK', 64)
    for e in ('EG1', 'EG2'):
        setf(b, f'{e}_A', 0)
        setf(b, f'{e}_D', 0)
        setf(b, f'{e}_S', 127)
        setf(b, f'{e}_R', 0)
    b[T1 + 108 + FIELD_OFF['MIX_OSC1']] = 0
    b[T1 + 108 + FIELD_OFF['MIX_NOISE']] = 0
    for k, v in (over or {}).items():
        if k in FIELD_OFF:
            setf(b, k, v)
        elif k == 'EG1_S':
            setf(b, 'EG1_S', v)
        elif k == 'CUTOFF':
            setf(b, 'CUTOFF', v)
    return b


def pack_korg(src):
    out = bytearray()
    for g in range(0, len(src), 7):
        grp = src[g:g + 7]
        coll = 0
        for j, b in enumerate(grp):
            coll |= (b >> 7) << (6 - j)
        out.append(coll)
        out.extend(b & 0x7F for b in grp)
    return bytes(out)


def midiin(knobs, reso=0):
    ev = []
    for i, k in enumerate(knobs):
        t = T_START + T_STEP * i
        prog = build_program(k, reso)
        payload = pack_korg(prog)
        syx = [0xF0, 0x42, 0x30, 0x58, 0x40] + list(payload) + [0xF7]
        ev.append(f"{t:.2f}:" + ' '.join(f'{b:02X}' for b in syx))
        ev.append(f"{t + 0.12:.2f}:90 {NOTE:02X} 64")
        ev.append(f"{t + 0.50:.2f}:80 {NOTE:02X} 00")
    return ';'.join(ev)


def force_rmtree(path):
    def onerr(func, p, exc):
        try:
            os.chmod(p, 0o666)
            func(p)
        except OSError:
            pass
    shutil.rmtree(path, onerror=onerr)


def copy_rw(src, dst):
    if os.path.exists(dst):
        os.chmod(dst, 0o666)
    shutil.copyfile(src, dst)


def render(knobs, reso=0, duration=9):
    home = os.path.join(HERE, 'emu_home_multi')
    if os.path.isdir(home):
        force_rmtree(home)
    os.makedirs(home, exist_ok=True)
    for name in ('ms2000.nvram', 'x811v107.sys'):
        copy_rw(os.path.join(EMU_HOME_TPL, name), os.path.join(home, name))
    base = bytearray(open(os.path.join(EMU_HOME_TPL, 'flash.bin'), 'rb').read())
    base[USER_BASE + 2 * 254: USER_BASE + 3 * 254] = build_program(127, reso)
    open(os.path.join(home, 'flash.bin'), 'wb').write(bytes(base))

    wav = os.path.join(HERE, 'wave_multi.wav')
    env = dict(os.environ, MS2K_HOME=home, MS2K_MIDIIN=midiin(knobs, reso))
    cmd = [EMU_EXE, '--headless', '--render-test', wav, '--duration', str(duration)]
    log = os.path.join(HERE, 'emu_multi.log')
    with open(log, 'wb') as lf:
        p = subprocess.run(cmd, env=env, stdout=lf, stderr=subprocess.STDOUT,
                           cwd=home, timeout=900)
    try:
        with open(log, 'rb') as lf:
            head = lf.read(120000).decode('utf-8', 'replace')
            lf.seek(max(0, os.path.getsize(log) - 120000))
            tail = lf.read().decode('utf-8', 'replace')
        open(os.path.join(HERE, 'emu_multi_head.log'), 'w', encoding='utf-8').write(head)
        open(log, 'w', encoding='utf-8').write(tail)
    except OSError:
        pass
    print(f'[multi] emu exit {p.returncode}')
    return p.returncode == 0


def read_wav(path):
    d = open(path, 'rb').read()
    i, dpos = 12, None
    while i < len(d) - 8:
        cid = d[i:i + 4]
        sz = struct.unpack('<I', d[i + 4:i + 8])[0]
        if cid == b'data':
            dpos = i + 8
            break
        i += 8 + sz
    x = np.frombuffer(d[dpos:], dtype='<i4').astype(np.float64) / 2.0 ** 31
    return 0.5 * (x[0::2] + x[1::2])


def harmonic_levels(x, nfft=16384):
    w = np.hanning(len(x)) if len(x) == nfft else np.hanning(len(x))
    n = min(len(x), nfft)
    seg = x[:n]
    P = np.abs(np.fft.rfft(seg * np.hanning(n))) ** 2
    f = np.fft.rfftfreq(n, 1.0 / SR)
    lv = []
    for i in range(1, int(18000 / F0) + 1):
        fh = i * F0
        if fh >= 18000:
            break
        j = int(np.argmin(np.abs(f - fh)))
        lo, hi = max(1, j - 2), min(len(P) - 1, j + 3)
        lv.append((fh, P[lo:hi + 1].max()))
    return lv


def main():
    if '--analyze' not in sys.argv:
        if not render(KNOBS):
            return 1
    m = read_wav(os.path.join(HERE, 'wave_multi.wav'))
    print(f'[multi] frames={m.size} dur={m.size / SR:.2f} s')
    per = {}
    for i, k in enumerate(KNOBS):
        t_audio = (T_START + T_STEP * i + 0.12) - 1.024
        s = int((t_audio + 0.06) * SR)
        seg = m[s:s + 16000]
        if seg.size < 16000:
            print(f'[k{k}] segmento corto')
            continue
        rms = 20 * np.log10(max(1e-9, np.sqrt(np.mean(seg ** 2))))
        per[k] = harmonic_levels(seg)
        print(f'[k{k}] rms={rms:6.1f} dBFS  armonicos={len(per[k])}')
    if 127 not in per:
        print('falta la referencia k127')
        return 1
    ref = np.array([p for _, p in per[127]])
    # guardar tabla f, |H| dB por knob
    f = np.array([fh for fh, _ in per[127]])
    out = {'f': f}
    for k in KNOBS:
        if k in per and len(per[k]) == len(ref):
            p = np.array([v for _, v in per[k]])
            with np.errstate(divide='ignore'):
                out[k] = 10 * np.log10(p / ref)
    np.savez(os.path.join(HERE, 'harm_curves.npz'), **{str(k): v for k, v in out.items()})
    for k in KNOBS:
        if k in out:
            d = out[k]
            print(f'[k{k}] |H| 32-500 Hz (dB): ' +
                  ' '.join(f'{1000 * 0 - 0:.0f}' for _ in []) +
                  ' '.join(f'{d[i]:6.1f}' for i in range(min(12, len(d)))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
