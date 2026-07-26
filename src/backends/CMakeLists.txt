# SPDX-License-Identifier: BSD-2-Clause
#
# retrace backend plugins. Each subdirectory owns one (OS, arch) pair.
# Adding a new backend is purely additive: new directory + new
# add_subdirectory call here. Engine code never changes.
#
# See TODO.v2/05-backend-abstraction.md and ADR-0003.

# Shared spawn helper for preload-* backends. Linked into each backend
# target PRIVATE so its symbols don't leak across backends.
add_library(retrace_preload_common OBJECT preload_common.c)
target_include_directories(retrace_preload_common
	PUBLIC
		${CMAKE_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/src/core)
target_link_libraries(retrace_preload_common PUBLIC retrace_headers)

# Backend registry. Linked into the retrace_v2 shared library so its
# exported API (retrace_backend_register/select/list/find) is visible to
# downstream library consumers.
add_library(retrace_backends OBJECT registry.c)
target_link_libraries(retrace_backends PUBLIC retrace_headers)
target_include_directories(retrace_backends
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# Per-backend OBJECT libraries, each owns its trampoline + bottom + backend.c.
# Phase 2 of TODO.v2 adds aarch64/ subdirs alongside x86_64/ in each backend.

if(RETRACE_PLATFORM_LINUX)
	add_subdirectory(preload_elf)
endif()

if(RETRACE_PLATFORM_DARWIN)
	add_subdirectory(preload_macho)
endif()

if(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	add_subdirectory(preload_bsd)
endif()
