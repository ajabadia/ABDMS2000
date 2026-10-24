#!/usr/bin/env python
"""Estimate LFO rate from paired long C4 captures with LFO1 -> Pitch enabled.

Capture format is CalibrationTests' existing binary format: uint32 frame count,
then planar little-endian float32 left and right channels. A windowed autocorrelation
tracks the fundamental pitch, and a spectral peak in pitch cents estimates LFO rate.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

import numpy as np

SAMPLE_RATE = 48_000.0
FRAME_SAMPLES = 1024
FRAME_HOP = 120


def read_capture(path: Path) -> np.ndarray:
    raw = path.read_bytes()
    if len(raw) < 4:
        raise ValueError(f"{path}: missing frame count")
    frames = struct.unpack_from("<I", raw)[0]
    expected = 4 + frames * 2 * 4
    if len(raw) != expected:
        raise ValueError(f"{path}: expected {expected} bytes for {frames} frames, got {len(raw)}")
    planar = np.frombuffer(raw, dtype="<f4", offset=4).reshape(2, frames)
    if not np.isfinite(planar).all():
        raise ValueError(f"{path}: non-finite audio sample")
    return (planar[0].astype(np.float64) + planar[1].astype(np.float64)) * 0.5


def track_fundamental(audio: np.ndarray, sample_rate: float = SAMPLE_RATE,
                      skip_seconds: float = 0.12) -> tuple[np.ndarray, np.ndarray]:
    """Return midpoint times and autocorrelation f0 estimates for voiced windows."""
    start = max(0, int(skip_seconds * sample_rate))
    x = np.asarray(audio[start:], dtype=np.float64)
    if x.size < int(sample_rate * 0.20):
        raise ValueError("capture must contain at least 0.20 s after onset")
    if not np.isfinite(x).all():
        raise ValueError("audio contains non-finite samples")

    window = np.hanning(FRAME_SAMPLES)
    nfft = 4096
    min_lag = int(sample_rate / 1200.0)
    max_lag = int(sample_rate / 55.0)
    times: list[float] = []
    pitches: list[float] = []
    for offset in range(0, x.size - FRAME_SAMPLES, FRAME_HOP):
        frame = x[offset:offset + FRAME_SAMPLES]
        frame = (frame - frame.mean()) * window
        spectrum = np.fft.rfft(frame, nfft)
        autocorrelation = np.fft.irfft(spectrum * spectrum.conj(), nfft)[:max_lag + 2]
        if autocorrelation[0] <= 1e-20:
            continue
        autocorrelation /= autocorrelation[0]
        local = np.flatnonzero(
            (autocorrelation[min_lag + 1:max_lag - 1] > autocorrelation[min_lag:max_lag - 2])
            & (autocorrelation[min_lag + 1:max_lag - 1] >= autocorrelation[min_lag + 2:max_lag])
        ) + min_lag + 1
        if local.size == 0:
            continue
        # The strongest periodic autocorrelation peak avoids confusing the second
        # harmonic for f0 on the bright saw diagnostic patch.
        lag = int(local[np.argmax(autocorrelation[local])])
        denom = autocorrelation[lag - 1] - 2.0 * autocorrelation[lag] + autocorrelation[lag + 1]
        correction = (0.5 * (autocorrelation[lag - 1] - autocorrelation[lag + 1]) / denom
                      if abs(denom) > 1e-12 else 0.0)
        times.append((start + offset + FRAME_SAMPLES * 0.5) / sample_rate)
        pitches.append(sample_rate / (lag + correction))

    if len(pitches) < 40:
        raise ValueError(f"too few voiced pitch frames ({len(pitches)})")
    return np.asarray(times), np.asarray(pitches)


def estimate_rate(audio: np.ndarray, sample_rate: float = SAMPLE_RATE,
                  skip_seconds: float = 0.12) -> dict[str, float | int]:
    times, pitch_hz = track_fundamental(audio, sample_rate, skip_seconds)
    cents = 1200.0 * np.log2(pitch_hz / np.median(pitch_hz))
    centered_time = times - times.mean()
    slope = np.dot(centered_time, cents) / max(np.dot(centered_time, centered_time), 1e-30)
    cents -= slope * centered_time

    dt = float(np.median(np.diff(times)))
    track_rate = 1.0 / dt
    uniform_step = FRAME_HOP / sample_rate
    uniform_times = np.arange(times[0], times[-1], uniform_step)
    uniform_cents = np.interp(uniform_times, times, cents)
    uniform_cents -= uniform_cents.mean()
    window = np.hanning(uniform_cents.size)
    spectrum = np.abs(np.fft.rfft(uniform_cents * window))
    frequencies = np.fft.rfftfreq(uniform_cents.size, d=uniform_step)
    minimum = max(0.10, 1.5 / max(times[-1] - times[0], 1e-9))
    maximum = min(45.0, track_rate * 0.40)
    search = np.flatnonzero((frequencies >= minimum) & (frequencies <= maximum))
    if search.size == 0:
        raise ValueError("capture duration/rate provides no usable LFO-frequency bins")
    peak_bin = int(search[np.argmax(spectrum[search])])
    rate_hz = float(frequencies[peak_bin])
    if peak_bin == 0:
        raise ValueError("no measurable LFO-rate component above the detrended/DC bin")
    if 1 <= peak_bin < spectrum.size - 1:
        a, b, c = np.log(np.maximum(spectrum[peak_bin - 1:peak_bin + 2], 1e-30))
        denom = a - 2.0 * b + c
        if abs(denom) > 1e-15:
            offset = float(np.clip(0.5 * (a - c) / denom, -0.5, 0.5))
            rate_hz += offset * float(frequencies[1] - frequencies[0])

    peak_amp = float(spectrum[peak_bin])
    noise_mask = np.ones(spectrum.size, dtype=bool)
    noise_mask[max(0, peak_bin - 2):min(noise_mask.size, peak_bin + 3)] = False
    noise_mask[:max(1, int(np.ceil(minimum / uniform_step * uniform_cents.size)))] = False
    noise_floor = float(np.median(spectrum[noise_mask])) if np.any(noise_mask) else 0.0
    prominence_db = 20.0 * np.log10(max(peak_amp, 1e-30) / max(noise_floor, peak_amp * 1e-8, 1e-30))
    return {
        "rate_hz": rate_hz,
        "prominence_db": float(prominence_db),
        "pitch_span_cents": float(np.percentile(cents, 95) - np.percentile(cents, 5)),
        "pitch_median_hz": float(np.median(pitch_hz)),
        "pitch_track_rate_hz": track_rate,
        "cycles_observed": float((times[-1] - times[0]) * rate_hz),
        "pitch_samples": int(pitch_hz.size),
    }


def synthetic_self_test() -> None:
    # Sawtooth fundamental with known sinusoidal pitch modulation.
    seconds = 5.0
    time = np.arange(int(SAMPLE_RATE * seconds), dtype=np.float64) / SAMPLE_RATE
    inst_hz = 261.6256 * np.power(2.0, (0.45 * np.sin(2.0 * np.pi * 3.2 * time)) / 12.0)
    phase = np.cumsum(inst_hz) / SAMPLE_RATE
    audio = 2.0 * (phase - np.floor(phase)) - 1.0
    result = estimate_rate(audio, skip_seconds=0.1)
    error = abs(float(result["rate_hz"]) - 3.2)
    if error > 0.20 or float(result["prominence_db"]) < 5.0:
        raise SystemExit(f"self-test failed: {result}, absolute error {error:.4f} Hz")
    print(f"SELF-TEST PASS: injected 3.200 Hz, measured {result['rate_hz']:.4f} Hz; "
          f"prominence {result['prominence_db']:.1f} dB")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="*", type=Path, help="paired comparator .bin captures")
    parser.add_argument("--self-test", action="store_true", help="check estimator on synthetic 3.2 Hz PM")
    args = parser.parse_args()
    if args.self_test:
        synthetic_self_test()
    for path in args.captures:
        result = estimate_rate(read_capture(path))
        print(f"{path}: rate={result['rate_hz']:.5f} Hz, prominence={result['prominence_db']:.1f} dB, "
              f"pitch-span(p5..p95)={result['pitch_span_cents']:.1f} cents, "
              f"cycles={result['cycles_observed']:.2f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
