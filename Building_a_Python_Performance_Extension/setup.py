
"""
setup.py — Build the stats C extension module.

Usage:
    python3 setup.py build_ext --inplace
"""

from setuptools import setup, Extension

stats_ext = Extension(
    name="stats",
    sources=["statsmodule.c"],
    extra_compile_args=["-O2", "-Wall"],
    libraries=["m"],          # link libm for sqrt()
)

setup(
    name="stats",
    version="1.0",
    description="High-performance C extension for statistical computation",
    ext_modules=[stats_ext],
)
