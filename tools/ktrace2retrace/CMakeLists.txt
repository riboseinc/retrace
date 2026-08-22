# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-ktrace2retrace: kdump (OpenBSD/NetBSD) log -> retrace
# trace JSON (TODO.trace-profile/21). The BSD kernel layer
# (truth); output feeds retrace-profile --kernel. Portable C.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_ktrace2retrace ktrace2retrace.c convert.c
	${_parson_source})
set_target_properties(retrace_ktrace2retrace PROPERTIES
    OUTPUT_NAME retrace-ktrace2retrace
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_ktrace2retrace PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_ktrace2retrace
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
