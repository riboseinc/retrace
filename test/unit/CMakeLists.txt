# SPDX-License-Identifier: BSD-2-Clause
#
# Unit tests for retrace_core registries. Link directly against the
# retrace_core OBJECT library -- no LD_PRELOAD needed. Issue #481.

# The arch-specific include is needed because data_types.h includes
# arch_spec_macros.h, which differs per platform. Compute the path
# here since _retrace_arch_include from src/core/CMakeLists.txt is
# not in scope.
if(RETRACE_PLATFORM_DARWIN)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_macho/aarch64)
		set(_unit_backend_lib retrace_backend_preload_macho)
	else()
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_macho/x86_64)
		set(_unit_backend_lib retrace_backend_preload_macho)
	endif()
elseif(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/aarch64)
		set(_unit_backend_lib retrace_backend_preload_elf)
	else()
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
		set(_unit_backend_lib retrace_backend_preload_elf)
	endif()
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_unit_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
	set(_unit_backend_lib retrace_backend_preload_bsd)
endif()

add_executable(retrace_test_registries test_registries.c)
set_target_properties(retrace_test_registries PROPERTIES
    OUTPUT_NAME test_registries
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_registries PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_registries PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

# Same feature-test macros as retrace_core
if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_registries PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_registries PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-registries
    COMMAND retrace_test_registries)
set_tests_properties(unit-test-registries PROPERTIES LABELS "unit")

