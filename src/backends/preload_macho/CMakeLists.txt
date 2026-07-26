# SPDX-License-Identifier: BSD-2-Clause
#
# preload-macho backend: macOS. Per-arch trampolines; Phase 3 of the v2 plan
# added aarch64 (Apple Silicon) alongside x86_64.

if(CMAKE_OSX_ARCHITECTURES)
	list(GET CMAKE_OSX_ARCHITECTURES 0 _retrace_darwin_arch_first)
	if(_retrace_darwin_arch_first MATCHES "aarch64|arm64|ARM64")
		set(_retrace_arch aarch64)
	elseif(_retrace_darwin_arch_first MATCHES "x86_64|amd64|AMD64")
		set(_retrace_arch x86_64)
	else()
		set(_retrace_arch x86_64)
		message(WARNING "preload-macho: unknown OSX arch '${_retrace_darwin_arch_first}', defaulting to x86_64")
	endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
	set(_retrace_arch aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
	set(_retrace_arch x86_64)
else()
	set(_retrace_arch x86_64)
	message(WARNING "preload-macho: unknown CPU '${CMAKE_SYSTEM_PROCESSOR}', defaulting to x86_64")
endif()

set(_preload_macho_${_retrace_arch}_sources
	${_retrace_arch}/arch_spec_top.S
	${_retrace_arch}/arch_spec_bottom.c
	backend.c)

add_library(retrace_backend_preload_macho OBJECT ${_preload_macho_${_retrace_arch}_sources})
target_include_directories(retrace_backend_preload_macho
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/${_retrace_arch}
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
