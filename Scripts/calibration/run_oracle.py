#!/usr/bin/env python3
"""P0.2 — Oráculo ms2000r-emu: de un preset a un WAV (o a emu_preset_<idx>.bin).

El emulador es un instrumento de medida externo (GPL-3.0): se ejecuta como proceso
y sólo se guardan sus salidas (WAV). No se enlaza su código.

Convenciones verificadas el 2026-11-04 sobre ms2000_emulator.exe (build_vst/Release):

  * `--audio --power-on --master-vol 100` son obligatorios. Sin ellos el log dice
    `gates: DSP=0 CODEC=0` y el WAV sale MUDO (293 KB de ceros).
  * La ruta de `--render-test` debe ser ABSOLUTA. Con ruta relativa el WAV aparece
    en el HOME del emulador (su carpeta del .exe), no en el cwd.
  * El WAV es PCM ENTERO de 32 bits (sampwidth=4, no float), 48 kHz estéreo.
  * El escritor DESCARTA el silencio inicial: 288.000 frames renderizados ->
    239.902 escritos. Por eso todo análisis se alinea por onset, nunca por índice.
  * Coste ~0,55x tiempo real.

SELECCIÓN DE PROGRAMA: se hace PARCHEADO EL FLASH, no por MIDI.
  El firmware carga siempre la ranura 0 al arrancar, así que se escribe el preset
  de fábrica deseado en 0x0A0400 y se arranca con esa imagen. El `Program Change`
  NO llega al firmware en este emulador: en el log de `[MIDI-IN]` aparece el byte
  de dato `0x00` pero nunca el status `0xC0`, y los renders de programa 0 y 127
  salen IDÉNTICOS bit a bit (md5 igual). Las notas sí entran (0x90 / 0x80).
  Ventaja añadida: no hay límite de 32 KB de variable de entorno, así que sirve
  igual para estímulos que modifiquen bytes del programa.

  La selección por flash también evita la trampa del banco de usuario: los 128
  programas se toman siempre de FACTORY_PRELOAD (0x0B0000), intactos, nunca de
  los 28 presets que el dueño anterior regrabó en 0x0A0400 sin cambiar el nombre.

Uso:
  # Puerta de aceptación: INIT Program (H16) + C4 -> 261,6 Hz
  python Scripts/calibration/run_oracle.py --gate

  # Un preset concreto -> WAV
  python Scripts/calibration/run_oracle.py --program 2 --out scratch/oracle/a03.wav

  # Los 8 presets que espera CalibrationTests -> tests/emu_preset_<idx>.bin
  python Scripts/calibration/run_oracle.py --bins

  # Copia de flash.bin con los 128 programas de fábrica en el banco de usuario
  python Scripts/calibration/run_oracle.py --make-factory-flash

Salidas: por defecto en scratch/oracle/ (ignorado por git).
"""

from __future__ import annotations

import argparse
import array
import math
import os
import shutil
import struct
import subprocess
import sys
import wave

# ----------------------------------------------------------------------------
# Rutas por defecto (sobreescribibles por entorno)
# ----------------------------------------------------------------------------
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_EMU = os.path.join(
    os.path.dirname(REPO),
    "_RESOURCES", "ms2000", "ms2000r-emu-main", "ms2000r-emu-main",
    "build_vst", "Release", "ms2000_emulator.exe",
)
DEFAULT_FLASH = os.path.join(
    os.path.dirname(REPO),
    "_RESOURCES", "ms2000", "firmware", "MS2000_Full_Firmware", "flash.bin",
)
SCRATCH = os.path.join(REPO, "scratch", "oracle")
FACTORY_FLASH = os.path.join(SCRATCH, "flash_factory.bin")

HW_SR = 48000
C4_HZ = 261.6255653005986
# Índices que consume tests/CalibrationTests.cpp::compareIdenticalPresetsWithEmu
DEFAULT_INDICES = [2, 5, 9, 16, 20, 22, 23, 24]

# Segmentos del flash.bin (ver DOCS/MS2000_Firmware_DSP_Reference.md §7.1)
USER_PROGRAMS = (0x0A0400, 0x0A82FF)      # 128 x 254 B regrabables
FACTORY_PRELOAD = (0x0B0000, 0x0B7EFF)    # 128 x 254 B de fábrica, intactos
PROGRAM_BYTES = 254

