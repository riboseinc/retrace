# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-dtrace2retrace: dtrace/dtruss log -> retrace trace JSON
# (TODO.trace-profile/14). The macOS kernel layer (truth) needs
# SIP off; output feeds retrace-profile --kernel. Portable C.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_dtrace2retrace dtrace2retrace.c convert.c
	${CMAKE_SOURCE_DIR}/tools/common/converter.c
	${_parson_source})
set_target_properties(retrace_dtrace2retrace PROPERTIES
    OUTPUT_NAME retrace-dtrace2retrace
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_dtrace2retrace PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tools/common
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_dtrace2retrace
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
