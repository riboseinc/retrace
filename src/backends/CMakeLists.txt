# SPDX-License-Identifier: BSD-2-Clause
#
# retrace backend plugins. Each subdirectory owns one (OS, arch) pair.
# Adding a new backend is purely additive: new directory + new
# add_subdirectory call here. Engine code never changes.
#
# and ADR-0003.

# Shared spawn helper for preload-* backends. Linked into each backend
# target PRIVATE so its symbols don't leak across backends.
# POSIX-only: uses fork/execv/waitpid. Windows backends use
# CreateProcess/CreateRemoteThread instead.
if(NOT RETRACE_PLATFORM_WINDOWS)
	add_library(retrace_preload_common OBJECT preload_common.c)
	target_include_directories(retrace_preload_common
		PUBLIC
			${CMAKE_SOURCE_DIR}/include
			${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_SOURCE_DIR}/src/core)
	target_link_libraries(retrace_preload_common PUBLIC retrace_headers)
endif()

# Backend registry. Linked into the retrace_v2 shared library so its
# exported API (retrace_backend_register/select/list/find) is visible to
# downstream library consumers.
# Windows (TODO.windows/04): the retrace_as_* trampoline contract
# links as no-op stubs until the first wrapper lands (item 05);
# ws2_32 covers sockaddr_inspect's ntohs on winsock.
if(RETRACE_PLATFORM_WINDOWS)
	add_library(retrace_backends OBJECT registry.c
		${CMAKE_CURRENT_SOURCE_DIR}/win_common/arch_spec_stub.c)
	set_target_properties(retrace_backends PROPERTIES
		INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/win_common")
	target_include_directories(retrace_backends PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/win_common
		${CMAKE_SOURCE_DIR}/src/core)
else()
	add_library(retrace_backends OBJECT registry.c)
endif()
target_link_libraries(retrace_backends PUBLIC retrace_headers)
if(RETRACE_PLATFORM_WINDOWS)
	target_link_libraries(retrace_backends INTERFACE ws2_32)
endif()
target_include_directories(retrace_backends
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# Per-backend OBJECT libraries, each owns its trampoline + bottom + backend.c.
# Phase 2 of the v2 plan adds aarch64/ subdirs alongside x86_64/ in each backend.

if(RETRACE_PLATFORM_LINUX)
	add_subdirectory(preload_elf)
	add_subdirectory(ptrace)
endif()

if(RETRACE_PLATFORM_DARWIN)
	add_subdirectory(preload_macho)
endif()

if(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	add_subdirectory(preload_bsd)
endif()

# Windows: from-scratch inline-hooking backends (ADR-0009). The hooking
# core (win_common/) is shared; preload-msvc is the native MSVC build and
# preload-mingw is the MSYS2/MinGW build. Only one is active per host.
if(RETRACE_PLATFORM_WINDOWS)
	add_subdirectory(win_common)
	if(MSVC OR CMAKE_C_COMPILER_ID STREQUAL "MSVC")
		add_subdirectory(preload_msvc)
	else()
		add_subdirectory(preload_mingw)
	endif()
endif()