# Offsets absolutos dentro del programa de 254 B: TIMBRE1_START (0x26) + campo de
# Source/MIDI/MS2000HardwareProgram.h::ti. Verificados contra el INIT Program real
# del flash (#127): osc1Wave=0x00 (Saw), mixOsc1=0x7F, cutoff=0x7F, reso=0x0A,
# filterType=0x01, ampLevel=0x7F, ampPan=0x40 (centro).
TI_OSC1_WAVE = 0x26 + 7       # 45
TI_OSC1_CTRL1 = 0x26 + 8      # 46
TI_MIX_OSC1 = 0x26 + 16       # 54
TI_MIX_OSC2 = 0x26 + 17       # 55
TI_MIX_NOISE = 0x26 + 18      # 56
TI_FILTER_TYPE = 0x26 + 19    # 57
TI_CUTOFF = 0x26 + 20         # 58
TI_RESONANCE = 0x26 + 21      # 59
TI_AMP_LEVEL = 0x26 + 25      # 63
TI_AMP_PAN = 0x26 + 26        # 64

USER_GLOBAL = (0x0A0010, 200)             # Global Data de usuario (0xC8 = 100 palabras)
# Plantilla de fábrica de teclado (la del rack vive en 0x0B8200 y sólo la usa el
# Factory Reset cuando btst #7,@0xFFF6C9 dice que la placa es MS2000R).
# Tamaño 200 B, no 234: GLOBAL 8: PRELOAD ejecuta `mov.w #0x64,R2` y mueve 100
# palabras = 0xC8 bytes (0x0B8100..0x0B81C7, último elemento de la tabla de
# Program Change). Los bytes 200..233 de 0x0A00D8 son residuo tras el bloque y
# NO forman parte de la Global Data, así que no se tocan.
FACTORY_GLOBAL = (0x0B8100, 200)
# Se sube al cambiar la receta de PRELOAD: invalida el flash_factory.bin cacheado.
# 3 = Global Data de 200 B (0xC8) en vez de 234; los 12 B de residuo en
#     0x0A00D8..0x0A00E3 se dejan intactos.
FACTORY_FLASH_VERSION = "3"


def emu_path() -> str:
    p = os.environ.get("MS2000_EMU", DEFAULT_EMU)
    if not os.path.isfile(p):
        sys.exit(f"[ERROR] No encuentro el emulador: {p}\n       Definí MS2K_EMU.")
    return p


def ensure_factory_flash() -> str:
    """flash.bin restaurado a fábrica en la zona de usuario (GLOBAL 8: PRELOAD).

    Copia, desde el sector PRELOAD de solo lectura al sector USR regrabable:
      * 0x0B0000..0x0B7EFF (128 programas x 254 B) -> 0x0A0400..0x0A82FF
      * 0x0B8100 (Global Data de fábrica)          -> 0x0A0010
        Sólo 200 B (0xC8 = 100 palabras), que es lo que mueve de verdad la rutina
        de PRELOAD; el resto de la ventana de 234 B es residuo y no se toca.

    El sector de respaldo de fábrica NO lleva cabecera KORG (la primera cabecera
    del chip está en 0x0A0000), así que se valida por contenido y no por firma.
    El resultado se cachea con un sello de versión: subir FACTORY_FLASH_VERSION
    fuerza a regenerarlo cuando cambie la receta.
    """
    stamp = FACTORY_FLASH + ".v"
    if os.path.isfile(FACTORY_FLASH) and os.path.isfile(stamp):
        with open(stamp, "r", encoding="ascii") as fh:
            if fh.read().strip() == FACTORY_FLASH_VERSION:
                return FACTORY_FLASH
    src = os.environ.get("MS2000_FLASH", DEFAULT_FLASH)
    if not os.path.isfile(src):
        sys.exit(f"[ERROR] No encuentro flash.bin: {src}\n       Definí MS2000_FLASH.")
    os.makedirs(SCRATCH, exist_ok=True)
    shutil.copyfile(src, FACTORY_FLASH)
    with open(FACTORY_FLASH, "r+b") as f:
        f.seek(FACTORY_PRELOAD[0])
        programs = f.read(PROGRAM_BYTES * 128)
        if len(programs) != PROGRAM_BYTES * 128 or programs == b"\xff" * len(programs):
            sys.exit("[ERROR] El bloque de Factory Preload está vacío o truncado.")
        f.seek(FACTORY_GLOBAL[0])
        global_data = f.read(FACTORY_GLOBAL[1])
        if len(global_data) != FACTORY_GLOBAL[1]:
            sys.exit("[ERROR] La plantilla de Global Data de fábrica está truncada.")
        f.seek(USER_PROGRAMS[0])
        f.write(programs)
        f.seek(USER_GLOBAL[0])
        f.write(global_data)
    with open(stamp, "w", encoding="ascii") as fh:
        fh.write(FACTORY_FLASH_VERSION)
    return FACTORY_FLASH


