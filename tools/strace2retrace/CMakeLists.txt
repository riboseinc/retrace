# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-strace2retrace: strace log -> retrace trace JSON
# (TODO.windows/08). The Linux kernel layer (truth) normalizes to
# retrace's entry shape; retrace-profile --kernel and
# retrace-correlate --outside consume the output like any other
# stream. Portable C.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_strace2retrace strace2retrace.c convert.c
	${CMAKE_SOURCE_DIR}/tools/common/converter.c
	${_parson_source})
set_target_properties(retrace_strace2retrace PROPERTIES
    OUTPUT_NAME retrace-strace2retrace
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_strace2retrace PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tools/common
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_strace2retrace
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
