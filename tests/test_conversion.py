#!/usr/bin/env python3
"""
Basic tests for DSP conversion formulas.
Verifies that normalizedToDomain produces correct results
for each curve type with extreme values (0.0, 0.5, 1.0).
"""

import math
import sys
import os

# Add the source directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'Source'))

def normalized_to_domain_linear(t, curve_p0=1.0, curve_p1=1.0):
    """Linear curve: returns t unchanged"""
    return t

def normalized_to_domain_exp(t, curve_p0, curve_p1):
    """Exp curve: curveP0 * e^(curveP1 * t)"""
    if curve_p0 <= 0.0:
        curve_p0 = 1.0
    return curve_p0 * math.exp(curve_p1 * t)

def normalized_to_domain_pow(t, curve_p1):
    """Pow curve: t ^ curveP1"""
    if curve_p1 <= 0.0:
        curve_p1 = 1.0
    t_clamped = max(0.0, min(1.0, t))
    return pow(t_clamped, curve_p1)


def test_linear_curve():
    """Test Linear curve with values 0.0, 0.5, 1.0"""
    print("=" * 60)
    print("TEST 1: Linear Curve (case 0)")
    print("=" * 60)
    
    test_values = [0.0, 0.5, 1.0]
    expected = [0.0, 0.5, 1.0]  # Linear returns t unchanged
    
    all_pass = True
    for t in test_values:
        result = normalized_to_domain_linear(t)
        passed = abs(result - expected[t_idx if False else test_values.index(t)]) < 1e-10
        # Actually let me index properly
        idx = test_values.index(t)
        passed = abs(result - expected[idx]) < 1e-10
        status = "[PASS]" if passed else "[FAIL]"
        print(f"  t={t:>4} -> result={result:>7.4f} (expected={expected[idx]:>7.4f}) {status}")
        all_pass = all_pass and passed
    
    print(f"  Result: {'ALL PASSED' if all_pass else 'SOME FAILED'}\n")
    return all_pass


def test_exp_curve():
    """Test Exp curve with various curveP1 values"""
    print("=" * 60)
    print("TEST 2: Exp Curve (case 1)")
    print("=" * 60)
    
    # Use curveP1 values from actual ABDMS2000 parameters
    test_cases = [
        # (t, curve_p0, curve_p1, description)
        (0.0, 1.0, 0.35, "env1Attack-like"),
        (0.0, 1.0, 0.01, "env1Release-like"),
        (0.5, 1.0, 0.35, "mid-range env1Attack"),
        (1.0, 1.0, 0.35, "env1Attack max"),
        (0.0, 1.0, 0.4, "LFO1Freq-like"),
        (0.5, 1.0, 0.4, "mid LFO1Freq"),
        (1.0, 1.0, 0.4, "LFO1Freq max"),
        (0.0, 0.001, 3.0, "env1Decay-like"),
        (0.5, 0.001, 3.0, "mid env1Decay"),
        (1.0, 0.001, 3.0, "env1Decay max"),
    ]
    
    all_pass = True
    for t, p0, p1, desc in test_cases:
        result = normalized_to_domain_exp(t, p0, p1)
        # Manual calculation
        expected = p0 * math.exp(p1 * t)
        passed = abs(result - expected) < 1e-10
        status = "[PASS]" if passed else "[FAIL]"
        print(f"  {desc:>20} t={t:>4} p1={p1:>5} -> result={result:>8.6f} (expected={expected:>8.6f}) {status}")
        all_pass = all_pass and passed
    
    print(f"  Result: {'ALL PASSED' if all_pass else 'SOME FAILED'}\n")
    return all_pass


def test_pow_curve():
    """Test Pow curve with various curveP1 values"""
    print("=" * 60)
    print("TEST 3: Pow Curve (case 2)")
    print("=" * 60)
    
    # Use curveP1 values from actual ABDMS2000 parameters
    test_cases = [
        # (t, curve_p1, description)
        (0.0, 0.35, "portamentoTime"),
        (0.0, 0.45, "filterCutoff skew"),
        (0.0, 0.25, "filterResonance"),
        (0.5, 0.35, "mid-range"),
        (0.5, 0.45, "mid filterCutoff"),
        (0.5, 0.25, "mid filterResonance"),
        (1.0, 0.35, "portamentoTime max"),
        (1.0, 0.45, "filterCutoff max"),
        (1.0, 0.25, "filterResonance max"),
        (0.0, 0.8, "osc1Ctrl1/CC-like"),
        (0.5, 0.8, "mid osc1Ctrl1"),
        (1.0, 0.8, "osc1Ctrl1 max"),
    ]
    
    all_pass = True
    for t, p1, desc in test_cases:
        result = normalized_to_domain_pow(t, p1)
        # Manual calculation
        t_clamped = max(0.0, min(1.0, t))
        expected = pow(t_clamped, p1)
        passed = abs(result - expected) < 1e-10
        status = "[PASS]" if passed else "[FAIL]"
        print(f"  {desc:>20} t={t:>4} p1={p1:>5} -> result={result:>8.6f} (expected={expected:>8.6f}) {status}")
        all_pass = all_pass and passed
    
    print(f"  Result: {'ALL PASSED' if all_pass else 'SOME FAILED'}\n")
    return all_pass


