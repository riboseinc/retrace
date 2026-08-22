# SPDX-License-Identifier: BSD-2-Clause
#
# Run a converter tool on a fixture and compare JSON output
# with the expected file. Normalized: machine-specific values
# (the concrete home) become $HOME so fixtures stay portable;
# whitespace is stripped before the byte compare.
execute_process(COMMAND ${TOOL} ${FIXTURE}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/conv-out.json
	RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
	message(FATAL_ERROR "converter exited ${rc}")
endif()
file(READ ${CMAKE_CURRENT_BINARY_DIR}/conv-out.json got)
file(READ ${EXPECTED} want)
string(REPLACE "$ENV{HOME}" "$HOME" got "${got}")
string(REGEX REPLACE "[ \t\n\r]" "" got_n "${got}")
string(REGEX REPLACE "[ \t\n\r]" "" want_n "${want}")
if(NOT got_n STREQUAL want_n)
	message(FATAL_ERROR "converter output mismatch")
endif()
