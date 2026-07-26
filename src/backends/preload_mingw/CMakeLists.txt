# SPDX-License-Identifier: BSD-2-Clause
#
# preload-mingw backend: MSYS2/MinGW build of the Windows inline-hooking
# library. Shares dllmain.c with preload-msvc (no toolchain-specific code
# lives there).

set(_preload_mingw_sources
	backend.c
	${CMAKE_SOURCE_DIR}/src/backends/preload_msvc/dllmain.c)

add_library(retrace_backend_preload_mingw SHARED ${_preload_mingw_sources})

target_include_directories(retrace_backend_preload_mingw
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/src/backends
		${CMAKE_SOURCE_DIR}/src/backends/win_common
		${CMAKE_SOURCE_DIR}/include)

target_link_libraries(retrace_backend_preload_mingw
	PUBLIC
		retrace_win_common
		retrace_headers
		retrace_backends)
