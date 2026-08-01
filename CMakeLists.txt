# SPDX-License-Identifier: BSD-2-Clause
#
# retrace top-level CMake build.
#
# v1 source was removed in Phase 9 (ADR-0011); CMake is now the only build
# system. Autotools tree (configure.ac, Makefile.am, m4/, etc.) was deleted
# in the same phase.

cmake_minimum_required(VERSION 3.20)

# Read version from include/retrace/version.h so there is a single source of
# truth (ADR-0006).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/retrace/version.h" _version_h)
foreach(_part MAJOR MINOR PATCH)
	string(REGEX MATCH "#define RETRACE_VERSION_${_part} ([0-9]+)" _match "${_version_h}")
	set(RETRACE_VERSION_${_part} ${CMAKE_MATCH_1})
endforeach()

project(retrace
	VERSION "${RETRACE_VERSION_MAJOR}.${RETRACE_VERSION_MINOR}.${RETRACE_VERSION_PATCH}"
	LANGUAGES C ASM
	DESCRIPTION "Versatile security vulnerability / bug discovery tool for ELF and Mach-O binaries."
	HOMEPAGE_URL "https://github.com/riboseinc/retrace")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Enable GNU extensions (asm volatile, etc.) — v2's per-arch trampolines
# use GNU-style inline assembly.
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to hidden visibility; only RETRACE_API-tagged symbols are exported
# (ADR-0008).
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Default build type.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		Debug Release RelWithDebInfo MinSizeRel)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------
# Options
# ---------------------------------------------------------------

option(RETRACE_BUILD_V2            "Build retrace v2"                                    ON)
option(RETRACE_BUILD_CLI           "Build the retrace CLI"                               ON)
option(RETRACE_BUILD_TESTS         "Build unit and integration tests"                    OFF)
option(RETRACE_BUILD_DOCS          "Build Doxygen API reference"                         OFF)
option(RETRACE_BUILD_SHARED        "Build shared libraries"                              ON)
option(RETRACE_BUILD_STATIC        "Build static libraries"                              OFF)
option(RETRACE_ENABLE_RPC          "Enable RPC subsystem"                                OFF)
option(RETRACE_ENABLE_COVERAGE     "Enable code coverage instrumentation"               OFF)
option(RETRACE_ENABLE_ASAN         "Enable AddressSanitizer"                             OFF)
option(RETRACE_ENABLE_UBSAN        "Enable UndefinedBehaviorSanitizer"                   OFF)
option(RETRACE_ENABLE_TSAN         "Enable ThreadSanitizer"                              OFF)
option(RETRACE_BUILD_EXAMPLES      "Build the example programs"                          OFF)

# retrace is a LD_PRELOAD/DYLD_INSERT library — shared by default.
# BUILD_SHARED_LIBS propagates to every add_library() call without an
# explicit type.
if(RETRACE_BUILD_SHARED AND NOT RETRACE_BUILD_STATIC)
	set(BUILD_SHARED_LIBS ON)
elseif(NOT RETRACE_BUILD_SHARED AND RETRACE_BUILD_STATIC)
	set(BUILD_SHARED_LIBS OFF)
else()
	set(BUILD_SHARED_LIBS ON)
endif()

