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
endif()
