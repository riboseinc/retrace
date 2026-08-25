# SPDX-License-Identifier: BSD-2-Clause
#
# libFuzzer harnesses (TODO.complete/33). Built only when
# RETRACE_BUILD_FUZZERS=ON, which adds -fsanitize=fuzzer,address.
# The resulting binaries are standalone fuzz targets (not ctest
# entries); run them manually or wire into a fuzz.yml workflow.
# otlp-c live-streaming link (TODO.trace-profile/31): the target
# only exists on POSIX builds (Windows stubs the module); empty
# elsewhere.
set(_otlp_lib)
if(TARGET otlp_c)
	list(APPEND _otlp_lib otlp_c)
endif()


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

# The script_resolver fuzzer needs retrace_core (for script_resolver
# + real_impls definition). Can't use the parson-only stub because
# the resolver calls into real_impls.strcmp at runtime.
if(RETRACE_PLATFORM_DARWIN)
    set(_fuzz_arch_include
        ${CMAKE_SOURCE_DIR}/src/backends/preload_macho/aarch64)
    set(_fuzz_backend_lib retrace_backend_preload_macho)
elseif(RETRACE_PLATFORM_LINUX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(_fuzz_arch_include
            ${CMAKE_SOURCE_DIR}/src/backends/preload_elf/aarch64)
        set(_fuzz_backend_lib retrace_backend_preload_elf)
    else()
        set(_fuzz_arch_include
            ${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
        set(_fuzz_backend_lib retrace_backend_preload_elf)
    endif()
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
    set(_fuzz_arch_include
        ${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
    set(_fuzz_backend_lib retrace_backend_preload_bsd)
endif()

add_executable(fuzz_script_resolve
    fuzz_script_resolve.c)
set_target_properties(fuzz_script_resolve PROPERTIES
    OUTPUT_NAME fuzz_script_resolve
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/fuzz")
target_compile_options(fuzz_script_resolve PRIVATE ${_fuzz_compile_flags})
target_link_options(fuzz_script_resolve PRIVATE ${_fuzz_link_flags})
target_link_libraries(fuzz_script_resolve PRIVATE
    retrace_core
    retrace_supervisor_agent
    retrace_supervisor_proto
    ${_otlp_lib}
    retrace_config_json
    retrace_backends
    ${_fuzz_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)
target_include_directories(fuzz_script_resolve PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_fuzz_arch_include}
    ${CMAKE_BINARY_DIR})

add_test(NAME fuzz-script-resolve-smoke
    COMMAND ${CMAKE_BINARY_DIR}/test/fuzz/fuzz_script_resolve
        -max_total_time=5
        ${CMAKE_CURRENT_SOURCE_DIR}/corpus/script_resolve
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(fuzz-script-resolve-smoke PROPERTIES
    LABELS "fuzz"
    TIMEOUT 30)

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

# fuzz_action_run -- links against retrace_core (like fuzz_script_resolve)
# because it calls retrace_actions_get + retrace_actions_init.
add_executable(fuzz_action_run fuzz_action_run.c)
set_target_properties(fuzz_action_run PROPERTIES
    OUTPUT_NAME fuzz_action_run
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/fuzz")
target_compile_options(fuzz_action_run PRIVATE ${_fuzz_compile_flags})
target_link_options(fuzz_action_run PRIVATE ${_fuzz_link_flags})
target_link_libraries(fuzz_action_run PRIVATE
    retrace_core
    retrace_supervisor_agent
    retrace_supervisor_proto
    ${_otlp_lib}
    retrace_config_json
    retrace_backends
    ${_fuzz_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)
target_include_directories(fuzz_action_run PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_fuzz_arch_include}
    ${CMAKE_BINARY_DIR})

add_test(NAME fuzz-action-run-smoke
    COMMAND ${CMAKE_BINARY_DIR}/test/fuzz/fuzz_action_run
        -max_total_time=5
        ${CMAKE_CURRENT_SOURCE_DIR}/corpus/action_run
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(fuzz-action-run-smoke PROPERTIES
    LABELS "fuzz"
    TIMEOUT 30)

#
# fuzz_call_hash: libFuzzer custom mutator using retrace's
# call_hash as coverage feedback (TODO.complete/24 P1).
# Links standalone (no engine dependency). The custom mutator
# reads retrace_call_hash_last via weak symbol.
#
add_retrace_fuzzer(fuzz_call_hash fuzz_call_hash.c)

add_test(NAME fuzz-call-hash-smoke
    COMMAND ${CMAKE_BINARY_DIR}/test/fuzz/fuzz_call_hash
        -max_total_time=5
        ${CMAKE_CURRENT_SOURCE_DIR}/corpus/call_hash
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(fuzz-call-hash-smoke PROPERTIES
    LABELS "fuzz"
    TIMEOUT 30)

# fuzz_cli_parse -- exercises the CLI config builders (trace/mock/fuzz/slow).
# Statically links the builders from src/cli/config_builder.c; no engine
# dependency. Catches buffer-overflow + JSON-injection regressions.
add_executable(fuzz_cli_parse fuzz_cli_parse.c
    ${CMAKE_SOURCE_DIR}/src/cli/config_builder.c)
set_target_properties(fuzz_cli_parse PROPERTIES
    OUTPUT_NAME fuzz_cli_parse
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/fuzz")
target_compile_options(fuzz_cli_parse PRIVATE ${_fuzz_compile_flags})
target_link_options(fuzz_cli_parse PRIVATE ${_fuzz_link_flags})
target_include_directories(fuzz_cli_parse PRIVATE
    ${CMAKE_SOURCE_DIR}/src/cli)

add_test(NAME fuzz-cli-parse-smoke
    COMMAND ${CMAKE_BINARY_DIR}/test/fuzz/fuzz_cli_parse
        -max_total_time=5
        ${CMAKE_CURRENT_SOURCE_DIR}/corpus/cli_parse
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(fuzz-cli-parse-smoke PROPERTIES
    LABELS "fuzz"
    TIMEOUT 30)
