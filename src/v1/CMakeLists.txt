# SPDX-License-Identifier: BSD-2-Clause
#
# retrace v1 build under CMake. See TODO.roadmap/01-foundation-build-cmake.md.
#
# This file builds the v1 library and launcher in parallel with the existing
# Autotools build. It does NOT yet have feature parity — the Autotools build
# remains authoritative until every probe and platform flag is migrated.
#
# Status: scaffold — see task list at the bottom.

set(_v1_sources
	char.c        common.c      dir.c         dlopen.c
	env.c         exec.c        exit.c        file.c
	fork.c        httpredirect.c id.c         log.c
	malloc.c      netdb.c       netfuzz.c     perror.c
	pipe.c        pledge.c      popen.c       printf.c
	read.c        scanf.c       select.c      sock.c
	ssl.c         str.c         strinject.c   temp.c
	time.c        trace.c       write.c       ioctl.c
	retrace_cli.c retrace_main.c)

set(_v1_headers
	char.h        common.h      dir.h         dlopen.h
	env.h         exec.h        exit.h        file.h
	fork.h        httpredirect.h id.h         log.h
	malloc.h      netfuzz.h     rtr-netdb.h   perror.h
	pipe.h        pledge.h      popen.h       printf.h
	read.h        rtr-time.h    scanf.h       select.h
	sock.h        ssl.h         str.h         strinject.h
	temp.h        trace.h       write.h       ioctl.h
	retrace_cli.h retrace_env.h retrace.h     retrace_main.h)

# Library
add_library(retrace_v1 ${_v1_sources})
add_library(retrace::v1 ALIAS retrace_v1)

# Feature-test macros — mirror AC_USE_SYSTEM_EXTENSIONS from configure.ac.
# Must be set per-target so they appear on the compile command before any
# system header is included. Without these on glibc, SO_REUSEPORT / alloca /
# getline / S_ISVTX / RTLD_NEXT are hidden behind __USE_GNU.
if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_v1 PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	target_compile_definitions(retrace_v1 PRIVATE _GNU_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_v1 PRIVATE _DARWIN_C_SOURCE)
endif()

target_include_directories(retrace_v1
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/util)

target_link_libraries(retrace_v1
	PUBLIC OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})

set_target_properties(retrace_v1 PROPERTIES
	OUTPUT_NAME retrace
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	C_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)

# Launcher binary (was: bin_PROGRAMS=retrace in Makefile.am)
add_executable(retrace retrace.c)
target_link_libraries(retrace PRIVATE retrace::v1)

install(TARGETS retrace_v1 retrace
	EXPORT retraceTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${_v1_headers}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/retrace/v1)

# TODO.roadmap/01 tasks for v1-CMake parity:
#
# - [ ] Mirror every AC_CHECK_DECLS probe (O_TMPFILE, PTRACE_*, SO_*, fcntl
#       features) via check_symbol_exists / check_struct_member.
# - [ ] Generate config.h equivalent via configure_file.
# - [ ] Verify macOS -mmacosx-version-min=10.12 flows through CMAKE_OSX_DEPLOYMENT_TARGET.
# - [ ] Verify OpenBSD STAILQ_*->SIMPLEQ_* rewrite (set at top-level CMakeLists).
# - [ ] Verify the cmocka test path compiles when RETRACE_BUILD_TESTS=ON.
