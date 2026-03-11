# Building a Python Performance Extension

A CPython C extension that implements high-performance statistical computation. This project demonstrates how to write a Python module in C to achieve significant speed improvements over pure Python implementations, without relying on external libraries like NumPy.

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [How It Works](#how-it-works)
- [Requirements](#requirements)
- [Setup](#setup)
- [Building the Extension](#building-the-extension)
- [Running the Benchmark](#running-the-benchmark)
- [API Reference](#api-reference)
- [Performance Results](#performance-results)
- [Algorithm Details](#algorithm-details)
- [Key Concepts](#key-concepts)

---

## Overview

This project contains two implementations of the same statistical functions:

| Implementation | File | Description |
|----------------|------|-------------|
| **C Extension** | `statsmodule.c` | Native C code compiled into a `.so` Python module |
| **Pure Python** | `stats_pure.py` | Standard Python implementation using only built-ins |

The C extension computes: **mean, variance, standard deviation, min, max, and median** — all in a single importable `stats` module.

---

## Project Structure

```
Building_a_Python_Performance_Extension/
├── statsmodule.c           # C source for the Python extension
├── setup.py                # Build script using setuptools
├── stats_pure.py           # Pure Python equivalent implementation
├── benchmark.py            # Head-to-head benchmark runner
├── benchmark_results.txt   # Sample benchmark output
├── stats.cpython-313-darwin.so  # Compiled extension (generated)
├── build/                  # Build artifacts (generated)
└── venv/                   # Python virtual environment (generated)
```

---

## How It Works

### C Extension (`statsmodule.c`)
- Written using the **CPython C API**
- Accepts any Python sequence (list, tuple, etc.) as input
- Unpacks all Python objects into a raw C `double[]` array in one pass
- All arithmetic (mean, variance, stddev, min, max, median) runs entirely in C with **no Python object overhead** in the hot loops
- Uses `qsort()` for median computation
- Returns results as a Python `dict`

### Pure Python (`stats_pure.py`)
- Uses only Python built-ins and `math.sqrt()`
- Implements the same algorithms for accurate comparison
- Each loop iteration has Python interpreter overhead per element

---

## Requirements

- Python 3.x (tested on 3.12 and 3.13)
- GCC or Clang compiler
- `setuptools` Python package
- macOS or Linux

---

## Setup

**Step 1 — Navigate to the project folder**
```bash
cd ~/Documents/C_programming_Y3T1/Linux_programming_Project-1_Assignment/Building_a_Python_Performance_Extension
```

**Step 2 — Create a virtual environment**
```bash
python3 -m venv venv
```

**Step 3 — Activate the virtual environment**
```bash
source venv/bin/activate
```

> Your terminal prompt will now show `(venv)` to confirm it is active.

**Step 4 — Install setuptools**
```bash
pip install setuptools
```

---

## Building the Extension

With the virtual environment active, run:

```bash
python3 setup.py build_ext --inplace
```

This will:
1. Compile `statsmodule.c` using your system's C compiler
2. Link against `libm` (for `sqrt()`)
3. Produce a shared library file like `stats.cpython-313-darwin.so` in the current directory

You should see output similar to:
```
running build_ext
building 'stats' extension
gcc -Wall -O2 ... statsmodule.c -o stats.cpython-313-darwin.so
```

---

## Running the Benchmark

With the virtual environment active:

```bash
python3 benchmark.py
```

This runs both implementations on datasets of **100,000 / 500,000 / 1,000,000** elements (5 runs each), then prints:
- Average and best time per implementation
- Whether the numerical results match (verification)
- Speedup factor
- A full summary table
- Detailed statistical results for n=1,000,000

You can also run the pure Python benchmark standalone:
```bash
python3 stats_pure.py
```

---

## API Reference

### `stats.compute(sequence) -> dict`

Accepts any Python sequence of numbers (list, tuple, etc.) and returns a dictionary with the following keys:

| Key | Type | Description |
|-----|------|-------------|
| `mean` | `float` | Arithmetic mean |
| `var` | `float` | Population variance |
| `stddev` | `float` | Population standard deviation |
| `min` | `float` | Minimum value |
| `max` | `float` | Maximum value |
| `median` | `float` | Median value |

**Example usage:**
```python
import stats

data = [1.0, 2.0, 3.0, 4.0, 5.0]
result = stats.compute(data)

print(result)
# {'mean': 3.0, 'var': 2.0, 'stddev': 1.4142..., 'min': 1.0, 'max': 5.0, 'median': 3.0}
```

**Raises:**
- `ValueError` — if the sequence is empty
- `TypeError` — if the sequence contains non-numeric values

---

## Performance Results

Benchmarked on x86_64 Linux, Python 3.12.3, 5 runs per configuration:

| Array Size | Pure Python (avg) | C Extension (avg) | Speedup |
|------------|-------------------|-------------------|---------|
| 100,000 | 0.0327s | 0.0169s | **1.9x** |
| 500,000 | 0.2131s | 0.0858s | **2.5x** |
| 1,000,000 | 0.4777s | 0.1843s | **2.6x** |

### Statistical Results (n = 1,000,000, seed = 42)

Both implementations produce **identical results** verified to 9 decimal places:

| Statistic | Value |
|-----------|-------|
| mean | 49.976787 |
| variance | 224.865354 |
| stddev | 14.995511 |
| min | -26.538841 |
| max | 123.912365 |
| median | 49.958360 |

---

## Algorithm Details

The C extension uses **4 passes** over the data:

| Pass | Operation | Details |
|------|-----------|---------|
| 1 | **Unboxing** | Convert Python objects → C `double[]` array |
| 2 | **Mean + Min/Max** | Single loop: accumulate sum, track lo/hi |
| 3 | **Variance** | Loop over deviations from mean: `Σ(x - mean)²` / n |
| 4 | **Median** | `memcpy` + `qsort` on a separate copy, then pick middle element |

Population variance formula used: `σ² = Σ(xᵢ - μ)² / n`

---

## Key Concepts

### Why C Extensions Are Faster
- **No interpreter overhead** — C loops execute directly as machine code
- **No boxing/unboxing per element** — all values are raw `double` in the hot loops
- **Cache-friendly** — contiguous `double[]` arrays fit in CPU cache efficiently
- **Compiler optimisations** — compiled with `-O2` flag

### CPython C API Used
| API Function | Purpose |
|---|---|
| `PyArg_ParseTuple` | Parse Python function arguments |
| `PySequence_Fast` | Convert any sequence to a fast-access form |
| `PyFloat_AsDouble` | Unbox Python float to C double |
| `PyLong_AsLong` | Fallback: unbox Python int to C long |
| `PyMem_Malloc / PyMem_Free` | CPython-aware memory allocation |
| `PyDict_New / PyDict_SetItemString` | Build the result dictionary |
| `PyFloat_FromDouble` | Box C double back to Python float |

### `sigaction` vs `signal` (Signal-Safe Writes)
> Not applicable here — note that in signal handlers, only async-signal-safe functions like `write()` should be used. `printf()` is not safe in signal context.

### Build System
`setup.py` uses `setuptools.Extension` to:
- Specify `statsmodule.c` as the C source
- Pass `-O2 -Wall` compile flags
- Link `libm` (`-lm`) for `sqrt()`