# -*- coding: utf-8 -*-
"""Render emu + comparador para cada rung de la escalera.

MS2K_MIDIIN: Program Change #2 a los 4 s, Note C4 vel100 a los 4.2 s, off a los 4.7 s.
Recorta 24000 frames desde el primer sample > 1e-3 (escala +-1.0) y escribe
emu_preset_2.bin en cmp_rN; luego corre CalibrationTests_diag_rN --compare-emu.
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


def pack_korg(src):
    """Empaquetado 7->8 Korg (MSB del byte j en el bit 6-j del colector)."""
    out = bytearray()
    for g in range(0, len(src), 7):
        grp = src[g:g + 7]
        coll = 0
        for j, b in enumerate(grp):
            coll |= (b >> 7) << (6 - j)
        out.append(coll)
        out.extend(b & 0x7F for b in grp)
    return bytes(out)


def midiin_for(n):
    raw = open(os.path.join(HERE, f'rung_r{n}.bin'), 'rb').read()
    payload = pack_korg(raw)
    syx = [0xF0, 0x42, 0x30, 0x58, 0x40] + list(payload) + [0xF7]
    hexmsg = ' '.join(f'{b:02X}' for b in syx)
    # dump al edit buffer a 3.6 s; nota a 4.5 (pasa el parseo del SysEx); off a 5.0
    return f"3.6:{hexmsg};4.5:90 3C 64;5.0:80 3C 00"


USER_BASE = 0x0A0400  # payload USER #00 (254 B por slot); PRELOAD en 0x0B0000


def patch_flash(n):
    home = os.path.join(HERE, f'emu_home_r{n}')
    rung = open(os.path.join(HERE, f'rung_r{n}.bin'), 'rb').read()
    assert len(rung) == 254, len(rung)
    fpath = os.path.join(home, 'flash.bin')
    flash = bytearray(open(fpath, 'rb').read())
    off = USER_BASE + 2 * 254
    flash[off:off + 254] = rung
    open(fpath, 'wb').write(bytes(flash))
    # sin estado NVRAM heredado: la memoria viva sale del flash
    st = os.path.join(home, 'ms2000_flash_state.bin')
    if os.path.exists(st):
        os.remove(st)
    print(f"[R{n}] USER#2 parcheado en {fpath} @ {hex(off)}")


def render_rung(n):
    home = os.path.join(HERE, f'emu_home_r{n}')
    patch_flash(n)
    wav = os.path.join(HERE, f'wave_r{n}.wav')
    env = dict(os.environ, MS2K_HOME=home, MS2K_MIDIIN=midiin_for(n))
    cmd = [EMU_EXE, '--headless', '--render-test', wav, '--duration', '7']
    log = os.path.join(HERE, f'emu_r{n}.log')
    with open(log, 'wb') as lf:
        p = subprocess.run(cmd, env=env, stdout=lf, stderr=subprocess.STDOUT, cwd=home, timeout=300)
    if p.returncode != 0:
        print(f"[R{n}] emu exit {p.returncode}; ver {log}")
        return False
    logsize = os.path.getsize(log)
    print(f"[R{n}] log={logsize} bytes")

    data = open(wav, 'rb').read()
    # saltar cabecera RIFF hasta 'data'
    dpos = data.find(b'data') + 8
    samples = np.frombuffer(data[dpos:], dtype='<i4')
    samples = samples.astype(np.float32) / np.float32(2 ** 31)
    if samples.size % 2:
        print(f"[R{n}] WAV impar: {samples.size}")
        return False
    L = samples[0::2]
    R = samples[1::2]
    nframes = L.size
    # onset: primer frame con abs > 1e-3 (~-60 dBFS)
    peak = np.maximum(np.abs(L), np.abs(R))
    idx = np.nonzero(peak > 1e-3)[0]
    if idx.size == 0:
        print(f"[R{n}] WAV silencioso")
        return False
    onset = int(idx[0])
    print(f"[R{n}] frames={nframes} onset={onset} peak={20*np.log10(max(1e-6, peak.max())):.1f} dBFS")
    if nframes - onset < 24000:
        print(f"[R{n}] insuficiente: {nframes - onset} < 24000")
        return False
    crop = (L[onset:onset + 24000], R[onset:onset + 24000])
    cmp_dir = os.path.join(HERE, f'cmp_r{n}')
    os.makedirs(cmp_dir, exist_ok=True)
    out = os.path.join(cmp_dir, 'emu_preset_2.bin')
    with open(out, 'wb') as f:
        f.write(struct.pack('<I', 24000))
        f.write(crop[0].tobytes())
        f.write(crop[1].tobytes())
    print(f"[R{n}] emu bin listo -> {out}")
    return True


def compare_rung(n):
    exe = os.path.join(REPO, 'build', 'Release', f'CalibrationTests_diag_r{n}.exe')
    cmp_dir = os.path.join(HERE, f'cmp_r{n}')
    p = subprocess.run([exe, '--compare-emu', cmp_dir], capture_output=True, text=True, timeout=300)
    txt = p.stdout
    open(os.path.join(HERE, f'compare_r{n}.log'), 'w', encoding='utf-8', errors='replace').write(txt)
    # extraer bloque del preset #02
    keep, out_line = False, []
    for line in txt.splitlines():
        if line.startswith('  Preset '):
            keep = 'A03' in line
            if keep:
                out_line.append(line)
        elif keep and line.startswith('    '):
            out_line.append(line)
    print(f"--- [R{n}] comparador ---")
    print('\n'.join(out_line) if out_line else txt[-2000:])
    abd = os.path.join(cmp_dir, 'abd_preset_2.bin')
    if os.path.exists(abd):
        print(f"[R{n}] abd bin: {os.path.getsize(abd)} bytes")
    return 'DIAG R' in txt


def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    rungs = [int(only)] if only else range(1, 9)
    for n in rungs:
        if not render_rung(n):
            continue
        if not compare_rung(n):
            print(f"[R{n}] ATENCION: el comparador no imprinio 'DIAG R' (el exe usa otra fuente de datos?)")


if __name__ == '__main__':
    main()
