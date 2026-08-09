# SPDX-License-Identifier: BSD-2-Clause
"""retrace Python binding -- config builder + CLI wrapper."""

from setuptools import setup

setup(
    name="retrace",
    version="2.2.2",
    description="Python config builder + CLI wrapper for retrace v2",
    long_description="Generates JSON configs programmatically and "
    "invokes the retrace CLI. No C extension needed.",
    license="BSD-2-Clause",
    author="Ribose Inc",
    url="https://github.com/riboseinc/retrace",
    py_modules=["retrace"],
    python_requires=">=3.6",
    classifiers=[
        "License :: OSI Approved :: BSD License",
        "Programming Language :: Python :: 3",
        "Topic :: Security",
        "Topic :: Software Development :: Debuggers",
    ],
)