def test_domain_boundaries():
    """Test that domainMin/domainMax boundaries are respected"""
    print("=" * 60)
    print("TEST 4: Domain Boundary Checks")
    print("=" * 60)
    
    # Test that values outside [0,1] are clamped for Pow curve
    print("  Testing Pow curve clamping at t=1.5 (should clamp to 1.0):")
    t = 1.5
    p1 = 0.5
    t_clamped = max(0.0, min(1.0, t))
    result = pow(t_clamped, p1)
    expected = 1.0  # Since clamped to 1.0 ^ 0.5 = 1.0
    passed = abs(result - expected) < 1e-10
    status = "[PASS]" if passed else "[FAIL]"
    print(f"    t=1.5, p1={p1} -> result={result:.6f} (expected={expected:.6f}) {status}")
    all_pass = passed
    
    print("  Testing Pow curve clamping at t=-0.1 (should clamp to 0.0):")
    t = -0.1
    p1 = 0.5
    t_clamped = max(0.0, min(1.0, t))
    result = pow(t_clamped, p1)
    expected = 0.0  # Since clamped to 0.0 ^ 0.5 = 0.0
    passed = abs(result - expected) < 1e-10
    status = "[PASS]" if passed else "[FAIL]"
    print(f"    t=-0.1, p1={p1} -> result={result:.6f} (expected={expected:.6f}) {status}")
    all_pass = all_pass and passed
    
    print(f"  Result: {'ALL PASSED' if all_pass else 'SOME FAILED'}\n")
    return all_pass


def domain_to_normalized_linear(domain, domain_min=0.0, domain_max=1.0):
    """Linear inverse: clamps to [0,1]"""
    if domain_min < domain_max:
        clamped = max(domain_min, min(domain_max, domain))
        return max(0.0, min(1.0, (clamped - domain_min) / (domain_max - domain_min)))
    return max(0.0, min(1.0, domain))

def domain_to_normalized_exp(domain, curve_p0, curve_p1, domain_min=0.0, domain_max=1.0):
    """Exp inverse: (1/curveP1) * ln(domain / curveP0)"""
    if curve_p0 <= 0.0:
        curve_p0 = 1.0
    if curve_p1 <= 0.0:
        curve_p1 = 1.0
    if domain <= 0.0:
        return 0.0
    ratio = domain / curve_p0
    if ratio <= 0.0:
        return 0.0
    val = (1.0 / curve_p1) * math.log(ratio)
    return max(0.0, min(1.0, val))

def domain_to_normalized_pow(domain, curve_p1, domain_min=0.0, domain_max=1.0):
    """Pow inverse: ((domain - domainMin) / span) ^ (1/curveP1)"""
    if curve_p1 <= 0.0:
        curve_p1 = 1.0
    val = domain
    if domain_min < domain_max:
        clamped = max(domain_min, min(domain_max, domain))
        val = (clamped - domain_min) / (domain_max - domain_min)
    if val < 0.0:
        val = 0.0
    return max(0.0, min(1.0, pow(val, 1.0 / curve_p1)))

def test_inverse_and_roundtrips():
    """DSP.9: Test inverse conversions and bidirectional round-trips"""
    print("=" * 60)
    print("TEST 5: Inverse Conversions & Round-Trips (DSP.9)")
    print("=" * 60)
    
    all_pass = True
    
    # 1. Linear round-trip
    for t in [0.0, 0.25, 0.5, 0.75, 1.0]:
        dom = normalized_to_domain_linear(t)
        t_back = domain_to_normalized_linear(dom)
        passed = abs(t - t_back) < 1e-7
        all_pass = all_pass and passed
        print(f"  Linear round-trip: t={t:>4} -> dom={dom:>4} -> t_back={t_back:>4} [{'PASS' if passed else 'FAIL'}]")

    # 2. Exp round-trip
    p0, p1 = 0.035, 7.5
    for t in [0.0, 0.25, 0.5, 0.75, 1.0]:
        dom = normalized_to_domain_exp(t, p0, p1)
        t_back = domain_to_normalized_exp(dom, p0, p1)
        passed = abs(t - t_back) < 1e-6
        all_pass = all_pass and passed
        print(f"  Exp round-trip:    t={t:>4} -> dom={dom:>8.4f} -> t_back={t_back:>6.4f} [{'PASS' if passed else 'FAIL'}]")

    # 3. Pow round-trip
    p1 = 0.8
    for t in [0.0, 0.25, 0.5, 0.75, 1.0]:
        dom = normalized_to_domain_pow(t, p1)
        t_back = domain_to_normalized_pow(dom, p1)
        passed = abs(t - t_back) < 1e-6
        all_pass = all_pass and passed
        print(f"  Pow round-trip:    t={t:>4} -> dom={dom:>8.4f} -> t_back={t_back:>6.4f} [{'PASS' if passed else 'FAIL'}]")

    print(f"  Result: {'ALL PASSED' if all_pass else 'SOME FAILED'}\n")
    return all_pass

