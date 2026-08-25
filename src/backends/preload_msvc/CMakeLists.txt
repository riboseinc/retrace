# SPDX-License-Identifier: BSD-2-Clause
#
# preload-msvc backend: native MSVC build of the Windows retrace
# library -- engine + hook core + backend in ONE injectable DLL
# (retrace.dll). On Windows this target IS the library; there is
# no separate retrace_v2 (no funcs_symbols.S interposition).

set(_preload_msvc_sources
	backend.c
	${CMAKE_SOURCE_DIR}/src/backends/preload_msvc/dllmain.c)

add_library(retrace_backend_preload_msvc SHARED ${_preload_msvc_sources})

set_target_properties(retrace_backend_preload_msvc PROPERTIES
	OUTPUT_NAME retrace
	PREFIX "")

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
		retrace_backends
		retrace_core
		retrace_supervisor_agent
		retrace_config_json)

install(TARGETS retrace_backend_preload_msvc
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# MSVC requires this to expose backend symbols to downstream consumers.
if(MSVC)
	set_target_properties(retrace_backend_preload_msvc PROPERTIES
		WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
