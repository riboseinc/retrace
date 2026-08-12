# SPDX-License-Identifier: BSD-2-Clause
#
# retrace wasm playground (TODO.complete/36 P2).
#
# Pure C source + HTML. No CMake build (compiled separately
# via clang --target=wasm32). CMake installs the files so
# distro packagers can find them.

install(FILES playground.c index.html README.md
    DESTINATION share/retrace/wasm-playground)
