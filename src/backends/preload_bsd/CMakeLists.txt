# SPDX-License-Identifier: BSD-2-Clause
#
# preload-bsd backend: FreeBSD/OpenBSD/NetBSD x86_64.

set(_preload_bsd_x86_64_sources
	x86_64/arch_spec_top.S
	x86_64/arch_spec_bottom.c
	backend.c)

add_library(retrace_backend_preload_bsd OBJECT ${_preload_bsd_x86_64_sources})
target_include_directories(retrace_backend_preload_bsd
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/x86_64
		${CMAKE_SOURCE_DIR}/src/backends)
target_link_libraries(retrace_backend_preload_bsd
	PRIVATE
		retrace_preload_common
		retrace_headers)

if(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	target_compile_definitions(retrace_backend_preload_bsd PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
endif()
