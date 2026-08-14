# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-audit: compliance audit report generator
# (TODO.complete/26 MVP). Reads a retrace JSON log, applies policy
# rules, emits findings as JSON. PDF rendering and the full policy
# schema land in follow-up PRs; this MVP proves the engine.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_audit audit.c policy.c scan.c pdf_writer.c ${_parson_source})
set_target_properties(retrace_audit PROPERTIES
    OUTPUT_NAME retrace-audit
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_audit PRIVATE
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_audit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
    ${CMAKE_SOURCE_DIR}/share/policies/baseline.json
    ${CMAKE_SOURCE_DIR}/share/policies/pci-dss.json
    ${CMAKE_SOURCE_DIR}/share/policies/hipaa.json
    ${CMAKE_SOURCE_DIR}/share/policies/iso27001.json
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/retrace/policies)
