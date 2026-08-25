# SPDX-License-Identifier: BSD-2-Clause
#
# The retraced control-protocol module (TODO.supervisor/01).
# Owns: the wire frame codec + the frozen message table (the
# single source of truth the conformance suite derives from).
# Does NOT own: the daemon (tools/retraced), the in-process
# agent (src/core, plan 03), transport (plan 08/12).
#
# A plain static library: linked by the daemon, the agent (via
# core), retrace-ctl, and the tests. No engine dependency --
# protocol code must stay testable in isolation.

add_library(retrace_supervisor_proto STATIC protocol.c)
target_include_directories(retrace_supervisor_proto PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
