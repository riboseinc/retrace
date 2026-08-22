# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-flatpak2inside: flatpak manifest finish-args -> the inside.json
# declared-set shape (TODO.trace-profile/19). Packaging-layer
# claims-vs-truth. Portable C.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_flatpak2inside flatpak2inside.c ${_parson_source})
set_target_properties(retrace_flatpak2inside PROPERTIES
    OUTPUT_NAME retrace-flatpak2inside
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_flatpak2inside PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_flatpak2inside
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
