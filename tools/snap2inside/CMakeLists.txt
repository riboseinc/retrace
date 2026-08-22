# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-snap2inside: snapcraft.yaml plugs -> the inside.json
# declared-set shape (TODO.trace-profile/19). Packaging-layer
# claims-vs-truth. Portable C.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_snap2inside snap2inside.c ${_parson_source})
set_target_properties(retrace_snap2inside PROPERTIES
    OUTPUT_NAME retrace-snap2inside
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_snap2inside PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_snap2inside
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
