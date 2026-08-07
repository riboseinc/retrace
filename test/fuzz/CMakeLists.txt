# SPDX-License-Identifier: BSD-2-Clause
#
# libFuzzer harnesses (TODO.complete/33). Built only when
# RETRACE_BUILD_FUZZERS=ON, which adds -fsanitize=fuzzer,address.
# The resulting binaries are standalone fuzz targets (not ctest
# entries); run them manually or wire into a fuzz.yml workflow.

option(RETRACE_BUILD_FUZZERS "Build libFuzzer harnesses" OFF)

if(NOT RETRACE_BUILD_FUZZERS)
    return()
endif()

# Require a clang-family compiler -- libFuzzer is part of clang.
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING
        "RETRACE_BUILD_FUZZERS=ON requires clang (libFuzzer is "
        "clang-only); skipping fuzz targets.")
    return()
endif()

# Parson source -- the fuzzers link against it directly so they
# exercise parson in isolation (no engine state).
set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

# Common flags: libFuzzer + ASAN + the parson include path.
set(_fuzz_compile_flags -fsanitize=fuzzer,address -fno-omit-frame-pointer)
set(_fuzz_link_flags    -fsanitize=fuzzer,address)

function(add_retrace_fuzzer name source)
    add_executable(${name} ${source} ${_parson_source})
    set_target_properties(${name} PROPERTIES
        OUTPUT_NAME ${name}
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/fuzz")
    target_compile_options(${name} PRIVATE ${_fuzz_compile_flags})
    target_link_options(${name} PRIVATE ${_fuzz_link_flags})
    # parson.c #includes "real_impls.h". Use the parson property-test
    # stub (sprintf/malloc/free only) so parson links standalone.
    # Order matters: parson_stub/ before src/core/ so the stub
    # wins for real_impls.h.
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/test/property/parson_stub
        ${CMAKE_SOURCE_DIR}/src/config/json
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/backends
        ${CMAKE_BINARY_DIR})
endfunction()

add_retrace_fuzzer(fuzz_config_parse fuzz_config_parse.c)

# Manual test target -- runs the fuzzer for 5 seconds when invoked
# via `ctest -L fuzz`. Not in the default suite (too slow).
add_test(NAME fuzz-config-parse-smoke
    COMMAND ${CMAKE_BINARY_DIR}/test/fuzz/fuzz_config_parse
        -max_total_time=5
        ${CMAKE_CURRENT_SOURCE_DIR}/corpus/config_parse
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(fuzz-config-parse-smoke PROPERTIES
    LABELS "fuzz"
    TIMEOUT 30)
