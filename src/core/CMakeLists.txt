# SPDX-License-Identifier: BSD-2-Clause
#
# retrace core engine module. Owns: engine, prototype registry, action
# registry, data types, real-impl table, logger. Does NOT own config
# parsing (that's src/config/) or trampolines (those stay in src/v2/arch/
# until the v2 plan/05 extracts them to src/backends/).
#
#

set(_core_sources
	engine.c        funcs.c         actions.c      data_types.c
	real_impls.c    logger.c        main.c         as_call_real.c
	thread_context.c reentrance_guard.c cleanup.c
	script_resolver.c action_runner.c
	sockaddr_inspect.c)

# dlopen_guard.c is Linux-only: it exports dlopen/dlclose/dlsym/dlerror
# as global symbols for LD_PRELOAD interposition. On macOS these symbols
# cause dyld init issues. On macOS the guard functions are still declared
# (in engine.h) but always return 0 (no-op).
if(RETRACE_PLATFORM_LINUX)
	list(APPEND _core_sources dlopen_guard.c)
endif()

set(_core_prototype_sources
	prototypes/locale.c prototypes/ctype.c  prototypes/signal.c
	prototypes/stdio.c  prototypes/stdlib.c prototypes/uio.c
	prototypes/unistd.c prototypes/dirent.c prototypes/sys_socket.c
	prototypes/string.c
	prototypes/pthread.c
	prototypes/time.c)

set(_core_action_sources
	actions/basic.c actions/memfuzz.c
	actions/incomplete_io.c actions/fuzzing_seed.c
	actions/delay.c actions/call_count_limit.c
	actions/sandbox.c actions/addr_deny.c)

set(_core_datatype_sources
	datatypes/basic.c datatypes/uio.c datatypes/unistd.c)

# Core is a single CMake OBJECT library so the eventual retrace_v2 shared
# library (built by src/v2/) can pull these in without exporting them.
# Downstream library users (the modernization plan/05) will get a real library
# target; for now, this object library is linked into retrace_v2.
add_library(retrace_core OBJECT
	${_core_sources}
	${_core_prototype_sources}
	${_core_action_sources}
	${_core_datatype_sources})

# Pick the per-platform macros.h (defines retrace_as_define_var_in_sec
# — ELF vs Mach-O section syntax). the modernization plan/02 removes the data_types
# dependency on this by centralizing section declaration in src/backends/.
if(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_retrace_arch_dir aarch64)
	else()
		set(_retrace_arch_dir x86_64)
	endif()
	set(_retrace_arch_include ${CMAKE_SOURCE_DIR}/src/backends/preload_elf/${_retrace_arch_dir})
elseif(RETRACE_PLATFORM_DARWIN)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_retrace_arch_dir aarch64)
	else()
		set(_retrace_arch_dir x86_64)
	endif()
	set(_retrace_arch_include ${CMAKE_SOURCE_DIR}/src/backends/preload_macho/${_retrace_arch_dir})
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_retrace_arch_include ${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
endif()

target_include_directories(retrace_core
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/backends>            # arch_spec.h
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/config/json>         # conf.h, parson.h (the v2 plan/04 decouples)
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

set(_retrace_core_libs PUBLIC Threads::Threads)
if(OpenSSL_FOUND)
	list(APPEND _retrace_core_libs OpenSSL::SSL)
endif()

target_link_libraries(retrace_core ${_retrace_core_libs})