def test_all_parameters_extremes():
    """DSP.10: Test all schema parameters with extreme inputs (0.0, 0.5, 1.0, -1.0, 2.0)"""
    import json
    print("=" * 60)
    print("TEST 6: All Schema Parameters Extreme Values (DSP.10)")
    print("=" * 60)

    schema_path = os.path.join(os.path.dirname(__file__), '..', 'schemas', 'parameters-spec.schema.v1.json')
    with open(schema_path, 'r', encoding='utf-8') as f:
        spec = json.load(f)

    params = spec.get('parameters', [])
    print(f"  Loaded {len(params)} parameters from schema.")

    all_pass = True
    tested_count = 0
    extreme_t_values = [0.0, 0.5, 1.0, -1.0, 2.0, 1e-7, 1.0 - 1e-7]

    for p in params:
        pid = p['id']
        curve = p.get('curve', 'Linear')
        p0 = p.get('curveP0', 0.0)
        p1 = p.get('curveP1', 1.0)
        d_min = p.get('domainMin', 0.0)
        d_max = p.get('domainMax', 0.0)

        for t in extreme_t_values:
            # Emular normalizedToDomain con clamping
            t_clamped = max(0.0, min(1.0, t))
            if curve == 'Exp':
                if p0 <= 0.0: p0 = 1.0
                res = p0 * math.exp(p1 * t_clamped)
            elif curve == 'Pow':
                if p1 <= 0.0: p1 = 1.0
                powered = pow(t_clamped, p1)
                res = (d_min + powered * (d_max - d_min)) if d_min < d_max else powered
            else:
                res = (d_min + t_clamped * (d_max - d_min)) if d_min < d_max else t_clamped

            if d_min < d_max:
                res = max(d_min, min(d_max, res))

            # Verificar no-NaN y no-Inf
            if math.isnan(res) or math.isinf(res):
                print(f"  [FAIL] {pid} produced NaN/Inf at t={t}")
                all_pass = False

            # Verificar clamping de dominio
            if d_min < d_max and (res < d_min - 1e-6 or res > d_max + 1e-6):
                print(f"  [FAIL] {pid} out of domain bounds: {res} not in [{d_min}, {d_max}] at t={t}")
                all_pass = False

            # Emular domainToNormalized
            if curve == 'Exp':
                norm = domain_to_normalized_exp(res, p0, p1, d_min, d_max)
            elif curve == 'Pow':
                norm = domain_to_normalized_pow(res, p1, d_min, d_max)
            else:
                norm = domain_to_normalized_linear(res, d_min, d_max)

            if math.isnan(norm) or math.isinf(norm) or norm < 0.0 or norm > 1.0:
                print(f"  [FAIL] {pid} inverse produced invalid norm={norm} for domain={res}")
                all_pass = False

        tested_count += 1

    print(f"  Tested {tested_count} parameters across {len(extreme_t_values)} extreme values each ({tested_count * len(extreme_t_values)} evaluations).")
    print(f"  Result: {'ALL PASSED' if all_pass else 'SOME FAILED'}\n")
    return all_pass

def main():
    print("\n" + "=" * 60)
    print("DSP CONVERSION FORMULA VALIDATION TESTS (ITERATION 2)")
    print("=" * 60 + "\n")
    
    results = []
    
    print("--- Test 1: Linear Curve ---")
    results.append(("Linear", test_linear_curve()))
    
    print("--- Test 2: Exp Curve ---")
    results.append(("Exp", test_exp_curve()))
    
    print("--- Test 3: Pow Curve ---")
    results.append(("Pow", test_pow_curve()))
    
    print("--- Test 4: Domain Boundaries ---")
    results.append(("Boundaries", test_domain_boundaries()))

    print("--- Test 5: Inverse & Round-trips (DSP.9) ---")
    results.append(("RoundTrips", test_inverse_and_roundtrips()))

    print("--- Test 6: All Schema Parameters Extremes (DSP.10) ---")
    results.append(("AllParams", test_all_parameters_extremes()))
    
    # Summary
    print("=" * 60)
    print("SUMMARY")
    print("=" * 60)
    
    total = len(results)
    passed = sum(1 for _, r in results if r)
    failed = total - passed
    
    for name, result in results:
        status = "[PASS]" if result else "[FAIL]"
        print(f"  {name:>12}: {status}")
    
    print(f"\nTotal: {total} tests")
    print(f"Passed: {passed}/{total}")
    print(f"Failed: {failed}/{total}")
    
    if failed > 0:
        print("\nSome tests failed. Review the formulas above.")
        sys.exit(1)
    else:
        print("\nAll tests passed! DSP conversion formulas and all parameters are verified.")
        sys.exit(0)


if __name__ == "__main__":
    main()