# SPDX-License-Identifier: BSD-2-Clause
#
# retrace v2 build under CMake. See TODO.roadmap/01 and TODO.roadmap/02.
#
# Status: scaffold — see task list at the bottom.

set(_v2_common_sources
	actions.c       data_types.c   engine.c       funcs.c
	main.c           parson.c       real_impls.c   conf.c
	logger.c         funcs_symbols.S)

set(_v2_prototype_sources
	prototypes/locale.c prototypes/ctype.c  prototypes/signal.c
	prototypes/stdio.c  prototypes/stdlib.c prototypes/uio.c
	prototypes/unistd.c prototypes/dirent.c)

set(_v2_action_sources
	actions/basic.c actions/memfuzz.c)

set(_v2_datatype_sources
	datatypes/basic.c datatypes/uio.c datatypes/unistd.c)

set(_v2_sources
	${_v2_common_sources}
	${_v2_prototype_sources}
	${_v2_action_sources}
	${_v2_datatype_sources})

# Per-arch / per-OS sources (was: src/v2/Makefile.am conditionals).
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

add_library(retrace_v2 ${_v2_sources} ${_v2_arch_sources})
add_library(retrace::v2 ALIAS retrace_v2)

target_include_directories(retrace_v2
	PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	PRIVATE ${_v2_arch_include})

set_target_properties(retrace_v2 PROPERTIES
	OUTPUT_NAME retrace_v2
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	C_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)

target_link_libraries(retrace_v2 PUBLIC Threads::Threads OpenSSL::SSL)

# v2 wrapper (was: bin_PROGRAMS=retrace2, gated on libnereon).
# libnereon dependency is being removed in TODO.roadmap/06-cli-redesign.md;
# until then the wrapper is not built from CMake.
# if(RETRACE_BUILD_CLI)
#   add_executable(retrace2 retrace_v2.c retrace_v2.nos.c)
#   target_link_libraries(retrace2 PRIVATE retrace::v2 nereon::nereon)
# endif()

install(TARGETS retrace_v2
	EXPORT retraceTargets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# TODO.roadmap/02 tasks: refactor v2 internals to match the layered model.
# This file is a thin shim that builds the existing v2 code. As TODOs 02-04
# land, code from src/v2/ migrates into src/core/, src/backends/,
# src/config/, and this file shrinks to a compatibility wrapper.
