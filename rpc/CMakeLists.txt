# SPDX-License-Identifier: BSD-2-Clause
#
# retrace RPC subsystem (was: enabled by --enable-rpc in configure.ac).
# See TODO.roadmap/01.

# Status: scaffold. The rpc/ subsystem is opt-in (RETRACE_ENABLE_RPC) and
# its build under CMake is not yet wired up. It depends on Mustache
# templating (functions.{c,h}.template generation) which the existing
# Autotools build handles via `mustache` gem.
#
# TODO.roadmap/01: add mustache generation, list rpc/*.c sources here.
