# SPDX-License-Identifier: BSD-2-Clause
#
# retrace top-level CMake build. See TODO.roadmap/01-foundation-build-cmake.md
# for the migration plan from Autotools. Both build systems coexist during
# Phase 1-2; Autotools is removed in Phase 3.
#
# The CMake build is the canonical build for vcpkg / conan / system packagers.
# The Autotools build (`./configure && make`) continues to work for existing
# users during the transition.

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
# use GNU-style inline assembly; several v1 files also rely on __attribute__
# extensions. configure.ac effectively enabled these via -D_GNU_SOURCE.
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

option(RETRACE_BUILD_V1            "Build retrace v1 (deprecated; see TODO.roadmap/10)" ON)
option(RETRACE_BUILD_V2            "Build retrace v2 (the future)"                       ON)
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

# retrace is a LD_PRELOAD/DYLD_INSERT library — shared by default. The v1
# code uses _dl_sym (GLIBC_PRIVATE), which is unresolved at static-link
# time but resolved at runtime by ld.so. BUILD_SHARED_LIBS propagates to
# every add_library() call without an explicit type.
if(RETRACE_BUILD_SHARED AND NOT RETRACE_BUILD_STATIC)
	set(BUILD_SHARED_LIBS ON)
elseif(NOT RETRACE_BUILD_SHARED AND RETRACE_BUILD_STATIC)
	set(BUILD_SHARED_LIBS OFF)
else()
	set(BUILD_SHARED_LIBS ON)
endif()

