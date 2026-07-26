# SPDX-License-Identifier: BSD-2-Clause
#
# preload-msvc backend: native MSVC build of the Windows inline-hooking
# library. Built as a SHARED library (DLL) so it can be injected into a
# target process via CreateRemoteThread(LoadLibrary).

set(_preload_msvc_sources
	backend.c
	${CMAKE_SOURCE_DIR}/src/backends/preload_msvc/dllmain.c)

add_library(retrace_backend_preload_msvc SHARED ${_preload_msvc_sources})

target_include_directories(retrace_backend_preload_msvc
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/src/backends
		${CMAKE_SOURCE_DIR}/src/backends/win_common
		${CMAKE_SOURCE_DIR}/include)

target_link_libraries(retrace_backend_preload_msvc
	PUBLIC
		retrace_win_common
		retrace_headers
		retrace_backends)

# MSVC requires this to expose backend symbols to downstream consumers.
if(MSVC)
	set_target_properties(retrace_backend_preload_msvc PROPERTIES
		WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
