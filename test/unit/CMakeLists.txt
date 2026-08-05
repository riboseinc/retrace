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
