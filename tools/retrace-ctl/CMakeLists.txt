# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-ctl -- the fleet CLI (TODO.supervisor/07). POSIX-only
# like retraced until plan 12 (named pipes). TLS client path
# (TODO.supervisor/08 P1) reuses tools/retraced/tls_gate.c.

if(RETRACE_PLATFORM_WINDOWS)
	# supervisor/12: the fleet CLI over the ctl named pipe. The
	# TLS client and sign-policy ride the POSIX build for now
	# (tls_gate.c pulls POSIX socket headers).
	add_executable(retrace_ctl main.c)
else()
	add_executable(retrace_ctl
		main.c
		${CMAKE_SOURCE_DIR}/tools/retraced/tls_gate.c)
endif()
set_target_properties(retrace_ctl PROPERTIES
	OUTPUT_NAME retrace-ctl)

target_include_directories(retrace_ctl PRIVATE
	${CMAKE_SOURCE_DIR}/tools/retraced)

if(OpenSSL_FOUND AND NOT RETRACE_PLATFORM_WINDOWS)
	target_compile_definitions(retrace_ctl PRIVATE
		RETRACED_HAVE_OPENSSL=1
		RETRACE_HAVE_OPENSSL=1)
	target_link_libraries(retrace_ctl PRIVATE OpenSSL::SSL)
	target_include_directories(retrace_ctl PRIVATE
		${OPENSSL_INCLUDE_DIR})
endif()

install(TARGETS retrace_ctl
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
