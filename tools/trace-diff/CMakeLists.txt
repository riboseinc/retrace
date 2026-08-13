# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-diff: differential trace analysis (TODO.complete/27 MVP).
# Reads two retrace JSON logs, normalizes them, diffs per-function
# call counts and total time, prints a structured report.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_diff diff.c normalize.c threshold.c ${_parson_source})
set_target_properties(retrace_diff PROPERTIES
    OUTPUT_NAME retrace-diff
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

# sqrt() from <math.h> needs -lm on Linux (part of libSystem on macOS).
target_link_libraries(retrace_diff PRIVATE m)

target_include_directories(retrace_diff PRIVATE
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_diff
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
