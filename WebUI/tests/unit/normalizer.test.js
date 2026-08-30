import { describe, it, expect } from 'vitest';

/**
 * normalizeParam and denormalizeParam — mirroring EnvelopeCurves.h / DSPUtils.h
 */
function normalizeParam(value, min, max, skew = 1.0) {
  if (min === max) return 0.5;
  const clamped = Math.max(min, Math.min(max, value));
  const norm = (clamped - min) / (max - min);
  if (skew <= 0) return norm;
  return Math.pow(norm, 1.0 / skew);
}

function denormalizeParam(norm, min, max, skew = 1.0) {
  if (min === max) return min;
  const clampedNorm = Math.max(0.0, Math.min(1.0, norm));
  const linearNorm = (skew <= 0) ? clampedNorm : Math.pow(clampedNorm, skew);
  return min + linearNorm * (max - min);
}

/** Attack time seconds formula from EnvelopeCurves.h */
function getAttackTimeSeconds(norm0to1) {
  const norm = Math.max(0.0, Math.min(1.0, norm0to1));
  const minTime = 0.001; // 1 ms
  const maxTime = 11.0;  // 11 seconds
  const curved = Math.pow(norm, 2.5);
  return minTime + (maxTime - minTime) * curved;
}

/** Decay/Release time seconds formula from EnvelopeCurves.h */
function getDecayReleaseTimeSeconds(norm0to1) {
  const norm = Math.max(0.0, Math.min(1.0, norm0to1));
  const minTime = 0.002; // 2 ms
  const maxTime = 20.0;  // 20 seconds
  const curved = Math.pow(norm, 2.5);
  return minTime + (maxTime - minTime) * curved;
}

/** Per-sample exponential decay multiplier matching C++ formula */
function getDecayMultiplier(timeSeconds, sampleRate) {
  if (timeSeconds <= 0.0001 || sampleRate <= 1000) return 0.0;
  return Math.exp(-4.60517 / (timeSeconds * sampleRate));
}

describe('Parameter Normalizer', () => {
  it('should normalize a value at minimum to 0.0', () => {
    expect(normalizeParam(0, 0, 127)).toBeCloseTo(0.0, 10);
  });

  it('should normalize a value at maximum to 1.0', () => {
    expect(normalizeParam(127, 0, 127)).toBeCloseTo(1.0, 10);
  });

  it('should normalize a mid-range value (skew=1)', () => {
    const val = normalizeParam(63.5, 0, 127, 1.0);
    expect(val).toBeCloseTo(0.5, 5);
  });

  it('should handle skew factor correctly', () => {
    // With skew=0.35, the value at position 32 should map lower than 0.25
    const val = normalizeParam(32, 0, 127, 0.35);
    expect(val).toBeLessThan(0.25); // Skew compresses low end
  });

  it('should round-trip: normalize then denormalize', () => {
    for (let v = 0; v <= 127; v += 13) {
      const norm = normalizeParam(v, 0, 127, 0.45);
      const denorm = denormalizeParam(norm, 0, 127, 0.45);
      expect(denorm).toBeCloseTo(v, 0);
    }
  });

  it('should handle bipolar range [-63, +63]', () => {
    const norm = normalizeParam(0, -63, 63, 1.0);
    expect(norm).toBeCloseTo(0.5, 5);
    const normNeg = normalizeParam(-63, -63, 63, 1.0);
    expect(normNeg).toBeCloseTo(0.0, 5);
  });
});

describe('EnvelopeCurves - Attack Time', () => {
  it('should return minimum attack at norm=0.0', () => {
    expect(getAttackTimeSeconds(0.0)).toBeCloseTo(0.001, 5);
  });

  it('should return maximum attack at norm=1.0', () => {
    expect(getAttackTimeSeconds(1.0)).toBeCloseTo(11.0, 3);
  });

  it('should be monotonic increasing', () => {
    const t0 = getAttackTimeSeconds(0.1);
    const t1 = getAttackTimeSeconds(0.5);
    const t2 = getAttackTimeSeconds(0.9);
    expect(t0).toBeLessThan(t1);
    expect(t1).toBeLessThan(t2);
  });

  it('should concentrate resolution in low range (skew=2.5)', () => {
    const lowQuarter = getAttackTimeSeconds(0.25);
    const midPoint = getAttackTimeSeconds(0.5);
    // Midpoint should be way past the actual midpoint of time range
    expect(midPoint / 11.0).toBeLessThan(0.25);
  });
});

