# SPDX-License-Identifier: BSD-2-Clause
#
# retrace core engine module. Owns: engine, prototype registry, action
# registry, data types, real-impl table, logger. Does NOT own config
# parsing (that's src/config/) or trampolines (those stay in src/v2/arch/
# until TODO.v2/05 extracts them to src/backends/).
#
# See TODO.v2/04-engine-refactor.md.

set(_core_sources
	engine.c        funcs.c         actions.c      data_types.c
	real_impls.c    logger.c        main.c)

set(_core_prototype_sources
	prototypes/locale.c prototypes/ctype.c  prototypes/signal.c
	prototypes/stdio.c  prototypes/stdlib.c prototypes/uio.c
	prototypes/unistd.c prototypes/dirent.c)

set(_core_action_sources
	actions/basic.c actions/memfuzz.c)

set(_core_datatype_sources
	datatypes/basic.c datatypes/uio.c datatypes/unistd.c)

# Core is a single CMake OBJECT library so the eventual retrace_v2 shared
# library (built by src/v2/) can pull these in without exporting them.
# Downstream library users (TODO.roadmap/05) will get a real library
# target; for now, this object library is linked into retrace_v2.
add_library(retrace_core OBJECT
	${_core_sources}
	${_core_prototype_sources}
	${_core_action_sources}
	${_core_datatype_sources})

# Pick the per-platform macros.h (defines retrace_as_define_var_in_sec
# — ELF vs Mach-O section syntax). TODO.roadmap/02 removes the data_types
# dependency on this by centralizing section declaration in src/backends/.
if(RETRACE_PLATFORM_LINUX)
	set(_retrace_arch_include ${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
elseif(RETRACE_PLATFORM_DARWIN)
	set(_retrace_arch_include ${CMAKE_SOURCE_DIR}/src/backends/preload_macho/x86_64)
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_retrace_arch_include ${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
endif()

target_include_directories(retrace_core
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/backends>            # arch_spec.h
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/config/json>         # conf.h, parson.h (TODO.v2/04 decouples)
	PRIVATE
		${_retrace_arch_include}                                       # macros.h per-platform
		${CMAKE_CURRENT_SOURCE_DIR}/prototypes
		${CMAKE_CURRENT_SOURCE_DIR}/actions
		${CMAKE_CURRENT_SOURCE_DIR}/datatypes)

# Apply POSIX feature-test macros (mirror AC_USE_SYSTEM_EXTENSIONS).
if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_core PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	target_compile_definitions(retrace_core PRIVATE _GNU_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_core PRIVATE _DARWIN_C_SOURCE)
endif()

target_link_libraries(retrace_core
	PUBLIC Threads::Threads OpenSSL::SSL)
