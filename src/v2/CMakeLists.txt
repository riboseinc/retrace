# SPDX-License-Identifier: BSD-2-Clause
#
# retrace v2 shared library. After TODO.v2/04 (engine refactor), this
# is a thin shim that pulls in:
#   - retrace_core (object library): engine, registries, prototypes, actions
#   - retrace_config_json (object library): JSON config source
#   - per-arch trampoline assembly + bottom (TODO.v2/05 moves to src/backends/)
#
# See TODO.v2/04-engine-refactor.md, TODO.v2/05-backend-abstraction.md.

# Per-arch / per-OS trampoline sources. Phase 5 moves these to
# src/backends/preload_{elf,macho,bsd}/.
set(_v2_arch_include "")
if(RETRACE_PLATFORM_LINUX)
	set(_v2_arch_sources
		arch/x86-64/linux/arch_spec_bottom.c
		arch/x86-64/linux/arch_spec_top.S)
	set(_v2_arch_include arch/x86-64/linux)
elseif(RETRACE_PLATFORM_DARWIN)
	set(_v2_arch_sources
		arch/x86-64/osx/arch_spec_bottom.c
		arch/x86-64/osx/arch_spec_top.S)
	set(_v2_arch_include arch/x86-64/osx)
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_v2_arch_sources
		arch/x86-64/bsd/arch_spec_bottom.c
		arch/x86-64/bsd/arch_spec_top.S)
	set(_v2_arch_include arch/x86-64/bsd)
endif()

# Trampoline-only source list — the per-function entries.
set(_v2_trampoline_sources
	funcs_symbols.S)

add_library(retrace_v2 SHARED
	${_v2_arch_sources}
	${_v2_trampoline_sources})
add_library(retrace::v2 ALIAS retrace_v2)

# Pull in the core engine + config source object libraries.
target_link_libraries(retrace_v2 PRIVATE retrace_core retrace_config_json)

target_include_directories(retrace_v2
	PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	PRIVATE ${_v2_arch_include})

set_target_properties(retrace_v2 PROPERTIES
	OUTPUT_NAME retrace_v2
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	C_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)

# POSIX feature-test macros for the trampoline bottom.
if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_v2 PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_v2 PRIVATE _DARWIN_C_SOURCE)
endif()

target_link_libraries(retrace_v2
	PUBLIC Threads::Threads OpenSSL::SSL ${CMAKE_DL_LIBS})

install(TARGETS retrace_v2
	EXPORT retraceTargets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