def preset_name(idx: int) -> str:
    with open(ensure_factory_flash(), "rb") as f:
        f.seek(FACTORY_PRELOAD[0] + idx * PROGRAM_BYTES)
        raw = f.read(12)
    return raw.decode("ascii", "replace").strip()


def parse_patches(specs: list[str] | None) -> dict[int, int]:
    """Convierte `--patch OFF=VAL` en {offset: valor} sobre los 254 B del programa."""
    out: dict[int, int] = {}
    for spec in specs or []:
        if "=" not in spec:
            sys.exit(f"[ERROR] --patch espera OFF=VAL (0..253), recibido {spec!r}")
        k, v = spec.split("=", 1)
        off, val = int(k, 0), int(v, 0)
        if not 0 <= off < PROGRAM_BYTES:
            sys.exit(f"[ERROR] offset fuera de 0..253: {off}")
        if not 0 <= val <= 0xFF:
            sys.exit(f"[ERROR] valor fuera de 0..255: {val}")
        out[off] = val
    return out


def flash_for_program(idx: int, patches: dict[int, int] | None = None) -> str:
    """Copia del flash con el programa de fábrica <idx> escrito en la ranura 0.

    `patches` aplica cambios de byte antes de escribir la ranura: es la vía de los
    estímulos de P1 (una sola variable sobre INIT Program), sin SysEx ni MIDI.
    """
    if not 0 <= idx <= 127:
        sys.exit(f"[ERROR] índice fuera de rango: {idx}")
    base = ensure_factory_flash()
    suffix = "" if not patches else "_" + "_".join(f"{o:03d}-{v:02x}" for o, v in sorted(patches.items()))
    dst = os.path.join(SCRATCH, f"flash_prog_{idx:03d}{suffix}.bin")
    with open(base, "rb") as f:
        data = bytearray(f.read())
    off = FACTORY_PRELOAD[0] + idx * PROGRAM_BYTES
    program = bytearray(data[off:off + PROGRAM_BYTES])
    for o, v in (patches or {}).items():
        program[o] = v
    data[USER_PROGRAMS[0]:USER_PROGRAMS[0] + PROGRAM_BYTES] = program
    with open(dst, "wb") as f:
        f.write(data)
    return dst


# ----------------------------------------------------------------------------
# Guion MIDI (MS2K_MIDIIN = "t:hex;t:hex;...")
# ----------------------------------------------------------------------------
def build_midi_script(note: int, vel: int, note_on: float, note_off: float) -> str:
    """Sólo notas: el programa se elige parcheando el flash, no por MIDI."""
    events = [
        (note_on, bytes([0x90, note & 0x7F, vel & 0x7F])),
        (note_off, bytes([0x80, note & 0x7F, 0x00])),
    ]
    events.sort(key=lambda e: e[0])
    return ";".join(f"{t:.3f}:{chunk.hex().upper()}" for t, chunk in events)


