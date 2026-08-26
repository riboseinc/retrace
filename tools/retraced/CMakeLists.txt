# SPDX-License-Identifier: BSD-2-Clause
#
# retraced: the supervisor daemon (TODO.supervisor/02).
# P0 scope: agent socket (UDS), registry, hash-chained journal,
# HELLO/HEARTBEAT/EVENT/BYE/POLICY_ACK + PING responder.
# POSIX-only (plan 12 brings the named-pipe transport).

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_retraced
	main.c registry.c journal.c peer_gate.c ${_parson_source})
set_target_properties(retrace_retraced PROPERTIES
	OUTPUT_NAME retraced)

target_include_directories(retrace_retraced PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/supervisor
	${CMAKE_SOURCE_DIR}/src/config/json
	${CMAKE_SOURCE_DIR}/test/property/parson_stub)

target_link_libraries(retrace_retraced PRIVATE
	retrace_supervisor_proto)

install(TARGETS retrace_retraced
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
