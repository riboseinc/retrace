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
	else()
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_macho/x86_64)
	endif()
elseif(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/aarch64)
	else()
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
	endif()
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_unit_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
endif()

add_executable(retrace_test_registries test_registries.c)
set_target_properties(retrace_test_registries PROPERTIES
    OUTPUT_NAME test_registries
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

target_link_libraries(retrace_test_registries PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    retrace_backend_preload_macho
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
