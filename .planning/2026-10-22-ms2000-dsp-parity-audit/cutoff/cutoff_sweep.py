# -*- coding: utf-8 -*-
"""
Barrida de la curva CUTOFF(knob) en el MS2000 emulado.

Programa de diagnostico: ruido blanco -> LPF24 con resonancia 0, EG planas,
sin FX. La respuesta medida es directamente |H(f)| del filtro (ruido blanco
como excitacion plana), y dividiendo por la corrida de knob 127 (filtro
practicamente abierto) se cancela cualquier coloracion de la fuente.

Uso:
  python cutoff_sweep.py 64         # un knob
  python cutoff_sweep.py            # 20/40/60/80/100/127
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
EMU_HOME_TPL = os.path.join(LADDER, 'emu_home_r1')  # ROM original + rung R1 en USER#2 (se sobrescribe)
USER_BASE = 0x0A0400
T1 = 0x26
NAME = 0x00

FIELD_OFF = {
    'VIBRATO': 6, 'OSC1_WAVE': 7, 'OSC2_FLAGS': 12, 'MIX_OSC1': 16,
    'MIX_OSC2': 17, 'MIX_NOISE': 18, 'FILTER_TYPE': 19, 'CUTOFF': 20,
    'RESONANCE': 21, 'FILTER_EG1_INT': 22, 'FILTER_VELO': 23,
    'FILTER_KEYTRACK': 24, 'AMP_LEVEL': 25, 'AMP_PAN': 26, 'AMP_FLAGS': 27,
    'AMP_VELO': 28, 'AMP_KEYTRACK': 29, 'EG1_A': 30, 'EG1_D': 31,
    'EG1_S': 32, 'EG1_R': 33, 'EG2_A': 34, 'EG2_D': 35, 'EG2_S': 36,
    'EG2_R': 37,
}
KNOBS = [20, 40, 60, 80, 100, 127]


def setf(b, field, value, timbre=1):
    b[T1 + FIELD_OFF[field]] = value & 0xFF


def build_program(knob, filt_type=0):
    b = bytearray(open(os.path.join(LADDER, 'init_base.bin'), 'rb').read())
    assert len(b) == 254, len(b)
    name = (f'DIAG CUT {knob:3d}' + ' ' * 12)[:12]
    for i, ch in enumerate(name):
        b[NAME + i] = ord(ch)
    # FX off
    b[0x13] = 0x00
    b[0x14] = 0x28
    b[0x15] = 0x00
    b[0x16] = 0x00
    b[0x17] = 0x28
    b[0x18] = 0x00
    b[0x19] = 0x00
    # fuente = ruido
    setf(b, 'VIBRATO', 64)
    setf(b, 'OSC1_WAVE', 0)
    setf(b, 'OSC2_FLAGS', 0)
    setf(b, 'MIX_OSC1', 0)
    setf(b, 'MIX_OSC2', 0)
    setf(b, 'MIX_NOISE', 127)
    # filtro
    setf(b, 'FILTER_TYPE', filt_type)  # 0=LPF24
    setf(b, 'CUTOFF', knob)
    setf(b, 'RESONANCE', 0)
    setf(b, 'FILTER_EG1_INT', 64)
    setf(b, 'FILTER_VELO', 64)
    setf(b, 'FILTER_KEYTRACK', 64)
    # amp plano
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
    # timbre 2 mudo
    b[T1 + 108 + FIELD_OFF['MIX_OSC1']] = 0
    b[T1 + 108 + FIELD_OFF['MIX_NOISE']] = 0
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


def midiin_for(prog):
    payload = pack_korg(prog)
    syx = [0xF0, 0x42, 0x30, 0x58, 0x40] + list(payload) + [0xF7]
    hexmsg = ' '.join(f'{b:02X}' for b in syx)
    return f"3.6:{hexmsg};4.5:90 3C 64;5.0:80 3C 00"


def render(knob, duration=7, filt_type=0, tag=None):
    tag = tag or f'k{knob}'
    home = os.path.join(HERE, f'emu_home_{tag}')
    if os.path.isdir(home):
        shutil.rmtree(home, ignore_errors=True)
    os.makedirs(home, exist_ok=True)
    for name in ('ms2000.nvram', 'x811v107.sys'):
        shutil.copy(os.path.join(EMU_HOME_TPL, name), os.path.join(home, name))
    prog = build_program(knob, filt_type)
    open(os.path.join(HERE, f'prog_{tag}.bin'), 'wb').write(bytes(prog))
    flash = bytearray(open(os.path.join(EMU_HOME_TPL, 'flash.bin'), 'rb').read())
    off = USER_BASE + 2 * 254
    flash[off:off + 254] = prog
    open(os.path.join(home, 'flash.bin'), 'wb').write(bytes(flash))

    wav = os.path.join(HERE, f'wave_{tag}.wav')
    env = dict(os.environ, MS2K_HOME=home, MS2K_MIDIIN=midiin_for(prog))
    cmd = [EMU_EXE, '--headless', '--render-test', wav, '--duration', str(duration)]
    log = os.path.join(HERE, f'emu_{tag}.log')
    with open(log, 'wb') as lf:
        p = subprocess.run(cmd, env=env, stdout=lf, stderr=subprocess.STDOUT,
                           cwd=home, timeout=600)
    # el emu escupe GB de log; conservar solo la cola
    try:
        with open(log, 'rb') as lf:
            lf.seek(max(0, os.path.getsize(log) - 120000))
            tail = lf.read().decode('utf-8', 'replace')
        with open(log, 'w', encoding='utf-8') as lf:
            lf.write(tail)
    except OSError:
        pass
    if p.returncode != 0:
        print(f'[{tag}] emu exit {p.returncode}; ver {log}')
        return False
    data = open(wav, 'rb').read()
    dpos = data.find(b'data') + 8
    samples = np.frombuffer(data[dpos:], dtype='<i4').astype(np.float32) / np.float32(2 ** 31)
    L, R = samples[0::2], samples[1::2]
    peak = np.maximum(np.abs(L), np.abs(R))
    idx = np.nonzero(peak > 1e-4)[0]
    if idx.size == 0:
        print(f'[{tag}] WAV silencioso (peak {peak.max():.2e})')
        return False
    onset = int(idx[0])
    avail = L.size - onset
    print(f'[{tag}] frames={L.size} onset={onset} peak={20*np.log10(max(1e-9, peak.max())):.1f} dBFS')
    # recorte estable: 2000 frames tras el onset, 20000 frames (termina antes
    # del note-off, que en tiempo de audio cae 0.5 s despues del note-on)
    start = onset + 2000
    if L.size - start < 20000:
        print(f'[{tag}] insuficiente: {L.size - onset}')
        return False
    outdir = os.path.join(HERE, f'cmp_{tag}')
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, 'emu_preset_2.bin')
    with open(out, 'wb') as f:
        f.write(struct.pack('<I', 20000))
        f.write(L[start:start + 20000].astype('<f4').tobytes())
        f.write(R[start:start + 20000].astype('<f4').tobytes())
    print(f'[{tag}] -> {out}')
    return True


if __name__ == '__main__':
    args = sys.argv[1:]
    if args and args[0] == '--ref':
        # fuente sola: HPF12 en el cutoff mas bajo, pasa todo el ruido
        render(20, filt_type=3, tag='ref')
    else:
        knobs = [int(a) for a in args] if args else KNOBS
        for k in knobs:
            render(k)
