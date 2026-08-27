# SPDX-License-Identifier: BSD-2-Clause
#
# retraced: the supervisor daemon (TODO.supervisor/02).
# P0 scope: agent socket (UDS), registry, hash-chained journal,
# HELLO/HEARTBEAT/EVENT/BYE/POLICY_ACK + PING responder.
# POSIX-only (plan 12 brings the named-pipe transport).
# P1: TLS 1.3 mTLS fleet ctl (TODO.supervisor/08 P1 /
# TODO.beyond-libc/05) -- optional OpenSSL.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_retraced
	main.c registry.c journal.c peer_gate.c retraced_ctl.c
	tls_gate.c ${_parson_source})
set_target_properties(retrace_retraced PROPERTIES
	OUTPUT_NAME retraced)

target_include_directories(retrace_retraced PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/supervisor
	${CMAKE_SOURCE_DIR}/src/config/json
	${CMAKE_SOURCE_DIR}/test/property/parson_stub)

target_link_libraries(retrace_retraced PRIVATE
	retrace_supervisor_proto)

if(OpenSSL_FOUND)
	target_compile_definitions(retrace_retraced PRIVATE
		RETRACED_HAVE_OPENSSL=1)
	target_link_libraries(retrace_retraced PRIVATE OpenSSL::SSL)
	target_include_directories(retrace_retraced PRIVATE
		${OPENSSL_INCLUDE_DIR})
endif()

install(TARGETS retrace_retraced
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