#
# parse_printf_format shim tests. The shim itself is a static inline
# in src/core/printf_compat.h; the test renames it via macro so it
# can be tested even on hosts that have a system parse_printf_format.
#
add_executable(retrace_test_printf_format test_printf_format.c)
set_target_properties(retrace_test_printf_format PROPERTIES
    OUTPUT_NAME test_printf_format
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_include_directories(retrace_test_printf_format PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core)

add_test(NAME unit-test-printf-format
    COMMAND retrace_test_printf_format)
set_tests_properties(unit-test-printf-format PROPERTIES LABELS "unit")

#
# script_resolver tests (ADR-0013 extraction). Verifies the JSON
# intercept_script lookup semantics against a constructed config.
#
add_executable(retrace_test_script_resolver test_script_resolver.c)
set_target_properties(retrace_test_script_resolver PROPERTIES
    OUTPUT_NAME test_script_resolver
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_script_resolver PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_script_resolver PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_script_resolver PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_script_resolver PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-script-resolver
    COMMAND retrace_test_script_resolver)
set_tests_properties(unit-test-script-resolver PROPERTIES LABELS "unit")

#
# sockaddr_inspect tests (TODO.complete/15). The helper is a pure C
# function -- no engine, no LD_PRELOAD. Minimal real_impls setup in
# the test main().
#
add_executable(retrace_test_sockaddr_inspect test_sockaddr_inspect.c)
set_target_properties(retrace_test_sockaddr_inspect PROPERTIES
    OUTPUT_NAME test_sockaddr_inspect
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_sockaddr_inspect PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_sockaddr_inspect PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_sockaddr_inspect PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_sockaddr_inspect PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-sockaddr-inspect
    COMMAND retrace_test_sockaddr_inspect)
set_tests_properties(unit-test-sockaddr-inspect PROPERTIES LABELS "unit")

#
# addr_deny action unit tests (TODO.complete/15). The action is
# looked up by name via retrace_actions_get(); ThreadContext is
# constructed inline. No engine, no LD_PRELOAD.
#
add_executable(retrace_test_addr_deny test_addr_deny.c)
set_target_properties(retrace_test_addr_deny PROPERTIES
    OUTPUT_NAME test_addr_deny
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_addr_deny PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_addr_deny PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_addr_deny PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_addr_deny PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-addr-deny
    COMMAND retrace_test_addr_deny)
set_tests_properties(unit-test-addr-deny PROPERTIES LABELS "unit")

#
# modify_return_value_int action unit tests (TODO.complete/14).
#
add_executable(retrace_test_modify_return_value_int
    test_modify_return_value_int.c)
set_target_properties(retrace_test_modify_return_value_int PROPERTIES
    OUTPUT_NAME test_modify_return_value_int
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_modify_return_value_int PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_modify_return_value_int PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_modify_return_value_int PRIVATE
        _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_modify_return_value_int PRIVATE
        _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-modify-return-value-int
    COMMAND retrace_test_modify_return_value_int)
set_tests_properties(unit-test-modify-return-value-int PROPERTIES
    LABELS "unit")

#
# call_count_limit action unit tests (TODO.complete/14).
#
add_executable(retrace_test_call_count_limit test_call_count_limit.c)
set_target_properties(retrace_test_call_count_limit PROPERTIES
    OUTPUT_NAME test_call_count_limit
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_call_count_limit PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_call_count_limit PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_call_count_limit PRIVATE
        _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_call_count_limit PRIVATE
        _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-call-count-limit
    COMMAND retrace_test_call_count_limit)
set_tests_properties(unit-test-call-count-limit PROPERTIES
    LABELS "unit")

#
# sandbox action unit tests (TODO.complete/14).
#
add_executable(retrace_test_sandbox test_sandbox.c)
set_target_properties(retrace_test_sandbox PROPERTIES
    OUTPUT_NAME test_sandbox
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_sandbox PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_sandbox PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_sandbox PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_sandbox PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-sandbox
    COMMAND retrace_test_sandbox)
set_tests_properties(unit-test-sandbox PROPERTIES LABELS "unit")

#
# modify_in_param_int action unit tests (TODO.complete/14).
#
add_executable(retrace_test_modify_in_param_int test_modify_in_param_int.c)
set_target_properties(retrace_test_modify_in_param_int PROPERTIES
    OUTPUT_NAME test_modify_in_param_int
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_modify_in_param_int PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_modify_in_param_int PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_modify_in_param_int PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_modify_in_param_int PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-modify-in-param-int
    COMMAND retrace_test_modify_in_param_int)
set_tests_properties(unit-test-modify-in-param-int PROPERTIES LABELS "unit")

#
# fuzzing_seed action unit tests (TODO.complete/14).
#
add_executable(retrace_test_fuzzing_seed test_fuzzing_seed.c)
set_target_properties(retrace_test_fuzzing_seed PROPERTIES
    OUTPUT_NAME test_fuzzing_seed
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_fuzzing_seed PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_fuzzing_seed PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_fuzzing_seed PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_fuzzing_seed PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-fuzzing-seed
    COMMAND retrace_test_fuzzing_seed)
set_tests_properties(unit-test-fuzzing-seed PROPERTIES LABELS "unit")

#
# delay action unit tests (TODO.complete/14).
#
add_executable(retrace_test_delay test_delay.c)
set_target_properties(retrace_test_delay PROPERTIES
    OUTPUT_NAME test_delay
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_delay PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_delay PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_delay PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_delay PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-delay
    COMMAND retrace_test_delay)
set_tests_properties(unit-test-delay PROPERTIES LABELS "unit")

#
# log_params action unit tests (TODO.complete/14).
#
add_executable(retrace_test_log_params test_log_params.c)
set_target_properties(retrace_test_log_params PROPERTIES
    OUTPUT_NAME test_log_params
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_log_params PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_log_params PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_log_params PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_log_params PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-log-params
    COMMAND retrace_test_log_params)
set_tests_properties(unit-test-log-params PROPERTIES LABELS "unit")

#
# call_real action unit tests (TODO.complete/14).
#
add_executable(retrace_test_call_real test_call_real.c)
set_target_properties(retrace_test_call_real PROPERTIES
    OUTPUT_NAME test_call_real
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_call_real PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_call_real PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_call_real PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_call_real PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-call-real
    COMMAND retrace_test_call_real)
set_tests_properties(unit-test-call-real PROPERTIES LABELS "unit")

#
# incomplete_io action unit tests (TODO.complete/14).
#
add_executable(retrace_test_incomplete_io test_incomplete_io.c)
set_target_properties(retrace_test_incomplete_io PROPERTIES
    OUTPUT_NAME test_incomplete_io
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_incomplete_io PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_incomplete_io PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_incomplete_io PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_incomplete_io PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-incomplete-io
    COMMAND retrace_test_incomplete_io)
set_tests_properties(unit-test-incomplete-io PROPERTIES LABELS "unit")

#
# memory_fuzz action unit tests (TODO.complete/14).
#
add_executable(retrace_test_memory_fuzz test_memory_fuzz.c)
set_target_properties(retrace_test_memory_fuzz PROPERTIES
    OUTPUT_NAME test_memory_fuzz
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_memory_fuzz PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_memory_fuzz PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_memory_fuzz PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_memory_fuzz PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-memory-fuzz
    COMMAND retrace_test_memory_fuzz)
set_tests_properties(unit-test-memory-fuzz PROPERTIES LABELS "unit")

#
# modify_in_param_str action unit tests (TODO.complete/14).
#
add_executable(retrace_test_modify_in_param_str test_modify_in_param_str.c)
set_target_properties(retrace_test_modify_in_param_str PROPERTIES
    OUTPUT_NAME test_modify_in_param_str
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_modify_in_param_str PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_modify_in_param_str PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_modify_in_param_str PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_modify_in_param_str PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-modify-in-param-str
    COMMAND retrace_test_modify_in_param_str)
set_tests_properties(unit-test-modify-in-param-str PROPERTIES LABELS "unit")

#
# reentrance_guard unit tests (TODO.complete/13).
#
add_executable(retrace_test_reentrance_guard test_reentrance_guard.c)
set_target_properties(retrace_test_reentrance_guard PROPERTIES
    OUTPUT_NAME test_reentrance_guard
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_reentrance_guard PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_reentrance_guard PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_reentrance_guard PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_reentrance_guard PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-reentrance-guard
    COMMAND retrace_test_reentrance_guard)
set_tests_properties(unit-test-reentrance-guard PROPERTIES LABELS "unit")

#
# modify_in_param_arr action unit tests (TODO.complete/14).
# Completes the 14-action sweep.
#
add_executable(retrace_test_modify_in_param_arr test_modify_in_param_arr.c)
set_target_properties(retrace_test_modify_in_param_arr PROPERTIES
    OUTPUT_NAME test_modify_in_param_arr
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_modify_in_param_arr PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_modify_in_param_arr PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_modify_in_param_arr PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_modify_in_param_arr PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-modify-in-param-arr
    COMMAND retrace_test_modify_in_param_arr)
set_tests_properties(unit-test-modify-in-param-arr PROPERTIES LABELS "unit")

#
# JSON config loader unit tests (TODO.complete/14). conf.c lives
# in src/config/json/, so we add that to includes.
#
add_executable(retrace_test_conf test_conf.c)
set_target_properties(retrace_test_conf PROPERTIES
    OUTPUT_NAME test_conf
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_conf PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_conf PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_conf PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_conf PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-conf
    COMMAND retrace_test_conf)
set_tests_properties(unit-test-conf PROPERTIES LABELS "unit")

#
# caller_match unit tests (TODO.complete/17).
#
add_executable(retrace_test_caller_match test_caller_match.c)
set_target_properties(retrace_test_caller_match PROPERTIES
    OUTPUT_NAME test_caller_match
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_caller_match PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_caller_match PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_caller_match PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_caller_match PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-caller-match
    COMMAND retrace_test_caller_match)
set_tests_properties(unit-test-caller-match PROPERTIES LABELS "unit")

#
# caller_cache unit tests (TODO.complete/17 P1).
#
add_executable(retrace_test_caller_cache test_caller_cache.c)
set_target_properties(retrace_test_caller_cache PROPERTIES
    OUTPUT_NAME test_caller_cache
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_caller_cache PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_caller_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_caller_cache PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_caller_cache PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-caller-cache
    COMMAND retrace_test_caller_cache)
set_tests_properties(unit-test-caller-cache PROPERTIES LABELS "unit")

#
# filter action unit tests (TODO.complete/20).
#
add_executable(retrace_test_filter test_filter.c)
set_target_properties(retrace_test_filter PROPERTIES
    OUTPUT_NAME test_filter
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_filter PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_unit_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_test_filter PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_unit_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
    target_compile_definitions(retrace_test_filter PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
    target_compile_definitions(retrace_test_filter PRIVATE _DARWIN_C_SOURCE)
endif()

add_test(NAME unit-test-filter
    COMMAND retrace_test_filter)
set_tests_properties(unit-test-filter PROPERTIES LABELS "unit")
