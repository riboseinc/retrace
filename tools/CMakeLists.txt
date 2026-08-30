# SPDX-License-Identifier: BSD-2-Clause
#
# retrace tools subdirectories. See the modernization plan/01.

add_subdirectory(spawn)
add_subdirectory(stringinjector)
add_subdirectory(otlp-converter)
add_subdirectory(audit-converter)
add_subdirectory(trace-diff)
add_subdirectory(ws-stream)
add_subdirectory(replay)
add_subdirectory(correlate)
add_subdirectory(procmon2retrace)
add_subdirectory(strace2retrace)
add_subdirectory(dtrace2retrace)
add_subdirectory(truss2retrace)
add_subdirectory(snap2inside)
add_subdirectory(flatpak2inside)
add_subdirectory(enforce)
add_subdirectory(ktrace2retrace)
add_subdirectory(etw2retrace)
add_subdirectory(fuzz-report)

# retraced and retrace-ctl run everywhere since plan 12 (the
# named-pipe transports on Windows).
add_subdirectory(retraced)
add_subdirectory(retrace-ctl)
add_subdirectory(profiler)
