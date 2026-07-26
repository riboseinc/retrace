# SPDX-License-Identifier: BSD-2-Clause
#
# Public header install rules.

set(RETRACE_PUBLIC_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/retrace.h
	${CMAKE_CURRENT_SOURCE_DIR}/version.h)

add_library(retrace_headers INTERFACE)
add_library(retrace::headers ALIAS retrace_headers)

target_include_directories(retrace_headers
	INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(FILES ${RETRACE_PUBLIC_HEADERS}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/retrace)
