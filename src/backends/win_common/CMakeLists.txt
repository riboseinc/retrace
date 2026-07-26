# SPDX-License-Identifier: BSD-2-Clause
#
# Windows inline-hooking core. Shared between preload-msvc and preload-mingw
# backends. Built as an OBJECT library so each backend target can link its
# own copy without leaking symbols (MECE per ADR-0003).

set(_win_common_sources
	hook.c
	trampoline_allocator.c
	disasm_x64.c
	disasm_arm64.c)

add_library(retrace_win_common OBJECT ${_win_common_sources})
target_include_directories(retrace_win_common
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/include)
