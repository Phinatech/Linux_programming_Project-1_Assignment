"""
benchmark.py — Head-to-head benchmark: Pure Python vs C extension.

Runs both implementations on identical datasets at three sizes,
prints a detailed results table, and verifies numerical agreement.
"""

import time
import random
import math
import sys
import platform

import stats          # C extension
from stats_pure import compute_stats_python


# ── helpers ──────────────────────────────────────────────────────────────────

def make_dataset(n, seed=42):
    random.seed(seed)
    return [random.gauss(50.0, 15.0) for _ in range(n)]


def run_timed(fn, data, runs=5):
    """Return (results_dict, avg_time, best_time)."""
    times = []
    result = None
    for _ in range(runs):
        t0 = time.perf_counter()
        result = fn(data)
        times.append(time.perf_counter() - t0)
    return result, sum(times) / runs, min(times)


def check_agreement(r_py, r_c, tol=1e-9):
    """Assert both implementations return numerically equal results."""
    keys = ["mean", "var", "stddev", "min", "max", "median"]
    ok = True
    for k in keys:
        if abs(r_py[k] - r_c[k]) > tol:
            print(f"  [MISMATCH] {k}: Python={r_py[k]:.10f}  C={r_c[k]:.10f}")
            ok = False
    return ok


def bar(ratio, width=30):
    """ASCII progress bar scaled to ratio (C time / Python time)."""
    filled = max(1, int(ratio * width))
    return "[" + "#" * min(filled, width) + "-" * max(0, width - filled) + "]"


# ── main benchmark ────────────────────────────────────────────────────────────

def main():
    SIZES = [100_000, 500_000, 1_000_000]
    RUNS  = 5

    print("=" * 70)
    print("  PERFORMANCE BENCHMARK: Pure Python vs C Extension (stats module)")
    print("=" * 70)
    print(f"  Python  : {sys.version.split()[0]}")
    print(f"  Platform: {platform.machine()} / {platform.system()}")
    print(f"  Runs    : {RUNS} per configuration")
    print("=" * 70)
    print()

    summary_rows = []   # (size, py_avg, c_avg, speedup)

    for n in SIZES:
        print(f"── Array size: {n:,} elements ──────────────────────────────")
        data = make_dataset(n)

        # Pure Python
        r_py, py_avg, py_best = run_timed(compute_stats_python, data, RUNS)
        print(f"  Pure Python  avg={py_avg:.4f}s  best={py_best:.4f}s")

        # C extension
        r_c,  c_avg,  c_best  = run_timed(lambda d: stats.compute(d), data, RUNS)
        print(f"  C Extension  avg={c_avg:.4f}s  best={c_best:.4f}s")

        # Verify results match
        match = check_agreement(r_py, r_c)
        print(f"  Results match: {'YES ✓' if match else 'NO ✗'}")

        speedup = py_avg / c_avg
        print(f"  Speedup      : {speedup:.1f}x  {bar(1.0 / speedup)}")
        print()

        summary_rows.append((n, py_avg, c_avg, speedup))

    # ── Summary table ──────────────────────────────────────────────────────
    print("=" * 70)
    print("  SUMMARY TABLE")
    print("=" * 70)
    print(f"  {'Array Size':>12}  {'Python (s)':>12}  {'C Ext (s)':>12}  {'Speedup':>10}")
    print("  " + "-" * 64)
    for n, py_t, c_t, spd in summary_rows:
        print(f"  {n:>12,}  {py_t:>12.4f}  {c_t:>12.4f}  {spd:>9.1f}x")
    print()

    # ── Detailed results for largest dataset ──────────────────────────────
    r_py, _, _ = run_timed(compute_stats_python, make_dataset(SIZES[-1]), 1)
    r_c,  _, _ = run_timed(lambda d: stats.compute(d), make_dataset(SIZES[-1]), 1)

    print("  STATISTICAL RESULTS (n=1,000,000)")
    print("  " + "-" * 56)
    print(f"  {'Statistic':>10}  {'Pure Python':>18}  {'C Extension':>18}")
    print("  " + "-" * 56)
    for k in ["mean", "var", "stddev", "min", "max", "median"]:
        print(f"  {k:>10}  {r_py[k]:>18.6f}  {r_c[k]:>18.6f}")
    print("=" * 70)


if __name__ == "__main__":
    main()
