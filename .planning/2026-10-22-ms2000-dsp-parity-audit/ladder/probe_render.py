# -*- coding: utf-8 -*-
"""Renderiza y compara los rungs nuevos (`probe_build.py`).

  python probe_render.py r4b r5p8 r5m32 r5p63

Por rung: parchea USER#2 del flash aislado, inyecta el programa por SysEx al
edit buffer, nota C4/100, recorta desde el onset y corre el exe ABD parcheado.
Opcional `--tail`: 216 000 frames (4.5 s) con note-off a los 24 000 para medir
la cola de delay en ambos lados.
"""
import os
import struct
import subprocess
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
EMU_EXE = r"D:\desarrollos\ABDSynths\_RESOURCES\ms2000\ms2000r-emu-main\ms2000r-emu-main\build_vst\Release\ms2000_emulator.exe"
USER_BASE = 0x0A0400
NOTE_OFF = 24000
RATE_FRAMES = 240000  # 5 s per capture, enough to resolve the slow LFO1 settings


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


def midiin(name):
    prog = open(os.path.join(HERE, f'rung_{name}.bin'), 'rb').read()
    syx = [0xF0, 0x42, 0x30, 0x58, 0x40] + list(pack_korg(prog)) + [0xF7]
    msg = ' '.join(f'{b:02X}' for b in syx)
    return f'3.6:{msg};4.5:90 3C 64;5.0:80 3C 00'


def patch_flash(name):
    home = os.path.join(HERE, f'emu_home_{name}')
    rung = open(os.path.join(HERE, f'rung_{name}.bin'), 'rb').read()
    fpath = os.path.join(home, 'flash.bin')
    flash = bytearray(open(fpath, 'rb').read())
    off = USER_BASE + 2 * 254
    flash[off:off + 254] = rung
    open(fpath, 'wb').write(bytes(flash))
    st = os.path.join(home, 'ms2000_flash_state.bin')
    if os.path.exists(st):
        os.remove(st)


def read_wav(path):
    w = wave.open(path, 'rb')
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(n)
    w.close()
    data = np.frombuffer(raw, dtype='<i4' if sw == 4 else '<i2').astype(np.float32)
    data /= 2 ** 31 if sw == 4 else 2 ** 15
    data = data.reshape(-1, ch)
    return data[:, 0], data[:, 1]


def render(name, frames=24000, duration=7):
    patch_flash(name)
    wav = os.path.join(HERE, f'wave_{name}.wav')
    env = dict(os.environ, MS2K_HOME=os.path.join(HERE, f'emu_home_{name}'), MS2K_MIDIIN=midiin(name))
    cmd = [EMU_EXE, '--headless', '--render-test', wav, '--duration', str(duration)]
    log = os.path.join(HERE, f'emu_{name}.log')
    with open(log, 'wb') as lf:
        p = subprocess.run(cmd, env=env, stdout=lf, stderr=subprocess.STDOUT,
                           cwd=os.path.join(HERE, f'emu_home_{name}'), timeout=900)
    # el emu escribe GBs de log: conservar solo la cola
    try:
        size = os.path.getsize(log)
        with open(log, 'rb') as lf:
            lf.seek(max(0, size - 200000))
            tail = lf.read().decode('utf-8', 'replace')
        with open(log, 'w', encoding='utf-8') as lf:
            lf.write(tail)
    except OSError:
        pass
    if p.returncode != 0:
        print(f'[{name}] emu exit {p.returncode}; ver {log}')
        return False
    L, R = read_wav(wav)
    peak = np.maximum(np.abs(L), np.abs(R))
    idx = np.nonzero(peak > 1e-3)[0]
    onset = int(idx[0])
    print(f'[{name}] frames={L.size} onset={onset} peak={20*np.log10(max(1e-9, peak.max())):.1f} dBFS')
    if L.size - onset < frames:
        print(f'[{name}] insuficiente: {L.size - onset} < {frames}')
        return False
    outdir = os.path.join(HERE, f'cmp_{name}')
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, 'emu_preset_2.bin')
    with open(out, 'wb') as f:
        f.write(struct.pack('<I', frames))
        f.write(L[onset:onset + frames].astype('<f4').tobytes())
        f.write(R[onset:onset + frames].astype('<f4').tobytes())
    print(f'[{name}] emu bin -> {out}')
    return True


def compare(name, note_off=-1):
    exe = os.path.join(REPO, 'build', 'Release', f'CalibrationTests_probe_{name}.exe')
    cmp_dir = os.path.join(HERE, f'cmp_{name}')
    args = [exe, '--compare-emu', cmp_dir]
    if note_off >= 0:
        args.append(str(note_off))
    p = subprocess.run(args, capture_output=True, text=True, timeout=900)
    txt = p.stdout
    open(os.path.join(HERE, f'compare_{name}.log'), 'w', encoding='utf-8', errors='replace').write(txt)
    keep, lines = False, []
    for line in txt.splitlines():
        if line.startswith('  Preset '):
            keep = 'A03' in line
            if keep:
                lines.append(line)
        elif keep and line.startswith('    '):
            lines.append(line)
    print(f'--- [{name}] ---')
    print('\n'.join(lines) if lines else txt[-1500:])
    return bool(lines)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    tail = '--tail' in sys.argv
    frames = 216000 if tail else 24000
    duration = 11 if tail else 7
    for name in args:
        if render(name, frames, duration):
            compare(name, NOTE_OFF if tail else -1)


if __name__ == '__main__':
    main()
