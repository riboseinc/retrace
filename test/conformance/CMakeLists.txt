# SPDX-License-Identifier: BSD-2-Clause
#
# Conformance registration for the retraced control protocol
# (TODO.supervisor/01 + 11). The suite derives everything from
# protocol.h (SSOT) and FAILS when the checked-in schemas or
# wire goldens drift. Python-driven; POSIX CI legs (the C-side
# frame test covers Windows).

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
	add_test(NAME conformance-rpc
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/test_rpc_conformance.py
			--repo ${CMAKE_SOURCE_DIR})
	set_tests_properties(conformance-rpc PROPERTIES LABELS "unit")

	# The reference stub agent (TODO.supervisor/11 P0): a
	# third-party implementation (no retrace headers) walking the
	# L3 state machine against the REAL daemon. The
	# implementability gate for the beyond-libc agents (03/04).
	if(NOT WIN32)
		add_test(NAME conformance-ref-agent
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/ref_agent.py
				$<TARGET_FILE:retrace_retraced>
				${CMAKE_SOURCE_DIR})
		set_tests_properties(conformance-ref-agent
			PROPERTIES LABELS "integration"
			TIMEOUT 60)
	endif()
endif()
