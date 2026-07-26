# SPDX-License-Identifier: BSD-2-Clause
#
# preload-macho backend: macOS x86_64 (Phase 3 of TODO.v2 adds aarch64).

set(_preload_macho_x86_64_sources
	x86_64/arch_spec_top.S
	x86_64/arch_spec_bottom.c
	backend.c)

add_library(retrace_backend_preload_macho OBJECT ${_preload_macho_x86_64_sources})
target_include_directories(retrace_backend_preload_macho
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/x86_64
		${CMAKE_SOURCE_DIR}/src/backends
		${CMAKE_SOURCE_DIR}/src/core
		${CMAKE_SOURCE_DIR}/src/config/json)
target_link_libraries(retrace_backend_preload_macho
	PRIVATE
		retrace_preload_common
		retrace_headers)

if(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_backend_preload_macho PRIVATE _DARWIN_C_SOURCE)
endif()
