# SPDX-License-Identifier: BSD-2-Clause
#
# Property-based tests. Pure C, no engine state required. Each test
# runs many iterations of a generated input against an invariant.
# See TODO.complete/16-property-based-tests.md for the plan.

# All property tests link against parson directly (no engine state).
set(_parson_sources
	${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_property_parson
	test_property_parson.c
	${_parson_sources})
set_target_properties(retrace_property_parson PROPERTIES
	OUTPUT_NAME property_parson
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/property")

target_include_directories(retrace_property_parson PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/parson_stub
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/config/json)

# Standard C; no platform-specific defines needed since parson is
# self-contained.

add_test(NAME property-parson
	COMMAND retrace_property_parson)
set_tests_properties(property-parson PROPERTIES
	LABELS "property;unit"
	TIMEOUT 60)

#
# sockaddr_inspect property tests (TODO.complete/16 expansion).
#
# The helper lives in src/core/sockaddr_inspect.c. It uses
# retrace_real_impls for libc calls (set up in the test main), so
# we need the same arch-include + feature-macro setup as the unit
# tests for src/core.
#
if(RETRACE_PLATFORM_DARWIN)
	set(_property_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_macho/aarch64)
	set(_property_backend_lib retrace_backend_preload_macho)
elseif(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_property_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/aarch64)
		set(_property_backend_lib retrace_backend_preload_elf)
	else()
		set(_property_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
		set(_property_backend_lib retrace_backend_preload_elf)
	endif()
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_property_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
	set(_property_backend_lib retrace_backend_preload_bsd)
endif()

add_executable(retrace_property_sockaddr
	test_property_sockaddr.c)
set_target_properties(retrace_property_sockaddr PROPERTIES
	OUTPUT_NAME property_sockaddr
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/property")

target_link_libraries(retrace_property_sockaddr PRIVATE
	retrace_core
	retrace_config_json
	retrace_backends
	${_property_backend_lib}
	retrace_preload_common
	retrace_headers
	${CMAKE_DL_LIBS}
	Threads::Threads)

target_include_directories(retrace_property_sockaddr PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/core
	${CMAKE_SOURCE_DIR}/src/backends
	${CMAKE_SOURCE_DIR}/src/config/json
	${_property_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_property_sockaddr PRIVATE
		_GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_property_sockaddr PRIVATE
		_DARWIN_C_SOURCE)
endif()

add_test(NAME property-sockaddr
	COMMAND retrace_property_sockaddr)
set_tests_properties(property-sockaddr PROPERTIES
	LABELS "property;unit"
	TIMEOUT 60)

#
# Property tests for v2 actions (TODO.complete/16 P1). The actions
# are pure C functions registered by name in __retrace_acts; we look
# them up via retrace_actions_get and call with constructed
# ThreadContext + JSON params, mirroring test/unit/test_*.c.
#
if(RETRACE_PLATFORM_DARWIN)
	set(_action_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_macho/aarch64)
	set(_action_backend_lib retrace_backend_preload_macho)
elseif(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_action_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/aarch64)
		set(_action_backend_lib retrace_backend_preload_elf)
	else()
		set(_action_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
		set(_action_backend_lib retrace_backend_preload_elf)
	endif()
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_action_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
	set(_action_backend_lib retrace_backend_preload_bsd)
endif()

add_executable(retrace_property_actions test_property_actions.c)
set_target_properties(retrace_property_actions PROPERTIES
    OUTPUT_NAME property_actions
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/property")

target_link_libraries(retrace_property_actions PRIVATE
    retrace_core
    retrace_config_json
    retrace_backends
    ${_action_backend_lib}
    retrace_preload_common
    retrace_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

target_include_directories(retrace_property_actions PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/config/json
    ${_action_arch_include})

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_property_actions PRIVATE
		_GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_property_actions PRIVATE
		_DARWIN_C_SOURCE)
endif()

add_test(NAME property-actions
    COMMAND retrace_property_actions)
set_tests_properties(property-actions PROPERTIES
    LABELS "property;unit"
    TIMEOUT 60)
