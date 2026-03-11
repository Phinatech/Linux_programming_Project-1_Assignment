"""
stats_pure.py — Pure Python heavy numeric computation.

Computes mean, variance, standard deviation, min, max,
and median on a large array of floats using only built-in
Python constructs (no numpy / external libraries).
"""

import time
import random
import math


def py_mean(data):
    """Arithmetic mean."""
    total = 0.0
    for x in data:
        total += x
    return total / len(data)


def py_variance(data, mean):
    """Population variance."""
    total = 0.0
    for x in data:
        diff = x - mean
        total += diff * diff
    return total / len(data)


def py_stddev(data, mean):
    """Population standard deviation."""
    return math.sqrt(py_variance(data, mean))


def py_minmax(data):
    """Find minimum and maximum in a single pass."""
    lo = data[0]
    hi = data[0]
    for x in data[1:]:
        if x < lo:
            lo = x
        if x > hi:
            hi = x
    return lo, hi


def py_median(data):
    """Median via sort (copy, does not mutate original)."""
    s = sorted(data)
    n = len(s)
    mid = n // 2
    if n % 2 == 0:
        return (s[mid - 1] + s[mid]) / 2.0
    return float(s[mid])


def compute_stats_python(data):
    """Run all statistics and return a results dict."""
    mean   = py_mean(data)
    var    = py_variance(data, mean)
    stddev = py_stddev(data, mean)
    lo, hi = py_minmax(data)
    median = py_median(data)
    return {
        "mean":   mean,
        "var":    var,
        "stddev": stddev,
        "min":    lo,
        "max":    hi,
        "median": median,
    }


def benchmark(n=1_000_000, runs=5):
    """
    Generate a random dataset of size n, then run the full
    statistics suite `runs` times, reporting wall-clock time.
    """
    random.seed(42)
    data = [random.gauss(50.0, 15.0) for _ in range(n)]

    print(f"[Pure Python] Array size : {n:,}")
    print(f"[Pure Python] Runs       : {runs}")
    print()

    times = []
    for i in range(runs):
        t0 = time.perf_counter()
        result = compute_stats_python(data)
        elapsed = time.perf_counter() - t0
        times.append(elapsed)
        print(f"  Run {i+1}: {elapsed:.4f}s")

    avg = sum(times) / runs
    best = min(times)
    print()
    print(f"[Pure Python] Average    : {avg:.4f}s")
    print(f"[Pure Python] Best       : {best:.4f}s")
    print()
    print("[Pure Python] Results:")
    for k, v in result.items():
        print(f"  {k:8s} = {v:.6f}")
    return avg, best, result


if __name__ == "__main__":
    benchmark()
