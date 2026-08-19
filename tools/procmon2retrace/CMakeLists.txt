# SPDX-License-Identifier: BSD-2-Clause
#
# procmon2retrace: procmon CSV -> retrace JSON producer
# (TODO.next-level/06). The Windows outer layer (ETW/procmon)
# normalizes to retrace's entry shape; retrace-correlate consumes
# the output like any other stream. Portable C -- once tools/
# builds on Windows this is the one tool Windows users want.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_procmon2retrace procmon2retrace.c convert.c
	csv.c ${_parson_source})
set_target_properties(retrace_procmon2retrace PROPERTIES
    OUTPUT_NAME retrace-procmon2retrace
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_procmon2retrace PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

install(TARGETS retrace_procmon2retrace
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(RETRACE_BUILD_TESTS)
    # Round-trip: sample CSV -> JSON -> retrace-correlate smoke
    # (TODO.next-level/06 acceptance). POSIX sh: tools/ is
    # POSIX-gated today.
    add_test(NAME procmon2retrace-roundtrip
        COMMAND sh -c
            "$<TARGET_FILE:retrace_procmon2retrace> \
                ${CMAKE_CURRENT_SOURCE_DIR}/samples/escape-sample.csv \
                ${CMAKE_CURRENT_BINARY_DIR}/rt-out.json && \
             $<TARGET_FILE:retrace_correlate> \
                --inside ${CMAKE_CURRENT_SOURCE_DIR}/samples/escape-inside.json \
                --outside ${CMAKE_CURRENT_BINARY_DIR}/rt-out.json \
                --prefix C:/pkg | \
             grep -qx 'escape C:/pkg/hidden.bin func=CreateFile tid=0'")
    set_tests_properties(procmon2retrace-roundtrip
        PROPERTIES LABELS "integration")

    # The libsass-importer golden case (correlate/golden/06):
    # regenerate its outside.json from the procmon CSV and prove
    # the two stay in sync, then run the correlation and diff the
    # expected escapes. Pins the real-world importer shapes end
    # to end (QueryOpen probes, \??\ prefixes, covered hits).
    set(_libsass_dir
        ${CMAKE_SOURCE_DIR}/tools/correlate/golden/06-libsass-importer)
    add_test(NAME procmon2retrace-libsass-golden
        COMMAND sh -c
            "$<TARGET_FILE:retrace_procmon2retrace> \
                ${_libsass_dir}/outside.csv \
                ${CMAKE_CURRENT_BINARY_DIR}/libsass-out.json && \
             diff -q ${CMAKE_CURRENT_BINARY_DIR}/libsass-out.json \
                ${_libsass_dir}/outside.json && \
             $<TARGET_FILE:retrace_correlate> \
                --inside ${_libsass_dir}/inside.json \
                --outside ${CMAKE_CURRENT_BINARY_DIR}/libsass-out.json \
                --prefix C:/pkg/scss > \
                ${CMAKE_CURRENT_BINARY_DIR}/libsass-corr.out; \\
             diff ${CMAKE_CURRENT_BINARY_DIR}/libsass-corr.out \\
                ${_libsass_dir}/expected.txt")
    # (Exit code 1 is asserted by the correlate golden runner,
    # which picks this case directory up automatically.)
    set_tests_properties(procmon2retrace-libsass-golden
        PROPERTIES LABELS "integration")
endif()
