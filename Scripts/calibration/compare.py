#!/usr/bin/env python3
"""P0.4 — Comparador pareado oráculo <-> motor sobre un barrido de un byte.

Cada fila es un estímulo: el mismo programa de fábrica con los bytes fijos y el
valor del barrido aplicados en los DOS renderizadores. Los dos lados se miden
con las MISMAS funciones, sobre el mismo tramo desde el onset, así que cada
diferencia es atribuible al motor.

Uso:
  # ley de resonancia con ruido del Mixer (plano, sin el SVF interno de OSC1)
  python Scripts/calibration/compare.py --metric psd --note 60 \
      --fixed 54=0 --fixed 56=127 --fixed 58=64 \
      --sweep 59=0,10,16,32,48,64,80,96 \
      --out-csv scratch/p01/q_c64.csv --out-md scratch/p01/q_c64.md

MÉTRICAS
  harmonic (por defecto): envolvente armónica de una fuente con altura (sierra).
      Realce = máximo del cociente armónico contra la fila de referencia
      (`--ref`). Sirve para cualquier preset, pero su resolución es f0 (55 Hz a
      A1), demasiado gruesa para fijar la ley de Q.
  psd: densidad espectral de potencia (Welch, ventana 4096, salto 2048) de una
      fuente plana — el canal Noise del Mixer, `54=0 56=127` — que va directo al
      VCF sin el SVF interno de OSC1. De ahí salen fc (pico) y Q = fc / ancho a
      −3 dB. Resolución ~12 Hz en el PSD, ~3 Hz tras el suavizado, suficiente
      para Q de 2 a 20 en fc >= 1 kHz. Con reso 0 no hay pico resonante y Q sale
      indefinido, que es lo correcto.

Se descartan los armónicos cuya referencia esté a más de 40 dB del máximo, para
no dividir por suelo de ruido.
"""

from __future__ import annotations

import argparse
import math
import os
import struct
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run_oracle as ro  # noqa: E402

EXE_DEFAULT = os.path.join(ro.REPO, "build-test", "Release", "ABDMS2000_Render.exe")
OUTDIR = os.path.join(ro.SCRATCH, "cmp")
NAN = float("nan")


# ----------------------------------------------------------------------------
# Lectura de la salida del motor (uint32 nFrames + float L + float R)
# ----------------------------------------------------------------------------
def read_engine_bin(path: str) -> tuple[list[float], list[float]]:
    with open(path, "rb") as f:
        raw = f.read()
    n = struct.unpack("<I", raw[:4])[0]
    inter = np.frombuffer(raw[4:4 + n * 8], dtype="<f4")
    return list(inter[:n]), list(inter[n:2 * n])


def parse_assignments(specs: list[str] | None) -> dict[int, int]:
    out: dict[int, int] = {}
    for spec in specs or []:
        if "=" not in spec:
            sys.exit(f"[ERROR] esperaba OFF=VAL, recibido {spec!r}")
        k, v = spec.split("=", 1)
        out[int(k, 0)] = int(v, 0)
    return out


def parse_sweep(spec: str) -> tuple[int, list[int]]:
    if "=" not in spec:
        sys.exit(f"[ERROR] --sweep espera OFF=v1,v2,... recibido {spec!r}")
    k, vals = spec.split("=", 1)
    return int(k, 0), [int(v, 0) for v in vals.split(",") if v.strip()]


# ----------------------------------------------------------------------------
# Render de los dos lados
# ----------------------------------------------------------------------------
def render_engine(exe: str, program: int, note: int, vel: int, hold: float,
                  patches: dict[int, int], tag: str) -> str:
    os.makedirs(OUTDIR, exist_ok=True)
    base = os.path.join(OUTDIR, f"eng_{tag}")
    cmd = [exe, "--program", str(program), "--note", str(note), "--vel", str(vel),
           "--hold", f"{hold:g}", "--out", base]
    for off, val in sorted(patches.items()):
        cmd += ["--patch", f"{off}={val}"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"[ERROR] motor: {proc.stdout}{proc.stderr}")
    return base + ".bin"


def render_oracle(program: int, note: int, vel: int, hold: float,
                  patches: dict[int, int], tag: str) -> str:
    os.makedirs(OUTDIR, exist_ok=True)
    wav = os.path.join(OUTDIR, f"emu_{tag}.wav")
    ro.render(program, note, vel, hold, wav, patches=patches)
    return wav


