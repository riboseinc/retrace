# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-profile: the claims-vs-truth profiler (TODO.windows/08).
# Reduces libc-layer traces to a profile, grades them against a
# kernel-layer truth stream (sub-libc detection), statically scans
# the binary for raw-syscall capability, and emits a ready-to-run
# sandbox jail. Pure offline tool: no engine linkage.
#   profile.c      CLI driver + delta + jail emitter
#   aggregate.c    trace -> Profile reduction (functions/paths/env/net)
#   capability.c   ELF/Mach-O/PE static capability scan
#   match.c        path normalizer shared with retrace-correlate
#   stream.c       tolerant log scanner (array doc / JSONL / truncated)

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_profile
    profile.c aggregate.c capability.c
    ${CMAKE_SOURCE_DIR}/tools/correlate/match.c
    ${CMAKE_SOURCE_DIR}/tools/common/stream.c
    ${_parson_source})
set_target_properties(retrace_profile PROPERTIES
    OUTPUT_NAME retrace-profile
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_profile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tools/correlate
    ${CMAKE_SOURCE_DIR}/tools/common
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_profile
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
