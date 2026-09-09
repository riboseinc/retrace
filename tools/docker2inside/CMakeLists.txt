# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-docker2inside: docker save tarball layers -> the inside.json
# declared-set shape (TODO.impl/02). Container-audience
# claims-vs-truth. Portable C.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_docker2inside docker2inside.c ${_parson_source})
set_target_properties(retrace_docker2inside PROPERTIES
    OUTPUT_NAME retrace-docker2inside
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_docker2inside PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_docker2inside
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
