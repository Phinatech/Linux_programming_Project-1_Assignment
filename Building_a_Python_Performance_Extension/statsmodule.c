/*
 * statsmodule.c — CPython C extension for high-performance statistics.
 *
 * Exposes one Python-callable function:
 *   stats.compute(list_or_sequence) -> dict
 *
 * Computes mean, variance, stddev, min, max, and median.
 * All heavy loops run in C with no Python object overhead
 * per element after the initial unboxing pass.
 *
 * Build:
 *   python3 setup.py build_ext --inplace
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── comparison function for qsort ── */
static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/*
 * stats_compute(sequence) -> dict
 *
 * Accepts any Python sequence of numbers (list, tuple, etc.).
 * Unpacks all values to C doubles in one pass, then performs
 * all arithmetic entirely in C — no Python calls inside the
 * hot loops.
 */
static PyObject *
stats_compute(PyObject *self, PyObject *args)
{
    PyObject *seq;

    /* Parse the single sequence argument */
    if (!PyArg_ParseTuple(args, "O", &seq))
        return NULL;

    /* Convert to a fast sequence */
    PyObject *fast = PySequence_Fast(seq,
        "argument must be a sequence of numbers");
    if (!fast)
        return NULL;

    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    if (n == 0) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "sequence must not be empty");
        return NULL;
    }

    /* Allocate a C array of doubles */
    double *arr = (double *)PyMem_Malloc(n * sizeof(double));
    if (!arr) {
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }

    /* ── Pass 1: unbox Python objects → C doubles ── */
    PyObject **items = PySequence_Fast_ITEMS(fast);
    for (Py_ssize_t i = 0; i < n; i++) {
        double v = PyFloat_AsDouble(items[i]);
        if (v == -1.0 && PyErr_Occurred()) {
            /* Try int conversion as fallback */
            PyErr_Clear();
            v = (double)PyLong_AsLong(items[i]);
            if (PyErr_Occurred()) {
                PyMem_Free(arr);
                Py_DECREF(fast);
                PyErr_SetString(PyExc_TypeError,
                    "sequence must contain only numbers");
                return NULL;
            }
        }
        arr[i] = v;
    }
    Py_DECREF(fast);

    /* ── Pass 2: mean + min/max (single loop) ── */
    double sum = 0.0;
    double lo  = arr[0];
    double hi  = arr[0];
    for (Py_ssize_t i = 0; i < n; i++) {
        sum += arr[i];
        if (arr[i] < lo) lo = arr[i];
        if (arr[i] > hi) hi = arr[i];
    }
    double mean = sum / (double)n;

    /* ── Pass 3: variance ── */
    double var_sum = 0.0;
    for (Py_ssize_t i = 0; i < n; i++) {
        double d = arr[i] - mean;
        var_sum += d * d;
    }
    double variance = var_sum / (double)n;
    double stddev   = sqrt(variance);

    /* ── Pass 4: median via in-place sort of a copy ── */
    double *sorted = (double *)PyMem_Malloc(n * sizeof(double));
    if (!sorted) {
        PyMem_Free(arr);
        return PyErr_NoMemory();
    }
    memcpy(sorted, arr, n * sizeof(double));
    qsort(sorted, (size_t)n, sizeof(double), cmp_double);

    double median;
    if (n % 2 == 0)
        median = (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
    else
        median = sorted[n/2];

    PyMem_Free(arr);
    PyMem_Free(sorted);

    /* ── Build result dict ── */
    PyObject *result = PyDict_New();
    if (!result) return NULL;

    /* Helper macro: insert a key/float pair, bail on failure */
#define SET_FLOAT(key, val)                              \
    do {                                                 \
        PyObject *pv = PyFloat_FromDouble(val);          \
        if (!pv) { Py_DECREF(result); return NULL; }    \
        if (PyDict_SetItemString(result, key, pv) < 0) {\
            Py_DECREF(pv);                               \
            Py_DECREF(result);                           \
            return NULL;                                 \
        }                                                \
        Py_DECREF(pv);                                   \
    } while (0)

    SET_FLOAT("mean",   mean);
    SET_FLOAT("var",    variance);
    SET_FLOAT("stddev", stddev);
    SET_FLOAT("min",    lo);
    SET_FLOAT("max",    hi);
    SET_FLOAT("median", median);

#undef SET_FLOAT

    return result;
}

/* ── Module method table ── */
static PyMethodDef StatsMethods[] = {
    {
        "compute",
        stats_compute,
        METH_VARARGS,
        "compute(sequence) -> dict\n\n"
        "Compute mean, variance, stddev, min, max, and median\n"
        "of a sequence of numbers using an optimised C implementation.\n"
        "Significantly faster than pure Python for large arrays."
    },
    { NULL, NULL, 0, NULL }   /* sentinel */
};

/* ── Module definition ── */
static struct PyModuleDef statsmodule = {
    PyModuleDef_HEAD_INIT,
    "stats",          /* module name */
    "High-performance statistics C extension for CPython.\n"
    "Implements mean, variance, stddev, min, max, and median\n"
    "using native C loops with no per-element Python overhead.",
    -1,               /* per-interpreter state size (-1 = global) */
    StatsMethods
};

/* ── Module initialiser — called by Python import machinery ── */
PyMODINIT_FUNC
PyInit_stats(void)
{
    return PyModule_Create(&statsmodule);
}
