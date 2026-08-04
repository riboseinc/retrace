# SPDX-License-Identifier: BSD-2-Clause
#
# Property-based tests. Pure C, no engine state required. Each test
# runs many iterations of a generated input against an invariant.
# See TODO.complete/16-property-based-tests.md for the plan.

# All property tests link against parson directly (no engine state).
set(_parson_sources
	${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_property_parson
	test_property_parson.c
	${_parson_sources})
set_target_properties(retrace_property_parson PROPERTIES
	OUTPUT_NAME property_parson
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/property")

target_include_directories(retrace_property_parson PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/config/json)

# Standard C; no platform-specific defines needed since parson is
# self-contained.

add_test(NAME property-parson
	COMMAND retrace_property_parson)
set_tests_properties(property-parson PROPERTIES
	LABELS "property;unit"
	TIMEOUT 60)
