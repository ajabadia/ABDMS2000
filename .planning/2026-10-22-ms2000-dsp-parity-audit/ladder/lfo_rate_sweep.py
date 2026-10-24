#!/usr/bin/env python
"""Long paired MS2000 emulator/ABD LFO1-rate sweep using existing diagnostic tools.

Run from repository root:
  python .planning/2026-10-22-ms2000-dsp-parity-audit/ladder/lfo_rate_sweep.py
  python .../lfo_rate_sweep.py --knobs 48 64 80 96 112 127

Each case clones the R5K (+32 pitch depth, triangle, voice key-sync) diagnostic
program, changes only LFO1 Speed, and uses an isolated emulator home and comparator.
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import shutil
import struct
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
SWEEP = HERE / "lfo_rate_sweep"
EMU_EXE = Path(r"D:\desarrollos\ABDSynths\_RESOURCES\ms2000\ms2000r-emu-main\ms2000r-emu-main\build_vst\Release\ms2000_emulator.exe")
BASE_RUNG = HERE / "rung_r5k.bin"
BASE_EXE = REPO / "build" / "Release" / "CalibrationTests_probe_r5k.exe"
BASE_HOME = HERE / "emu_home_r5k"
USER_SLOT_OFFSET = 0x0A0400 + 2 * 254
LFO1_SPEED_BYTE_OFFSET = 0x26 + 39
PATCH1_INT_BYTE_OFFSET = 0x26 + 45
LFO1_FLAGS_BYTE_OFFSET = 0x26 + 38
FRAMES = 840_000
EMULATOR_DURATION = 24.0
NOTE_ON_TIME = 4.5
NOTE_OFF_TIME = 22.5
MIN_ABD_CYCLES = 2.0
DEFAULT_KNOBS = (32, 48, 64, 80, 96, 112, 127)


def pack_korg(src: bytes) -> bytes:
    out = bytearray()
    for group_start in range(0, len(src), 7):
        group = src[group_start:group_start + 7]
        high_bits = 0
        for index, value in enumerate(group):
            high_bits |= (value >> 7) << (6 - index)
        out.append(high_bits)
        out.extend(value & 0x7F for value in group)
    return bytes(out)


def make_rung(base: bytes, knob: int) -> bytes:
    if len(base) != 254:
        raise ValueError(f"expected 254-byte diagnostic program, got {len(base)}")
    rung = bytearray(base)
    rung[:12] = f"LRATE {knob:03d}".ljust(12).encode("ascii")
    rung[LFO1_SPEED_BYTE_OFFSET] = knob
    if rung[PATCH1_INT_BYTE_OFFSET] != 96 or rung[LFO1_FLAGS_BYTE_OFFSET] != 0x22:
        raise ValueError("base rung is not R5K (+32, triangle, voice key-sync)")
    return bytes(rung)


def patch_comparator_exe(rung: bytes, knob: int) -> Path:
    source = BASE_EXE.read_bytes()
    signature = b"DIAG R5K SYN"
    positions = []
    cursor = 0
    while True:
        cursor = source.find(signature, cursor)
        if cursor < 0:
            break
        positions.append(cursor)
        cursor += len(signature)
    if len(positions) != 256:
        raise RuntimeError(f"expected 256 diagnostic preset records, found {len(positions)}")

    patched = bytearray(source)
    for position in positions:
        patched[position:position + 254] = rung
    if any(patched[position:position + 254] != rung for position in positions):
        raise RuntimeError("comparator preset patch validation failed")
    output = REPO / "build" / "Release" / f"CalibrationTests_lfoRate_{knob:03d}.exe"
    output.write_bytes(patched)
    return output


def make_emulator_home(rung: bytes, knob: int) -> Path:
    home = SWEEP / f"emu_home_{knob:03d}"
    if home.exists():
        shutil.rmtree(home)
    shutil.copytree(BASE_HOME, home)
    flash_path = home / "flash.bin"
    flash = bytearray(flash_path.read_bytes())
    flash[USER_SLOT_OFFSET:USER_SLOT_OFFSET + 254] = rung
    flash_path.write_bytes(flash)
    cached_state = home / "ms2000_flash_state.bin"
    if cached_state.exists():
        cached_state.unlink()
    return home


def midi_schedule(rung: bytes) -> str:
    sysex = bytes((0xF0, 0x42, 0x30, 0x58, 0x40)) + pack_korg(rung) + bytes((0xF7,))
    message = " ".join(f"{value:02X}" for value in sysex)
    return f"3.6:{message};{NOTE_ON_TIME}:90 3C 64;{NOTE_OFF_TIME}:80 3C 00"


def read_wav(path: Path) -> tuple[np.ndarray, np.ndarray]:
    with wave.open(str(path), "rb") as stream:
        if stream.getframerate() != 48_000 or stream.getnchannels() != 2:
            raise RuntimeError(f"unexpected WAV format: {path}")
        channels, width, frames = stream.getnchannels(), stream.getsampwidth(), stream.getnframes()
        data = stream.readframes(frames)
    if width == 4:
        audio = np.frombuffer(data, dtype="<i4").astype(np.float64) / (2.0**31)
    elif width == 2:
        audio = np.frombuffer(data, dtype="<i2").astype(np.float64) / (2.0**15)
    else:
        raise RuntimeError(f"unexpected WAV sample width {width}: {path}")
    audio = audio.reshape(-1, channels)
    return audio[:, 0], audio[:, 1]


def write_comparator_capture(path: Path, left: np.ndarray, right: np.ndarray) -> int:
    peak = np.maximum(np.abs(left), np.abs(right))
    audible = np.flatnonzero(peak > 1e-3)
    if audible.size == 0:
        raise RuntimeError(f"silent emulator capture: {path}")
    onset = int(audible[0])
    available = left.size - onset
    sample_count = min(FRAMES, available, int((NOTE_OFF_TIME - NOTE_ON_TIME) * 48_000))
    if sample_count < int(12.0 * 48_000):
        raise RuntimeError(f"note-on segment too short: have {sample_count}, need at least 12 seconds")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        stream.write(struct.pack("<I", sample_count))
        stream.write(left[onset:onset + sample_count].astype("<f4").tobytes())
        stream.write(right[onset:onset + sample_count].astype("<f4").tobytes())
    return onset


def render_case(knob: int) -> None:
    rung = make_rung(BASE_RUNG.read_bytes(), knob)
    home = make_emulator_home(rung, knob)
    (SWEEP / f"rung_{knob:03d}.bin").write_bytes(rung)
    exe = patch_comparator_exe(rung, knob)

    wav_path = SWEEP / f"emu_{knob:03d}.wav"
    log_path = SWEEP / f"emu_{knob:03d}.log"
    env = dict(os.environ)
    env["MS2K_HOME"] = str(home)
    env["MS2K_MIDIIN"] = midi_schedule(rung)
    command = [str(EMU_EXE), "--headless", "--render-test", str(wav_path), "--duration", str(EMULATOR_DURATION)]
    # The emulator's instruction tracing can emit gigabytes of stdout per run.
    # Discard it while rendering; the WAV and exit status are the relevant evidence.
    with log_path.open("wb") as log:
        result = subprocess.run(command, cwd=home, env=env, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL, timeout=900, check=False)
    if result.returncode:
        raise RuntimeError(f"emulator failed at knob {knob} (exit {result.returncode}); see {log_path}")

    left, right = read_wav(wav_path)
    capture_dir = SWEEP / f"cmp_{knob:03d}"
    emu_capture = capture_dir / "emu_preset_2.bin"
    onset = write_comparator_capture(emu_capture, left, right)
    peak_db = 20.0 * np.log10(max(float(np.max(np.maximum(np.abs(left), np.abs(right)))), 1e-12))
    print(f"[knob {knob:03d}] emulator frames={left.size}, onset={onset}, peak={peak_db:.1f} dBFS")

    comparator_log = SWEEP / f"compare_{knob:03d}.log"
    compared = subprocess.run([str(exe), "--compare-emu", str(capture_dir)],
                              cwd=REPO / "build" / "Release", capture_output=True,
                              text=True, timeout=900, check=False)
    comparator_log.write_text(compared.stdout + compared.stderr, encoding="utf-8", errors="replace")
    if compared.returncode or "Preset A03" not in compared.stdout:
        raise RuntimeError(f"ABD comparator failed at knob {knob} (exit {compared.returncode}); see {comparator_log}")
    if not (capture_dir / "abd_preset_2.bin").is_file():
        raise RuntimeError(f"ABD comparator did not emit output for knob {knob}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--knobs", nargs="+", type=int, default=list(DEFAULT_KNOBS),
                        help="LFO1 speed values (default: 32 48 64 80 96 112 127)")
    parser.add_argument("--skip-render", action="store_true", help="analyze already generated paired captures")
    parser.add_argument("--skip-knobs-with-captures", action="store_true",
                        help="keep existing paired captures and only render missing knob cases")
    args = parser.parse_args()
    if not args.knobs or any(knob < 0 or knob > 127 for knob in args.knobs):
        parser.error("all knob values must be within 0..127")
    if len(set(args.knobs)) != len(args.knobs):
        parser.error("knob values must be unique")
    if not args.skip_render:
        if not (EMU_EXE.is_file() and BASE_RUNG.is_file() and BASE_EXE.is_file() and BASE_HOME.is_dir()):
            raise SystemExit("existing emulator, R5K rung/executable and isolated emulator home are required")
        SWEEP.mkdir(parents=True, exist_ok=True)
        for knob in args.knobs:
            capture_dir = SWEEP / f"cmp_{knob:03d}"
            paired = all((capture_dir / f"{side}_preset_2.bin").is_file()
                         for side in ("emu", "abd"))
            if args.skip_knobs_with_captures and paired:
                print(f"[knob {knob:03d}] retaining existing paired captures")
                continue
            if args.skip_knobs_with_captures and not paired:
                stale_log = SWEEP / f"emu_{knob:03d}.log"
                if stale_log.exists():
                    stale_log.unlink()
            render_case(knob)

    analyzer = HERE / "lfo_rate_analyze.py"
    subprocess.run([sys.executable, str(analyzer), "--self-test"], check=True)
    table_text = (REPO / "Source" / "Core" / "MS2000FirmwareTables.h").read_text(encoding="utf-8")
    match = re.search(r"kRomLfo1IncrementQ23\[128\]\s*=\s*\{(.*?)\};", table_text, re.S)
    if not match:
        raise RuntimeError("could not parse firmware LFO1 ROM table")
    increments = [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]+)u", match.group(1))]
    if len(increments) != 128:
        raise RuntimeError(f"expected 128 LFO1 ROM entries, got {len(increments)}")

    records: list[dict[str, str]] = []
    for knob in args.knobs:
        capture_dir = SWEEP / f"cmp_{knob:03d}"
        for side in ("emu", "abd"):
            capture = capture_dir / f"{side}_preset_2.bin"
            result = subprocess.run([sys.executable, str(analyzer), str(capture)], cwd=REPO,
                                    capture_output=True, text=True, timeout=120, check=False)
            if result.returncode:
                raise RuntimeError(f"rate analysis failed for knob {knob} ({side}): {result.stderr}")
            line = result.stdout.strip().splitlines()[-1]
            rate = float(re.search(r"rate=([0-9.]+) Hz", line).group(1))
            prominence = float(re.search(r"prominence=([0-9.]+) dB", line).group(1))
            span = float(re.search(r"pitch-span\(p5\.\.p95\)=([0-9.]+) cents", line).group(1))
            cycles = float(re.search(r"cycles=([0-9.]+)", line).group(1))
            if prominence < 5.0 or cycles < 2.0:
                raise RuntimeError(f"weak rate evidence at knob {knob} ({side}): {line}")
            if side == "abd" and cycles < MIN_ABD_CYCLES:
                raise RuntimeError(
                    f"ABD pitch-modulation capture at knob {knob} contains only {cycles:.2f} cycles; "
                    "render a longer note or treat this LFO rate as under-resolved"
                )
            records.append({"knob": str(knob), "side": side, "rate_hz": f"{rate:.6f}",
                            "rom_expected_hz": f"{increments[knob] * 48000.0 / 16777216.0:.6f}",
                            "prominence_db": f"{prominence:.2f}", "pitch_span_p5_p95_cents": f"{span:.2f}",
                            "cycles_observed": f"{cycles:.2f}"})
            print(f"[knob {knob:03d}] {side}: {line}")

    if not records:
        raise RuntimeError("no LFO rate samples found")
    csv_path = SWEEP / "rate_results.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0].keys()))
        writer.writeheader()
        writer.writerows(records)

    by_knob = {int(record["knob"]): {} for record in records}
    for record in records:
        by_knob[int(record["knob"])][record["side"]] = float(record["rate_hz"])
    print("\nknob | ROM Hz | emu Hz | ABD Hz | emu/ROM | emu/ABD")
    for knob in args.knobs:
        row = by_knob[knob]
        rom_hz = increments[knob] * 48000.0 / 16777216.0
        print(f"{knob:4d} | {rom_hz:6.3f} | {row['emu']:6.3f} | {row['abd']:6.3f} | "
              f"{row['emu'] / rom_hz:7.3f} | {row['emu'] / row['abd']:7.3f}")
    print(f"\nCSV: {csv_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
