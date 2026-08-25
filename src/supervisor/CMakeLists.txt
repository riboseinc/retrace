# SPDX-License-Identifier: BSD-2-Clause
#
# The retraced control-protocol module (TODO.supervisor/01).
# Owns: the wire frame codec + the frozen message table (the
# single source of truth the conformance suite derives from).
# Does NOT own: the daemon (tools/retraced), the in-process
# agent (src/core, plan 03), transport (plan 08/12).
#
# A plain static library: linked by the daemon, the agent (via
# core), retrace-ctl, and the tests. No engine dependency --
# protocol code must stay testable in isolation.

add_library(retrace_supervisor_proto STATIC protocol.c)
target_include_directories(retrace_supervisor_proto PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# The in-process agent (TODO.supervisor/03): links INTO the
# retrace library via core's main.c boot hook. Depends on the
# protocol lib verbatim (DRY) and core's public surfaces
# (logger, real_impls, reentrance_guard) resolved at final
# link. POSIX body + WIN32 stubs in one file.
add_library(retrace_supervisor_agent STATIC agent.c)
target_include_directories(retrace_supervisor_agent PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(retrace_supervisor_agent PUBLIC
	retrace_supervisor_proto)
# The per-arch include core computes (engine.h ->
# arch_spec_macros.h); mirrored here because the agent links
# core headers without core's CMake context.
if(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_agent_arch_dir aarch64)
	else()
		set(_agent_arch_dir x86_64)
	endif()
	set(_agent_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_elf/${_agent_arch_dir})
elseif(RETRACE_PLATFORM_DARWIN)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_agent_arch_dir aarch64)
	else()
		set(_agent_arch_dir x86_64)
	endif()
	set(_agent_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_macho/${_agent_arch_dir})
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_agent_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
elseif(RETRACE_PLATFORM_WINDOWS)
	set(_agent_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/win_common)
endif()

target_include_directories(retrace_supervisor_agent PRIVATE
	${CMAKE_SOURCE_DIR}/src/core
	${CMAKE_SOURCE_DIR}/src/config/json
	${CMAKE_SOURCE_DIR}/src/backends
	${_agent_arch_include})

# Same feature-test macros as core (mirror; the agent compiles
# against core headers without core's CMake context).
if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_supervisor_agent
		PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_supervisor_agent
		PRIVATE _DARWIN_C_SOURCE)
endif()