# ----------------------------------------------------------------------------
# Render en el emulador
# ----------------------------------------------------------------------------
def render(program: int, note: int, vel: int, hold: float, out_wav: str,
           tail: float = 1.0, patches: dict[int, int] | None = None) -> str:
    os.makedirs(os.path.dirname(os.path.abspath(out_wav)) or ".", exist_ok=True)
    note_on = 3.0                     # tras el arranque (~2,5 s de MCU)
    note_off = note_on + hold
    duration = note_off + tail

    cmd = [
        emu_path(),
        "--rom", flash_for_program(program, patches),
        "--headless", "--quiet-boot", "--audio", "--power-on",
        "--master-vol", "100",
        "--sample-rate", str(HW_SR),
        "--duration", f"{duration:.2f}",
        "--render-test", os.path.abspath(out_wav),
    ]
    env = dict(os.environ)
    env["MS2K_MIDIIN"] = build_midi_script(note, vel, note_on, note_off)

    os.makedirs(SCRATCH, exist_ok=True)
    proc = subprocess.run(cmd, cwd=SCRATCH, env=env, capture_output=True, text=True,
                          timeout=600)
    log = (proc.stdout or "") + (proc.stderr or "")
    if proc.returncode != 0:
        print(log[-2000:])
        sys.exit(f"[ERROR] El emulador salió con código {proc.returncode}")
    if "TX0 0/0" in log:
        print("[WARN] TX0 0/0: la DSP no transmitió palabras; probable salida muda.")
    if not os.path.isfile(out_wav):
        print(log[-2000:])
        sys.exit(f"[ERROR] No se generó {out_wav}")
    return log


# ----------------------------------------------------------------------------
# Lectura del WAV del emulador (PCM entero 32-bit) y conversión
# ----------------------------------------------------------------------------
def read_wav_int32(path: str) -> tuple[list[float], list[float], int]:
    with wave.open(path, "rb") as w:
        ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        if sw != 4:
            sys.exit(f"[ERROR] {path}: sampwidth={sw}, se esperaba 4 (PCM 32-bit).")
        raw = w.readframes(n)
    inter = array.array("i")
    inter.frombytes(raw)
    if sys.byteorder == "big":
        inter.byteswap()
    scale = 1.0 / 2147483648.0
    left, right = [], []
    for i in range(0, len(inter) - ch + 1, ch):
        left.append(inter[i] * scale)
        right.append(inter[i + 1] * scale if ch > 1 else inter[i] * scale)
    return left, right, sr


def estimate_f0(seg: list[float], sr: int, lo: float = 60.0, hi: float = 1200.0) -> tuple[float, float]:
    """Fundamental por FFT + periodograma (numpy) o autocorrelación (respaldo).

    NO se usan cruces por cero: una sierra con armónicos altos más el rizado del
    filtro da ~6,75 cruces por ciclo (medido: 883 cruces en 0,5 s para una
    fundamental de 261,6 Hz => 1766 Hz aparentes).
    """
    n = len(seg)
    if n < 512:
        return 0.0, 0.0

    f_fft = 0.0
    try:
        import numpy as np  # opcional

        x = np.asarray(seg, dtype=float)
        x = x - x.mean()
        # Zero-padding x4: con 0,5 s de ventana los bins caen cada 2 Hz y la
        # fundamental de una sierra se parte entre dos bins.
        nfft = 1 << int(math.ceil(math.log2(n * 4)))
        spec = np.abs(np.fft.rfft(x * np.hanning(n), nfft))
        freqs = np.fft.rfftfreq(nfft, 1.0 / sr)
        # HPS de 3 armónicos: si el 2.º armónico supera a la fundamental, el
        # producto sigue premiando la fundamental. Sólo da una estimación
        # gruesa: el pico puede estar partido (medido 261,29 y 262,76 Hz con la
        # misma amplitud en una sierra de C4, y la parábola se iba 11 cents).
        hps = spec.copy()
        for h in (2, 3):
            dec = spec[::h]
            hps[: len(dec)] *= dec
        band = (freqs >= lo) & (freqs <= hi)
        coarse = float(freqs[int(np.argmax(np.where(band, hps, 0.0)))])
        # Refinado por periodograma (correlación compleja sobre la señal sin
        # ventanear): ~0,01 Hz = 0,07 cents.
        t = np.arange(n) / sr
        best_f, best_v = coarse, -1.0
        for cand in np.arange(coarse * 0.97, coarse * 1.03, 0.01):
            v = abs(complex(np.dot(x, np.exp(-2j * np.pi * cand * t))))
            if v > best_v:
                best_f, best_v = float(cand), v
        f_fft = best_f
    except ImportError:
        pass

    # Autocorrelación: sólo se usa si no hay numpy. Se toma el PRIMER lag cuyo
    # pico supere el 90 % del máximo; el máximo global da la octava baja
    # (medido: 131,5 Hz = 261,6/2 sobre una sierra).
    f_acf = 0.0
    if f_fft <= 0.0:
        m = min(n, 8192)
        lag_min, lag_max = int(sr / hi), min(int(sr / lo), m - 2)
        step = 2
        acf: dict[int, float] = {}
        for lag in range(lag_min, lag_max + 1):
            num = sum(seg[i] * seg[i + lag] for i in range(0, m - lag, step))
            e1 = math.sqrt(sum(seg[i] * seg[i] for i in range(0, m - lag, step)))
            e2 = math.sqrt(sum(seg[i + lag] * seg[i + lag] for i in range(0, m - lag, step)))
            acf[lag] = num / (e1 * e2 + 1e-12)
        if acf:
            top = max(acf.values())
            best = next((lag for lag in sorted(acf)
                         if acf[lag] >= 0.9 * top
                         and acf.get(lag - 1, -2.0) <= acf[lag] >= acf.get(lag + 1, -2.0)), 0)
            if best:
                f_acf = sr / best
    return f_fft, f_acf