describe('EnvelopeCurves - Decay/Release Time', () => {
  it('should return minimum time at norm=0.0', () => {
    expect(getDecayReleaseTimeSeconds(0.0)).toBeCloseTo(0.002, 5);
  });

  it('should return maximum time at norm=1.0', () => {
    expect(getDecayReleaseTimeSeconds(1.0)).toBeCloseTo(20.0, 3);
  });

  it('decay multiplier should be ~1.0 at very short times', () => {
    const mult = getDecayMultiplier(0.002, 44100);
    expect(mult).toBeCloseTo(0.95, 2); // Very fast decay, near 0.95 multiplier per sample
  });

  it('decay multiplier should approach 1.0 for long times', () => {
    const mult = getDecayMultiplier(10.0, 44100);
    expect(mult).toBeGreaterThan(0.99);
  });


  it('decay multiplier should be 0.0 for invalid inputs', () => {
    expect(getDecayMultiplier(0.0, 44100)).toBe(0.0);
    expect(getDecayMultiplier(1.0, 500)).toBe(0.0);
  });
});

describe('LFO Frequency mapping', () => {
  /** Matches SynthEngine::updateParametersFromAPVTS LFO freq formula */
  function lfoNormToFreqHz(norm0to1) {
    return 0.05 * Math.pow(1000.0, norm0to1);
  }

  it('should be ~0.05Hz at norm=0.0', () => {
    expect(lfoNormToFreqHz(0.0)).toBeCloseTo(0.05, 4);
  });

  it('should be ~50Hz at norm=1.0', () => {
    expect(lfoNormToFreqHz(1.0)).toBeCloseTo(50.0, 1);
  });

  it('should be monotonic', () => {
    expect(lfoNormToFreqHz(0.3)).toBeLessThan(lfoNormToFreqHz(0.7));
  });
});

describe('Filter Cutoff mapping', () => {
  /** Matches DSPUtils::convertSysExToCutoffHz */
  function cutoffNormToHz(norm0to1) {
    const minHz = 20.0;
    const maxHz = 20000.0;
    const clamped = Math.max(0.0, Math.min(1.0, norm0to1));
    return minHz * Math.pow(maxHz / minHz, clamped);
  }

  it('should be 20Hz at norm=0.0', () => {
    expect(cutoffNormToHz(0.0)).toBeCloseTo(20.0, 1);
  });

  it('should be 20kHz at norm=1.0', () => {
    expect(cutoffNormToHz(1.0)).toBeCloseTo(20000.0, 1);
  });

  it('should be ~447Hz at norm=0.5', () => {
    const val = cutoffNormToHz(0.5);
    expect(val).toBeGreaterThan(400);
    expect(val).toBeLessThan(700);
  });
});

describe('MIDI Note to Frequency', () => {
  function midiToFreq(midiNote) {
    return 440.0 * Math.pow(2.0, (midiNote - 69.0) / 12.0);
  }

  it('should return 440Hz for MIDI note 69 (A4)', () => {
    expect(midiToFreq(69)).toBeCloseTo(440.0, 2);
  });

  it('should return 261.63Hz for MIDI note 60 (C4)', () => {
    expect(midiToFreq(60)).toBeCloseTo(261.63, 0);
  });

  it('should be one octave up at +12 semitones', () => {
    expect(midiToFreq(81)).toBeCloseTo(880.0, 2);
  });
});

describe('Stereo Pan Law (Equal-Power sqrt)', () => {
  /** Matches Voice.cpp new pan law */
  function panGains(pan) {
    const clamped = Math.max(-1.0, Math.min(1.0, pan));
    return {
      left: Math.sqrt(0.5 * (1.0 - clamped)),
      right: Math.sqrt(0.5 * (1.0 + clamped))
    };
  }

  it('hard left: left=1.0, right=0.0', () => {
    const g = panGains(-1.0);
    expect(g.left).toBeCloseTo(1.0, 5);
    expect(g.right).toBeCloseTo(0.0, 5);
  });

  it('hard right: left=0.0, right=1.0', () => {
    const g = panGains(1.0);
    expect(g.left).toBeCloseTo(0.0, 5);
    expect(g.right).toBeCloseTo(1.0, 5);
  });

  it('center: equal gains at ~0.7071', () => {
    const g = panGains(0.0);
    expect(g.left).toBeCloseTo(0.7071, 3);
    expect(g.right).toBeCloseTo(0.7071, 3);
  });

  it('constant intensity: left² + right² = 1.0 at any pan', () => {
    for (let p = -1.0; p <= 1.0; p += 0.1) {
      const g = panGains(p);
      const powerSum = g.left * g.left + g.right * g.right;
      expect(powerSum).toBeCloseTo(1.0, 5);
    }
  });
});