# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-ebpf: eBPF backend for retrace (TODO.complete/29 MVP).
#
# BPF C source + Python loader. The BPF program compiles to bytecode
# (via clang) and is loaded by the Python script. No CMake-built
# binary here.

install(FILES retrace-ebpf.bpf.c retrace-ebpf-loader README.md
    DESTINATION share/retrace/ebpf-bridge)
