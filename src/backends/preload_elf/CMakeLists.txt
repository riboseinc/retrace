# SPDX-License-Identifier: BSD-2-Clause
#
# preload-elf backend: Linux x86_64 (Phase 2 of TODO.v2 adds aarch64).

set(_preload_elf_x86_64_sources
	x86_64/arch_spec_top.S
	x86_64/arch_spec_bottom.c
	backend.c)

add_library(retrace_backend_preload_elf OBJECT ${_preload_elf_x86_64_sources})
target_include_directories(retrace_backend_preload_elf
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/x86_64
		${CMAKE_SOURCE_DIR}/src/backends)
target_link_libraries(retrace_backend_preload_elf
	PRIVATE
		retrace_preload_common
		retrace_headers)

if(RETRACE_PLATFORM_LINUX)
	target_compile_definitions(retrace_backend_preload_elf PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
endif()