def spectral_centroid_hz(seg: list[float], sr: int) -> float:
    """Centroide espectral (ponderado por potencia) de la ventana medida."""
    try:
        import numpy as np

        x = np.asarray(seg, dtype=float)
        x = x - x.mean()
        power = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
        freqs = np.fft.rfftfreq(len(x), 1.0 / sr)
        total = power.sum()
        return float((power * freqs).sum() / total) if total > 0 else 0.0
    except ImportError:
        return 0.0


def measure(left: list[float], right: list[float], sr: int) -> dict:
    """Nivel, onset y fundamental (FFT + periodograma) desde el onset."""
    mono = [0.5 * (l + r) for l, r in zip(left, right)]
    peak = max((abs(x) for x in mono), default=0.0)
    onset = next((k for k, x in enumerate(mono) if abs(x) > 1e-3), -1)
    if onset < 0:
        return {"peak_db": -120.0, "rms_db": -120.0, "onset": -1, "f0": 0.0,
                "f0_acf": 0.0, "centroid": 0.0}
    seg = mono[onset:onset + min(len(mono) - onset, 24000)]  # 0,5 s
    rms = math.sqrt(sum(x * x for x in seg) / max(1, len(seg)))
    f_fft, f_acf = estimate_f0(seg, sr)
    return {
        "peak_db": 20 * math.log10(peak) if peak > 0 else -120.0,
        "rms_db": 20 * math.log10(rms) if rms > 0 else -120.0,
        "onset": onset,
        "f0": f_fft,
        "f0_acf": f_acf,
        "centroid": spectral_centroid_hz(seg, sr),
    }


def write_bin(path: str, left: list[float], right: list[float]) -> None:
    """Formato que espera CalibrationTests: uint32 nFrames + float L + float R."""
    with open(path, "wb") as f:
        f.write(struct.pack("<I", len(left)))
        f.write(struct.pack(f"<{len(left)}f", *left))
        f.write(struct.pack(f"<{len(right)}f", *right))


# ----------------------------------------------------------------------------
# Comandos
# ----------------------------------------------------------------------------
def cmd_gate(args) -> int:
    out = os.path.join(SCRATCH, "gate_init.wav")
    print(f"[P0.2] INIT Program (H16, #{127}) \"{preset_name(127)}\" + C4 ...")
    render(127, 60, 100, hold=1.5, out_wav=out)
    left, right, sr = read_wav_int32(out)
    m = measure(left, right, sr)
    cents_fft = 1200.0 * math.log2(m["f0"] / C4_HZ) if m["f0"] > 0 else float("inf")
    cents_acf = 1200.0 * math.log2(m["f0_acf"] / C4_HZ) if m["f0_acf"] > 0 else float("inf")
    cents = min(abs(cents_fft), abs(cents_acf))
    print(f"  sr={sr} frames={len(left)} ({len(left)/sr:.2f}s) "
          f"peak={m['peak_db']:.1f} dBFS rms={m['rms_db']:.1f} dBFS "
          f"centroide={m['centroid']:.0f} Hz")
    print(f"  f0 FFT={m['f0']:.2f} Hz ({cents_fft:+.2f} cents) | "
          f"f0 ACF={m['f0_acf']:.1f} Hz ({cents_acf:+.1f} cents) | C4={C4_HZ:.1f} Hz")
    ok = cents <= 5.0 and m["peak_db"] > -60.0
    print(f"  {'[PASS]' if ok else '[FAIL]'} puerta INIT Program + C4 -> 261,6 Hz ±5 cents")
    return 0 if ok else 1