# ----------------------------------------------------------------------------
# Métricas
# ----------------------------------------------------------------------------
def onset_index(mono: np.ndarray, thresh: float = 1e-3) -> int:
    idx = np.flatnonzero(np.abs(mono) > thresh)
    return int(idx[0]) if idx.size else -1


def sustain(mono: np.ndarray, sr: int, skip: float = 0.2,
            dur: float = 1.2) -> np.ndarray:
    on = onset_index(mono)
    if on < 0:
        return np.zeros(0)
    seg = mono[on + int(skip * sr): on + int(skip * sr) + int(dur * sr)]
    return seg - seg.mean() if seg.size else seg


# --- modo harmonic ----------------------------------------------------------
def harmonic_envelope(mono: np.ndarray, sr: int, f0: float,
                      skip: float = 0.2, dur: float = 1.0) -> tuple[np.ndarray, np.ndarray]:
    seg = sustain(mono, sr, skip, dur)
    if seg.size < 4096 or f0 <= 0:
        return np.zeros(0), np.zeros(0)
    nfft = 1 << 16
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), nfft))
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)
    ns = np.arange(1, 181)
    amp = np.array([spec[(freqs > n * f0 - 3) & (freqs < n * f0 + 3)].max()
                    if np.any((freqs > n * f0 - 3) & (freqs < n * f0 + 3)) else 1e-12
                    for n in ns])
    return ns * f0, 20.0 * np.log10(amp + 1e-30)


def ref_peak_harmonic(env, ref) -> tuple[float, float]:
    f, db = env
    rf, rdb = ref
    if db.size == 0 or rdb.size == 0 or db.size != rdb.size:
        return NAN, NAN
    valid = rdb > rdb.max() - 40.0
    if not np.any(valid):
        return NAN, NAN
    ratio = db[valid] - rdb[valid]
    i = int(np.argmax(ratio))
    return float(ratio[i]), float(f[valid][i])


