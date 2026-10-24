# -*- coding: utf-8 -*-
"""
Vuelca la memoria X/Y del DSP56362 emulado después de cargar un programa con un
cutoff dado, usando `MS2K_DSPDUMP` del emulador (read-only, built-in).

El modo `--headless` sin render sólo avanza ~0.3 s de tiempo de MCU, así que se
usa `--render-test` (como en la escalera) para llegar al dump a 4.3 s, después
del dump SysEx (3.6 s) y de la nota (4.0 s).

Uso:  python dsp_dump.py 20 100
Salida: dspdump_k<knob>.txt con las líneas [DSPDUMP] ...
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dsp_coeff_probe as P  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DUMP = '4.3:X:0:2000,4.3:Y:0:800'
SECONDS = 5


def run(knob):
    home = os.path.abspath(os.path.join(HERE, f'coeff_home_k{knob:03d}'))
    wav = os.path.join(HERE, f'dump_k{knob:03d}.wav')
    env = dict(os.environ, MS2K_HOME=home, MS2K_MIDIIN=P.midiin(knob), MS2K_DSPDUMP=DUMP)
    cmd = [P.EMU_EXE, '--headless', '--render-test', wav, '--duration', str(SECONDS)]
    out_path = os.path.join(HERE, f'dspdump_k{knob:03d}.txt')
    keep = 0
    with open(out_path, 'w', encoding='utf-8', errors='replace') as f:
        p = subprocess.Popen(cmd, env=env, cwd=home, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True, errors='replace', bufsize=1)
        for line in p.stdout:
            if line.startswith('[DSPDUMP]'):
                f.write(line)
                keep += 1
            elif line.startswith('[DSP56362] end'):
                f.write(line)
        p.wait(timeout=900)
    print(f'[k{knob:03d}] exit={p.returncode} dump_lines={keep}')


if __name__ == '__main__':
    for a in sys.argv[1:]:
        run(int(a))
