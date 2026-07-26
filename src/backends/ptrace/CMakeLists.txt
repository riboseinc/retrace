# SPDX-License-Identifier: BSD-2-Clause
#
# ptrace backend: statically-linked Linux binaries. Built only on
# Linux (the only host with a ptrace syscall). Adding this backend is
# purely additive: new directory + add_subdirectory() in the parent,
# plus a target_link_libraries entry in src/v2/CMakeLists.txt.

if(NOT RETRACE_PLATFORM_LINUX)
	return()
endif()

set(_ptrace_sources
	backend.c
	trace_loop.c
	translate.c
	syscall_table.c)

add_library(retrace_backend_ptrace OBJECT ${_ptrace_sources})

# arch_spec_macros.h is included transitively via src/core/engine.h ->
# data_types.h. It lives in the per-platform preload backend
# (preload_elf/x86_64 on Linux).
set(_ptrace_arch_include ${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)

target_include_directories(retrace_backend_ptrace
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/src/backends
		${CMAKE_SOURCE_DIR}/src/core
		${_ptrace_arch_include})
target_link_libraries(retrace_backend_ptrace
	PRIVATE
		retrace_headers)

# _GNU_SOURCE for PTRACE_GETREGSET/SETREGSET via <sys/ptrace.h>; the
# Linux-specific regset structures live behind _GNU_SOURCE.
target_compile_definitions(retrace_backend_ptrace
	PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