# --- modo psd ---------------------------------------------------------------
def welch(mono: np.ndarray, sr: int, n: int = 4096, hop: int = 2048) -> tuple[np.ndarray, np.ndarray]:
    seg = sustain(mono, sr, 0.2, 1.6)
    if seg.size < n * 2:
        return np.zeros(0), np.zeros(0)
    w = np.hanning(n)
    acc = np.zeros(n // 2 + 1)
    count = 0
    for i in range(0, seg.size - n, hop):
        acc += np.abs(np.fft.rfft(seg[i:i + n] * w)) ** 2
        count += 1
    return np.fft.rfftfreq(n, 1.0 / sr), acc / max(count, 1)


def psd_metrics(f: np.ndarray, P: np.ndarray) -> tuple[float, float, float]:
    """(fc del pico, Q por ancho a -3 dB, nivel pico en dB). Q=nan si no hay pico."""
    if f.size == 0:
        return NAN, NAN, NAN
    db = 10.0 * np.log10(P + 1e-30)
    sm = np.convolve(db, np.ones(3) / 3.0, mode="same")
    band = (f > 80) & (f < 12000)
    i = int(np.argmax(np.where(band, sm, -1e9)))
    fpk, pk = float(f[i]), float(sm[i])
    low = sm[(f > 80) & (f < 200)]
    if low.size == 0 or pk < low.max() + 2.0:
        return fpk, NAN, pk          # sin pico resonante: filtro sobreamortiguado
    lo = i
    while lo > 0 and sm[lo] > pk - 3.0:
        lo -= 1
    hi = i
    while hi < sm.size - 1 and sm[hi] > pk - 3.0:
        hi += 1
    bw = float(f[hi] - f[lo])
    return fpk, (fpk / bw if bw > 0 else NAN), pk


def fc_from_psd(f: np.ndarray, P: np.ndarray) -> float:
    """fc a -3 dB sobre la meseta de graves. Fiable con reso 0, donde no hay pico."""
    if f.size == 0:
        return NAN
    db = 10.0 * np.log10(P + 1e-30)
    sm = np.convolve(db, np.ones(3) / 3.0, mode="same")
    low = sm[(f >= 60) & (f <= 150)]
    if low.size == 0:
        return NAN
    idx = np.where((f > 150) & (sm < float(np.median(low)) - 3.0))[0]
    return float(f[idx[0]]) if idx.size else NAN


def fit_svf(f: np.ndarray, P: np.ndarray, fc0: float) -> tuple[float, float, float]:
    """Ajusta |H| de un SVF LPF de 2 polos al PSD medido. Devuelve (fc, Q, residuo dB).

    Modelo: |H(f)| = 1 / sqrt((1-u^2)^2 + (2R u)^2), u = f/fc, R = 1/(2Q).
    Se busca en rejilla (fc, Q) minimizando la desviación típica tras quitar el
    nivel, así que ni el nivel absoluto ni la compensación de ganancia importan.
    """
    if f.size == 0 or fc0 != fc0 or fc0 <= 0:
        return NAN, NAN, NAN
    db = 10.0 * np.log10(P + 1e-30)
    sm = np.convolve(db, np.ones(3) / 3.0, mode="same")
    band = (f >= max(40.0, fc0 / 12.0)) & (f <= min(14000.0, fc0 * 10.0))
    if band.sum() < 20:
        return NAN, NAN, NAN
    ff, mm = f[band], sm[band]
    best_err, best_fc, best_q = 1e9, NAN, NAN
    for fc in np.geomspace(fc0 / 3.0, fc0 * 3.0, 40):
        u = ff / fc
        for q in np.geomspace(0.45, 30.0, 40):
            r = 0.5 / q
            mag = 1.0 / np.sqrt((1.0 - u * u) ** 2 + (2.0 * r * u) ** 2)
            model = 20.0 * np.log10(mag + 1e-12)
            resid = mm - model
            err = float(np.std(resid - resid.mean()))
            if err < best_err:
                best_err, best_fc, best_q = err, float(fc), float(q)
    return best_fc, best_q, best_err


def measure_side(left: list[float], right: list[float], sr: int, metric: str) -> dict:
    m = ro.measure(left, right, sr)
    mono = 0.5 * (np.asarray(left, dtype=float) + np.asarray(right, dtype=float))
    out = {**m, "fc": NAN, "q": NAN, "ref_db": NAN, "ref_hz": NAN}
    if metric in ("psd", "fit"):
        f, P = welch(mono, sr)
        out["psd"] = (f, P)
        out["fc"], out["q"], _ = psd_metrics(f, P)
    else:
        out["env"] = harmonic_envelope(mono, sr, m["f0"] or 0.0)
    return out


def ref_value(row: dict, ref: dict, side: str, metric: str) -> tuple[float, float]:
    if metric in ("psd", "fit"):
        f, P = row[f"{side}_psd"]
        rf, RP = ref[f"{side}_psd"]
        if f.size == 0 or rf.size == 0 or f.size != rf.size:
            return NAN, NAN
        fpk = row[f"{side}_fc"]
        if fpk != fpk:
            return NAN, NAN
        i = int(np.argmin(np.abs(f - fpk)))
        return float(10 * np.log10(P[i] / RP[i] + 1e-30)), fpk
    return ref_peak_harmonic(row[f"{side}_env"], ref[f"{side}_env"])


# ----------------------------------------------------------------------------
# Salida
# ----------------------------------------------------------------------------
COLS = ["value", "emu_peak_db", "emu_rms_db", "emu_centroid", "emu_f0", "emu_fc", "emu_q",
        "abd_peak_db", "abd_rms_db", "abd_centroid", "abd_f0", "abd_fc", "abd_q",
        "d_rms_db", "emu_ref_db", "abd_ref_db", "ref_hz"]


def fmt(v) -> str:
    if isinstance(v, float):
        return "nan" if v != v else f"{v:.3f}"
    return str(v)


def write_csv(path: str, rows: list[dict]) -> None:
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(",".join(COLS) + "\n")
        for r in rows:
            f.write(",".join(fmt(r.get(c, NAN)) for c in COLS) + "\n")


def write_md(path: str, rows: list[dict], title: str) -> None:
    lines = [f"### {title}", "",
             "| valor | emu fc | emu Q | abd fc | abd Q | emu realce | abd realce | Δ RMS |",
             "|---|---|---|---|---|---|---|---|"]
    for r in rows:
        lines.append(
            f"| {r['value']} | {fmt_q(r['emu_fc'], 'Hz')} | {fmt_q(r['emu_q'], '')} | "
            f"{fmt_q(r['abd_fc'], 'Hz')} | {fmt_q(r['abd_q'], '')} | "
            f"{fmt_q(r['emu_ref_db'], 'dB')} | {fmt_q(r['abd_ref_db'], 'dB')} | "
            f"{r['d_rms_db']:+.1f} |")
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def fmt_q(v: float, unit: str) -> str:
    if v != v:
        return "—"
    return f"{v:.0f} {unit}".strip() if unit else f"{v:.2f}"


# ----------------------------------------------------------------------------
def main() -> int:
    try:  # la consola de Windows (cp1252) revienta con Δ al redirigir la salida
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass
    ap = argparse.ArgumentParser(description="Comparador pareado P0.4")
    ap.add_argument("--program", type=int, default=127)
    ap.add_argument("--note", type=int, default=60)
    ap.add_argument("--vel", type=int, default=100)
    ap.add_argument("--hold", type=float, default=1.5)
    ap.add_argument("--metric", choices=("harmonic", "psd", "fit"), default="harmonic")
    ap.add_argument("--sweep", required=True, metavar="OFF=v1,v2,...")
    ap.add_argument("--fixed", action="append", metavar="OFF=VAL")
    ap.add_argument("--ref", type=int, default=0, help="fila de referencia del barrido")
    ap.add_argument("--sweep-name", default="")
    ap.add_argument("--out-csv", default=os.path.join(OUTDIR, "sweep.csv"))
    ap.add_argument("--out-md", default=os.path.join(OUTDIR, "sweep.md"))
    ap.add_argument("--engine", default=EXE_DEFAULT)
    args = ap.parse_args()

    if not os.path.isfile(args.engine):
        sys.exit(f"[ERROR] no encuentro el renderizador del motor: {args.engine}")
    sweep_off, values = parse_sweep(args.sweep)
    fixed = parse_assignments(args.fixed)
    name = args.sweep_name or f"byte {sweep_off}"
    print(f"[P0.4] {name} ({args.metric}) sobre #{args.program:03d} nota {args.note} "
          f"hold {args.hold:g}s ({len(values)} estímulos)")

    rows: list[dict] = []
    for val in values:
        patches = dict(fixed)
        patches[sweep_off] = val
        tag = "_".join(f"{o:03d}-{v:02x}" for o, v in sorted(patches.items()))
        eng = read_engine_bin(render_engine(args.engine, args.program, args.note,
                                           args.vel, args.hold, patches, tag))
        wav = render_oracle(args.program, args.note, args.vel, args.hold, patches, tag)
        emu_l, emu_r, sr = ro.read_wav_int32(wav)
        emu = measure_side(emu_l, emu_r, sr, args.metric)
        abd = measure_side(eng[0], eng[1], sr, args.metric)
        row = {"value": val, "d_rms_db": abd["rms_db"] - emu["rms_db"]}
        for side, m in (("emu", emu), ("abd", abd)):
            row[f"{side}_peak_db"] = m["peak_db"]
            row[f"{side}_rms_db"] = m["rms_db"]
            row[f"{side}_centroid"] = m["centroid"]
            row[f"{side}_f0"] = m["f0"]
            row[f"{side}_fc"] = m["fc"]
            row[f"{side}_q"] = m["q"]
            if args.metric in ("psd", "fit"):
                row[f"{side}_psd"] = m["psd"]
            else:
                row[f"{side}_env"] = m["env"]
        rows.append(row)
        print(f"  {name} = {val:>3}: emu fc {fmt_q(emu['fc'], 'Hz')} Q {fmt_q(emu['q'], '')}"
              f" | abd fc {fmt_q(abd['fc'], 'Hz')} Q {fmt_q(abd['q'], '')}"
              f" | Δ RMS {row['d_rms_db']:+.1f}", flush=True)

    if args.metric == "fit":
        ref0 = rows[min(max(args.ref, 0), len(rows) - 1)]
        for side in ("emu", "abd"):
            fc0 = fc_from_psd(*ref0[f"{side}_psd"])
            print(f"  fc0({side}) = {fc0:.0f} Hz con {name} = {ref0['value']}")
            for r in rows:
                fc, q, err = fit_svf(*r[f"{side}_psd"], fc0)
                r[f"{side}_fc"], r[f"{side}_q"] = fc, q
                print(f"    {side} {name} {r['value']:>3}: fc {fmt_q(fc, 'Hz')} "
                      f"Q {fmt_q(q, '')} residuo {err:.1f} dB", flush=True)

    ref = rows[min(max(args.ref, 0), len(rows) - 1)]
    for r in rows:
        for side in ("emu", "abd"):
            db, hz = ref_value(r, ref, side, args.metric)
            r[f"{side}_ref_db"], r["ref_hz"] = db, hz

    write_csv(args.out_csv, rows)
    write_md(args.out_md, rows, f"{name} [{args.metric}] — #{args.program:03d}, nota "
                                f"{args.note} (referencia: {ref['value']})")
    print(f"  -> {args.out_csv}\n  -> {args.out_md}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
