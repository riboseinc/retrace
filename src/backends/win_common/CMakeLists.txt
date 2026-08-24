# SPDX-License-Identifier: BSD-2-Clause
#
# Windows inline-hooking core. Shared between preload-msvc and preload-mingw
# backends. Built as an OBJECT library so each backend target can link its
# own copy without leaking symbols (MECE per ADR-0003).

set(_win_common_sources
	hook.c
	inject.c
	trampoline_allocator.c
	disasm_x64.c
	disasm_arm64.c
	veh_diag.c)

# x64 wrappers (TODO.windows/05): gas dialect for MinGW, MASM for
# MSVC -- identical semantics.
# arm64 wrappers (TODO.trace-profile/05, 08): gas dialect
# (MinGW / Clang arm64), armasm64 for MSVC-arm64.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|amd64")
	if(MSVC)
		list(APPEND _win_common_sources wrapper_x64.asm)
	else()
		list(APPEND _win_common_sources wrapper_x64.S)
	endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
	if(MSVC)
		list(APPEND _win_common_sources wrapper_arm64.asm)
	else()
		list(APPEND _win_common_sources wrapper_arm64.S)
	endif()
endif()

add_library(retrace_win_common OBJECT ${_win_common_sources})
target_include_directories(retrace_win_common
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/include)