# ---------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(RETRACE_PLATFORM_LINUX    TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(RETRACE_PLATFORM_DARWIN   TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	set(RETRACE_PLATFORM_FREEBSD  TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	set(RETRACE_PLATFORM_OPENBSD  TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	set(RETRACE_PLATFORM_NETBSD   TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
	# Android — Bionic libc (not glibc/musl). LD_PRELOAD works via
	# wrap.sh (debug builds) or Magisk (rooted). preload_elf backend.
	set(RETRACE_PLATFORM_ANDROID TRUE)
	set(RETRACE_PLATFORM_LINUX   TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
	# OpenHarmony / HarmonyOS — musl-based aarch64 Linux.
	# LD_PRELOAD interposition works (preload_elf backend). See issue #443.
	set(RETRACE_PLATFORM_OHOS     TRUE)
	set(RETRACE_PLATFORM_LINUX    TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(RETRACE_PLATFORM_WINDOWS  TRUE)
endif()

# v2's assembly trampolines (src/backends/preload_*/{x86_64,aarch64}/...)
# are per-arch. Both x86_64 and aarch64 are supported on Linux, macOS, BSDs.
if(RETRACE_PLATFORM_DARWIN AND CMAKE_OSX_ARCHITECTURES)
	set(_retrace_proc_list "${CMAKE_OSX_ARCHITECTURES}")
else()
	set(_retrace_proc_list "${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(_retrace_proc_supported FALSE)
foreach(_proc ${_retrace_proc_list})
	if(_proc MATCHES "x86_64|amd64|AMD64|aarch64|arm64|ARM64")
		set(_retrace_proc_supported TRUE)
		break()
	endif()
endforeach()
if(RETRACE_BUILD_V2 AND NOT _retrace_proc_supported)
	message(STATUS "retrace v2 requires x86-64 or aarch64 (per-arch trampolines not yet ported); disabling.")
	set(RETRACE_BUILD_V2 OFF)
endif()

# v2 core (engine.c) is POSIX-only. On Windows, the inline-hooking backends
# (preload-msvc, preload-mingw) build standalone — they link win_common
# directly and inject via CreateRemoteThread. See ADR-0009.
if(RETRACE_PLATFORM_WINDOWS)
	if(RETRACE_BUILD_V2)
		message(STATUS "retrace v2 core (engine.c) requires POSIX APIs; disabling on Windows. The Windows inline-hooking backends build standalone.")
		set(RETRACE_BUILD_V2 OFF)
	endif()
endif()

# Feature-test macros — mirror AC_USE_SYSTEM_EXTENSIONS from the old
# configure.ac. Without _GNU_SOURCE on glibc, SO_REUSEPORT / alloca /
# getline / S_ISVTX are hidden. _DEFAULT_SOURCE covers BSDs.
if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	add_compile_definitions(_GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	add_compile_definitions(_GNU_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	add_compile_definitions(_DARWIN_C_SOURCE)
endif()

# OHOS defines __ohos__ in the toolchain; expose it as a CMake variable
# for use in source-level conditionals.
if(RETRACE_PLATFORM_OHOS)
	add_compile_definitions(RETRACE_PLATFORM_OHOS=1)
endif()

set(RETRACE_INSTALL_CMAKEDIR
	"${CMAKE_INSTALL_LIBDIR}/cmake/retrace-${PROJECT_VERSION}"
	CACHE PATH "Install path for CMake package config files")

# macOS minimum version
if(RETRACE_PLATFORM_DARWIN)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "macOS deployment target" FORCE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DARWIN_C_SOURCE")
endif()

# OpenBSD STAILQ_* -> SIMPLEQ_* aliasing
if(RETRACE_PLATFORM_OPENBSD)
	add_compile_definitions(
		STAILQ_INIT=SIMPLEQ_INIT
		STAILQ_ENTRY=SIMPLEQ_ENTRY
		STAILQ_HEAD=SIMPLEQ_HEAD
		STAILQ_INSERT_TAIL=SIMPLEQ_INSERT_TAIL
		STAILQ_FIRST=SIMPLEQ_FIRST
		STAILQ_NEXT=SIMPLEQ_NEXT
		STAILQ_FOREACH=SIMPLEQ_FOREACH)
endif()

# ---------------------------------------------------------------
# Feature probes — populate config.h
# ---------------------------------------------------------------

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckCSourceCompiles)

check_include_file(dlfcn.h    HAVE_DLFCN_H)
check_include_file(dirent.h   HAVE_DIRENT_H)
check_include_file(execinfo.h HAVE_EXECINFO_H)
check_include_file(printf.h   HAVE_PRINTF_H)
check_include_file(pthread.h  HAVE_PTHREAD_H)
check_include_file(pthread_np.h HAVE_PTHREAD_NP_H)
check_include_file(lwp.h      HAVE_LWP_H)
check_include_file(pwd.h      HAVE_PWD_H)
check_include_file(syscall.h  HAVE_SYSCALL_H)
check_include_file(unistd.h   HAVE_UNISTD_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/utsname.h HAVE_SYS_UTSNAME_H)

if(HAVE_EXECINFO_H)
	set(BACKTRACE ON)
endif()

# gcc __atomic builtins
check_c_source_compiles("
	int main(void) {
		int i;
		__atomic_load_n(&i, __ATOMIC_ACQUIRE);
		__atomic_store_n(&i, 1, __ATOMIC_RELEASE);
		return 0;
	}" HAVE_ATOMIC_BUILTINS)

# gettimeofday signature
check_c_source_compiles("
	#include <sys/time.h>
	int main(void) {
		struct timeval tv;
		int tz;
		gettimeofday(&tv, &tz);
		return 0;
	}" HAVE_GETTIMEOFDAY_WITHOUT_TIMEZONE)

# fcntl / socket / ptrace declaration probes
foreach(_decl
		O_TMPFILE
		PTRACE_GETREGS PTRACE_SETREGS
		PTRACE_GETFPREGS PTRACE_SETFPREGS
		PTRACE_GETFPXREGS PTRACE_SETFPXREGS
		PTRACE_GETREGSET PTRACE_SETREGSET
		PTRACE_SEIZE PTRACE_INTERRUPT PTRACE_LISTEN PTRACE_PEEKSIGINFO
		F_DUPFD_CLOEXEC F_SETOWN F_GETOWN F_SETLKW F_GETPATH
		F_PREALLOCATE F_PUNCHHOLE F_SETSIZE F_RDADVISE F_RDAHEAD
		F_READBOOTSTRAP F_WRITEBOOTSTRAP F_NOCACHE
		F_LOG2PHYS F_LOG2PHYS_EXT F_FULLFSYNC
		F_SETNOSIGPIPE F_GETNOSIGPIPE
		F_OFD_SETLK F_OFD_SETLKW F_OFD_GETLK
		F_GETOWN_EX F_SETOWN_EX F_GETSIG F_SETSIG
		F_SETLEASE F_GETLEASE F_NOTIFY
		F_SETPIPE_SZ F_GETPIPE_SZ F_ADD_SEALS F_GET_SEALS
		_IO_putc
		SOCK_PACKET
		SO_BINDTODEVICE SO_BSDCOMPAT SO_DOMAIN SO_MARK
		SO_PASSCRED SO_PEERCRED SO_PRIORITY SO_PROTOCOL
		SO_RXQ_OVFL SO_SNDBUFFORCE SO_RCVBUFFORCE SO_BUSY_POLL)
	check_symbol_exists(${_decl}
		"fcntl.h;sys/socket.h;sys/ptrace.h;stdio.h"
		HAVE_DECL_${_decl})
	if(NOT HAVE_DECL_${_decl})
		set(HAVE_DECL_${_decl} 0)
	endif()
endforeach()

# struct existence (fcntl feature structs).
foreach(_struct flock fstore fpunchhole radvisory fbootstraptransfer log2phys f_owner_ex)
	string(TOUPPER "${_struct}" _upper)
	check_c_source_compiles("
		#include <fcntl.h>
		int main(void) {
			struct ${_struct} s;
			(void)s;
			return 0;
		}" HAVE_STRUCT_${_upper})
endforeach()

if(Cmocka_FOUND)
	set(HAVE_CMOCKA_H ON)
	set(USE_CMOCKA ON)
endif()

# Build switches
if(RETRACE_BUILD_V2)
	set(RTR_V2 ON)
endif()
if(RETRACE_BUILD_CLI)
	set(RTR_V2_WRAPPER ON)
endif()
if(RETRACE_ENABLE_RPC)
	set(RTR_RPC ON)
endif()

# Generate config.h from the template
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake.in"
	"${CMAKE_BINARY_DIR}/config.h"
	ESCAPE_QUOTES)

# Detect glibc's __isoc99_* scanf variants. Modern gcc + glibc headers
# redirect scanf/fscanf/sscanf/v*scanf to __isoc99_* via __REDIRECT, so
# binaries built today call __isoc99_scanf (not scanf) at the PLT. We
# must interpose both names on glibc; musl and BSDs only need the plain
# names. The C-side prototypes are always compiled (the engine looks up
# by name and silently skips prototypes whose wrappers aren't present),
# but the .S wrappers must only be emitted when libc actually exports
# the symbol or load fails.
if(RETRACE_PLATFORM_LINUX AND NOT RETRACE_PLATFORM_OHOS)
	# check_symbol_exists looks for a declaration in headers. __isoc99_*
	# isn't declared publicly; we need a real link check.
	check_c_source_compiles("
		extern int __isoc99_scanf(const char *__restrict, ...);
		int main(void) { return __isoc99_scanf(0); }
	" HAVE_ISOC99_SCANF)
endif()
if(NOT HAVE_ISOC99_SCANF)
	set(HAVE_ISOC99_SCANF 0)
endif()
add_compile_definitions(RETRACE_HAVE_ISOC99_SCANF=${HAVE_ISOC99_SCANF})

# Make config.h discoverable by every target.
# Also add src/config/json (parson.h, conf.h) and src/backends (arch_spec.h)
# globally — OBJECT libraries don't always propagate target_include_directories
# to their own sources across all CMake versions.
add_compile_definitions(HAVE_CONFIG_H)
include_directories(BEFORE
	"${CMAKE_BINARY_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/config/json"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/backends")

# ---------------------------------------------------------------
# Sanitizers and coverage (see cmake/Sanitizers.cmake)
# ---------------------------------------------------------------

include(Sanitizers)

# ---------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------

find_package(Threads REQUIRED)
# OpenSSL was used by v1's ssl.c; that source was removed in Phase 9.
# The dependency survives as a PUBLIC link on retrace_core/retrace_v2 from
# before the cleanup. Make it OPTIONAL so platforms without OpenSSL (OHOS)
# can still build v2. Tracked for full removal in TODO.roadmap/02.
find_package(OpenSSL QUIET)

if(RETRACE_BUILD_TESTS)
	find_package(Cmocka QUIET)
endif()

# ---------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------

# Public headers are installed from include/.
add_subdirectory(include/retrace)

if(RETRACE_BUILD_V2)
	add_subdirectory(src/core)
	add_subdirectory(src/config)
	add_subdirectory(src/backends)
	add_subdirectory(src/v2)
elseif(RETRACE_PLATFORM_WINDOWS)
	# Windows: build the inline-hooking backends standalone (ADR-0009).
	add_subdirectory(src/backends)
endif()

# CLI launcher -- native C binary replacing the v1 shell script (#485).
# POSIX-only: Windows uses CreateRemoteThread + IAT hooking (ADR-0009),
# not LD_PRELOAD-style env var injection, so the launcher doesn't apply.
if(RETRACE_BUILD_CLI AND NOT RETRACE_PLATFORM_WINDOWS)
	add_subdirectory(src/cli)
endif()

if(RETRACE_BUILD_TESTS)
	if(RETRACE_BUILD_V2)
		enable_testing()
		add_subdirectory(test)
	else()
		message(STATUS "Tests requested but RETRACE_BUILD_V2=OFF; skipping.")
	endif()
endif()

if(RETRACE_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(RETRACE_ENABLE_RPC)
	add_subdirectory(rpc)
endif()

# tools/ uses POSIX-only headers (sys/wait.h etc.) -- skip on Windows.
if(NOT RETRACE_PLATFORM_WINDOWS)
	add_subdirectory(tools)
endif()

# ---------------------------------------------------------------
# Package config
# ---------------------------------------------------------------

include(CMakePackageConfigHelpers)

configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/retrace-config.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/retrace-config.cmake"
	INSTALL_DESTINATION "${RETRACE_INSTALL_CMAKEDIR}")

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/retrace-config-version.cmake"
	VERSION "${PROJECT_VERSION}"
	COMPATIBILITY SameMajorVersion)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/retrace-config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/retrace-config-version.cmake"
	DESTINATION "${RETRACE_INSTALL_CMAKEDIR}")

# pkg-config module
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/retrace-core.pc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/retrace-core.pc"
	@ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/retrace-core.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