# ---------------------------------------------------------------
# Platform detection (was: configure.ac is{linux,darwin,freebsd,openbsd})
# Must come BEFORE the v1/v2 auto-disable logic below.
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
elseif(CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(RETRACE_PLATFORM_WINDOWS  TRUE)
endif()

# v2's assembly trampolines (src/v2/arch/x86-64/*) are x86-64 only until
# arm64 backends land (TODO.roadmap/03). Auto-disable on non-x86-64 hosts
# unless the user explicitly insisted.
if(RETRACE_BUILD_V2 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
	message(STATUS "retrace v2 requires x86-64 (per-arch trampolines not yet ported); disabling. See TODO.roadmap/03.")
	set(RETRACE_BUILD_V2 OFF)
endif()

# v2 also fails to build on Darwin: macOS <stdio.h> macro-substitutes
# sprintf/snprintf to __builtin___*_chk, which then references nonexistent
# members of struct RetraceRealImpls. Auto-disable on Darwin until
# TODO.roadmap/02 ports v2 to a clean real-impl indirection.
if(RETRACE_BUILD_V2 AND RETRACE_PLATFORM_DARWIN)
	message(STATUS "retrace v2 does not yet build on Darwin (stdio.h macro substitution; see TODO.roadmap/02); disabling.")
	set(RETRACE_BUILD_V2 OFF)
endif()

# v1 source uses POSIX APIs (dlfcn, unistd, sys/socket, pthread). Native
# Windows MSVC doesn't provide these — auto-disable on MSVC; the
# MSYS2/MinGW build (retrace-msys.yml) is the canonical Windows path.
if(RETRACE_PLATFORM_WINDOWS AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
	if(RETRACE_BUILD_V1)
		message(STATUS "retrace v1 requires POSIX APIs not available under MSVC; disabling. Use the MSYS2/MinGW build (msys.yml).")
		set(RETRACE_BUILD_V1 OFF)
	endif()
	if(RETRACE_BUILD_V2)
		set(RETRACE_BUILD_V2 OFF)
	endif()
endif()

# Feature-test macros — mirror AC_USE_SYSTEM_EXTENSIONS from configure.ac.
# Without _GNU_SOURCE on glibc, SO_REUSEPORT / alloca / getline / S_ISVTX
# are hidden. _DEFAULT_SOURCE covers BSDs.
if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	add_compile_definitions(_GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	add_compile_definitions(_GNU_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	add_compile_definitions(_DARWIN_C_SOURCE)
endif()

set(RETRACE_INSTALL_CMAKEDIR
	"${CMAKE_INSTALL_LIBDIR}/cmake/retrace-${PROJECT_VERSION}"
	CACHE PATH "Install path for CMake package config files")

# macOS minimum version (was: configure.ac -mmacosx-version-min=10.12)
if(RETRACE_PLATFORM_DARWIN)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "macOS deployment target" FORCE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DARWIN_C_SOURCE")
endif()

# OpenBSD STAILQ_* -> SIMPLEQ_* aliasing (was: configure.ac AC_DEFINE)
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
# Feature probes — populate config.h (was: configure.ac AC_CHECK_*)
# ---------------------------------------------------------------

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckCSourceCompiles)

check_include_file(dlfcn.h    HAVE_DLFCN_H)
check_include_file(dirent.h   HAVE_DIRENT_H)
check_include_file(execinfo.h HAVE_EXECINFO_H)
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

# gcc __atomic builtins (was: AC_LINK_IFELSE in configure.ac)
check_c_source_compiles("
	int main(void) {
		int i;
		__atomic_load_n(&i, __ATOMIC_ACQUIRE);
		__atomic_store_n(&i, 1, __ATOMIC_RELEASE);
		return 0;
	}" HAVE_ATOMIC_BUILTINS)

# gettimeofday signature (was: AC_COMPILE_IFELSE in configure.ac)
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
# check_symbol_exists doesn't handle `struct foo` well; use a compile test.
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

# Build switches (mirrored from configure.ac --enable-* flags)
if(RETRACE_BUILD_V2)
	set(RTR_V2 ON)
endif()
if(RETRACE_BUILD_CLI)
	set(RTR_V2_WRAPPER ON)
endif()
if(RETRACE_ENABLE_RPC)
	set(RTR_RPC ON)
endif()

# Generate config.h from the template (was: AC_CONFIG_HEADERS([config.h]))
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake.in"
	"${CMAKE_BINARY_DIR}/config.h"
	ESCAPE_QUOTES)

# Make config.h discoverable by every target.
add_compile_definitions(HAVE_CONFIG_H)
include_directories(BEFORE "${CMAKE_BINARY_DIR}")

# ---------------------------------------------------------------
# Sanitizers and coverage (see cmake/Sanitizers.cmake)
# ---------------------------------------------------------------

include(Sanitizers)

# ---------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

if(RETRACE_BUILD_TESTS)
	# cmocka ships with vcpkg; for system installs use the local find module.
	# It's optional — tests that need it are gated on its presence.
	find_package(Cmocka QUIET)
endif()

# ---------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------

# Public headers are installed from include/.
add_subdirectory(include/retrace)

# Core library (TODO 02) — for now, just the public API scaffold.
# Each src/<layer>/CMakeLists.txt declares its own target. Once the layer
# targets exist, an aggregate `retrace::core` alias ties them together.

if(RETRACE_BUILD_V1)
	add_subdirectory(src/v1)
endif()

if(RETRACE_BUILD_V2)
	add_subdirectory(src/v2)
endif()

# CLI (TODO 06) — placeholder until the new CLI lands.
# For now, the v1 launcher is the canonical `retrace` binary.
if(RETRACE_BUILD_CLI)
	# add_subdirectory(src/cli)  # TODO 06
endif()

if(RETRACE_BUILD_TESTS)
	if(RETRACE_BUILD_V1)
		enable_testing()
		add_subdirectory(test)
	else()
		message(STATUS "Tests requested but RETRACE_BUILD_V1=OFF; skipping (tests link against v1).")
	endif()
endif()

if(RETRACE_BUILD_EXAMPLES AND RETRACE_BUILD_V1)
	add_subdirectory(examples)
endif()

if(RETRACE_ENABLE_RPC)
	add_subdirectory(rpc)
endif()

if(RETRACE_BUILD_V1)
	add_subdirectory(tools)
endif()

# ---------------------------------------------------------------
# Package config (TODO 07)
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

# pkg-config module (TODO 07)
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/retrace-core.pc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/retrace-core.pc"
	@ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/retrace-core.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