def cmd_render(args) -> int:
    out = args.out or os.path.join(SCRATCH, "oracle.wav")
    patches = parse_patches(args.patch)
    extra = "" if not patches else " patch " + " ".join(f"[{o}]={v:02X}" for o, v in sorted(patches.items()))
    print(f"[P0.2] #{args.program:03d} \"{preset_name(args.program)}\"{extra}")
    render(args.program, args.note, args.vel, args.hold, out, patches=patches)
    left, right, sr = read_wav_int32(out)
    m = measure(left, right, sr)
    print(f"  -> {out}")
    print(f"  frames={len(left)} ({len(left)/sr:.2f}s) peak={m['peak_db']:.1f} dBFS "
          f"rms={m['rms_db']:.1f} dBFS f0={m['f0']:.1f} Hz "
          f"centroide={m['centroid']:.0f} Hz")
    return 0


def cmd_bins(args) -> int:
    indices = [int(x) for x in args.indices.split(",") if x.strip()]
    test_dir = os.path.join(REPO, "tests")
    failures = 0
    for idx in indices:
        wav = os.path.join(SCRATCH, f"emu_prog_{idx}.wav")
        print(f"[P0.2] #{idx:03d} ({chr(65 + idx // 16)}{idx % 16 + 1:02d}) "
              f"\"{preset_name(idx)}\" ...", flush=True)
        render(idx, 60, 100, args.hold, wav)
        left, right, sr = read_wav_int32(wav)
        m = measure(left, right, sr)
        if m["peak_db"] <= -60.0:
            print(f"  [FAIL] silencio (peak {m['peak_db']:.1f} dBFS)")
            failures += 1
            continue
        start = max(0, m["onset"])
        n = min(24000, len(left) - start)
        dst = os.path.join(test_dir, f"emu_preset_{idx}.bin")
        write_bin(dst, left[start:start + n], right[start:start + n])
        print(f"  OK  peak={m['peak_db']:.1f} dBFS f0={m['f0']:.1f} Hz -> {dst} ({n} frames)")
    print(f"\n[P0.2] {len(indices) - failures}/{len(indices)} presets con audio real.")
    return 1 if failures else 0


def cmd_make_factory_flash(args) -> int:
    for path in (FACTORY_FLASH, FACTORY_FLASH + ".v"):
        if os.path.isfile(path):
            os.remove(path)
    print(f"[P0.2] {ensure_factory_flash()}")
    print(f"  programas: {PROGRAM_BYTES * 128} B (128 x 254) "
          f"0x{FACTORY_PRELOAD[0]:06X} -> 0x{USER_PROGRAMS[0]:06X}")
    print(f"  global:    {FACTORY_GLOBAL[1]} B "
          f"0x{FACTORY_GLOBAL[0]:06X} -> 0x{USER_GLOBAL[0]:06X}")
    print("  equivale a GLOBAL 8: PRELOAD del hardware; los 128 programas y los")
    print("  ajustes globales del emulador quedan == MS2000FactoryPreloadData.h")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Oráculo ms2000r-emu (P0.2)")
    ap.add_argument("--gate", action="store_true", help="puerta INIT Program + C4")
    ap.add_argument("--program", type=int, help="preset de fábrica 0..127")
    ap.add_argument("--note", type=int, default=60, help="nota MIDI (60 = C4)")
    ap.add_argument("--vel", type=int, default=100)
    ap.add_argument("--hold", type=float, default=2.0, help="segundos de nota")
    ap.add_argument("--out", help="WAV de salida")
    ap.add_argument("--patch", action="append", metavar="OFF=VAL",
                    help="modifica un byte del programa (p. ej. TI_CUTOFF=58: --patch 58=0x20)")
    ap.add_argument("--bins", action="store_true", help="genera tests/emu_preset_*.bin")
    ap.add_argument("--indices", default=",".join(str(i) for i in DEFAULT_INDICES))
    ap.add_argument("--make-factory-flash", action="store_true")
    args = ap.parse_args()

    if args.make_factory_flash:
        return cmd_make_factory_flash(args)
    if args.gate:
        return cmd_gate(args)
    if args.bins:
        return cmd_bins(args)
    if args.program is not None:
        return cmd_render(args)
    ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
