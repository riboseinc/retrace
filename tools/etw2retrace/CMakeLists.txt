# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-etw2retrace: raw ETW rows (etw-capture.ps1 jsonl) ->
# retrace trace JSON (TODO.trace-profile/24). The Windows
# kernel layer (truth); output feeds retrace-profile --kernel.
# Portable C.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_etw2retrace etw2retrace.c convert.c
	${_parson_source})
set_target_properties(retrace_etw2retrace PROPERTIES
    OUTPUT_NAME retrace-etw2retrace
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_etw2retrace PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_etw2retrace
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
