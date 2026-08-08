# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-to-otlp: converts retrace's JSON log to OTLP/JSON
# (TODO.complete/21 MVP). Standalone tool; links against parson
# for JSON parsing. No engine dependency.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_to_otlp otlp_converter.c ${_parson_source})
set_target_properties(retrace_to_otlp PROPERTIES
    OUTPUT_NAME retrace-to-otlp
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_to_otlp PRIVATE
    ${CMAKE_SOURCE_DIR}/src/config/json)

# parson.c includes real_impls.h. Provide the parson_stub so it
# links standalone (same pattern as test/property/parson_stub/).
target_include_directories(retrace_to_otlp PRIVATE
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)
