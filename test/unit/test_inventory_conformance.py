#!/usr/bin/env python3
"""
Function-inventory conformance (the architecture-review deepening).

Every backend's funcs_symbols.S must wrap AT LEAST the functions
in the shared funcs_symbols.def -- the v2.36.0 incident shipped a
linux-aarch64 inventory that had drifted to miss 28 functions
(the whole socket family + open/openat), silently no-op'ing
configs on that platform. This test makes that drift a red check
instead of a coverage hole.

For each backend inventory:
  - preprocess it exactly as the build would (same include dirs,
    same platform defines, RETRACE_HAVE_ISOC99_SCANF from the
    generated config.h)
  - extract the wrapped symbol set (any WRAPPER_ENTRY_* macro)
  - strip macOS $DARWIN_EXTSN suffixes (the engine strips them
    too)
  - assert: funcs_symbols.def's set is a SUBSET of it

Backend-specific extras (dlerror, pthread cleanup handlers,
isoc99 scanf variants, fopen$DARWIN_EXTSN) are allowed and
reported informationally -- they are additions, not drift.

Usage:
    test_inventory_conformance.py --cc <cc> --src <repo> --build <builddir>
"""
import argparse
import re
import subprocess
import sys

DEF_FILE = "src/v2/funcs_symbols.def"


def def_symbols(src, accept4):
    """The canonical set, honoring the .def's own platform gates
    (accept4 is RETRACE_HAVE_ACCEPT4-conditional)."""
    names = set()
    with open(f"{src}/{DEF_FILE}") as f:
        for ln in f:
            m = re.match(r"^RETRACE_WRAP\s+(\S+)", ln)
            if m:
                names.add(m.group(1))
    if not accept4:
        names.discard("accept4")
    return names


def isoc99_from_config(build):
    try:
        with open(f"{build}/config.h") as f:
            m = re.search(r"RETRACE_HAVE_ISOC99_SCANF\s+(\d)", f.read())
            return m.group(1) if m else "0"
    except OSError:
        return "0"


def backend_symbols(cc, src, rel, includes, defines):
    path = f"{src}/{rel}"
    cmd = [cc, "-E", "-x", "assembler-with-cpp"]
    for i in includes:
        cmd += ["-I", f"{src}/{i}"]
    for d in defines:
        cmd += [f"-D{d}"]
    cmd += [path]
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        print(f"FAIL: preprocess {rel}: {out.stderr[:300]}", file=sys.stderr)
        sys.exit(1)
    names = set()
    for m in re.finditer(r"WRAPPER_ENTRY_\w+\s+(\S+)", out.stdout):
        names.add(m.group(1).split("$DARWIN_EXTSN")[0])
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", required=True)
    ap.add_argument("--src", required=True)
    ap.add_argument("--build", required=True)
    ap.add_argument("--apple", action="store_true")
    args = ap.parse_args()

    isoc99 = isoc99_from_config(args.build)
    try:
        with open(f"{args.build}/config.h") as f:
            accept4 = re.search(
                r"RETRACE_HAVE_ACCEPT4\s+(\d)", f.read()).group(1) == "1"
    except (OSError, AttributeError):
        accept4 = False

    canon = def_symbols(args.src, accept4)
    if not canon:
        print("FAIL: empty funcs_symbols.def?", file=sys.stderr)
        return 1

    darwin = ["__MACH__"] if args.apple else []

    backends = [
        ("src/v2/funcs_symbols.S", ["src/v2"],
         darwin + [f"RETRACE_HAVE_ISOC99_SCANF={isoc99}"]),
        ("src/backends/preload_macho/aarch64/funcs_symbols.S",
         ["src/backends/preload_macho/aarch64", "src/v2"], darwin),
        ("src/backends/preload_elf/aarch64/funcs_symbols.S",
         ["src/backends/preload_elf/aarch64", "src/v2"], []),
    ]

    failures = 0
    for rel, inc, defs in backends:
        got = backend_symbols(args.cc, args.src, rel, inc, defs)
        missing = canon - got
        extras = got - canon
        tag = rel.split("/")[-2] + "/" + rel.split("/")[-1]
        if missing:
            failures += 1
            print(f"FAIL: {tag} is missing {len(missing)} def functions:",
                  file=sys.stderr)
            for n in sorted(missing):
                print(f"  - {n}", file=sys.stderr)
            print("(drift between funcs_symbols.def and this backend -- "
                  "configs silently no-op for these)",
                  file=sys.stderr)
        else:
            extra_s = ", ".join(sorted(extras)) if extras else "none"
            print(f"ok: {tag} covers all {len(canon)} def functions "
                  f"(extras: {extra_s})")

    if failures:
        return 1
    print(f"PASS: all backends conform to funcs_symbols.def "
          f"({len(canon)} functions)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
