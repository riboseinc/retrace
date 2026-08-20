# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-correlate: the escape-report tool (TODO.next-level/02).
# Joins an inside (VFS) stream against an outside (retrace) stream
# and reports host-filesystem touches the VFS never saw. Pure
# offline tool: three modules, no engine linkage.
#   match.c    path normalization + the escape decision
#   stream.c   tolerant log scanner (array doc / JSONL / truncated)
#   correlate.c CLI driver

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_correlate correlate.c match.c
    ${CMAKE_SOURCE_DIR}/tools/common/stream.c ${_parson_source})
set_target_properties(retrace_correlate PROPERTIES
    OUTPUT_NAME retrace-correlate
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_correlate PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/common
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_correlate
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Golden fixtures are the parity contract any escape correlator can
# assert against (tebako's Rust correlator consumes the same cases);
# ship them with binary artifacts too.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/tools/correlate/golden
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/retrace/correlate)

if(RETRACE_BUILD_TESTS AND NOT RETRACE_PLATFORM_WINDOWS)
    # golden_runner drives the CLI with POSIX wait helpers.
    add_executable(retrace_correlate_golden golden_runner.c)
    set_target_properties(retrace_correlate_golden PROPERTIES
        OUTPUT_NAME correlate-golden
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

    file(GLOB _correlate_cases LIST_DIRECTORIES true
        ${CMAKE_SOURCE_DIR}/tools/correlate/golden/*)

    foreach(case_dir ${_correlate_cases})
        if(IS_DIRECTORY ${case_dir})
            get_filename_component(case_name ${case_dir} NAME)
            add_test(NAME correlate-golden-${case_name}
                COMMAND retrace_correlate_golden ${case_dir}
                    $<TARGET_FILE:retrace_correlate>
                    ${CMAKE_CURRENT_BINARY_DIR}/golden-${case_name}.out)
            set_tests_properties(correlate-golden-${case_name}
                PROPERTIES LABELS "integration")
        endif()
    endforeach()
endif()
