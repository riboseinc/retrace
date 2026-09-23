# SPDX-License-Identifier: BSD-2-Clause
#
# netobserve smoke (hermetic): a local upstream (python3
# http.server), the proxy in front of it, curl through the
# chain -- the JSON record is the assertion. Daemons run in
# the BACKGROUND: execute_process waits for completion, and
# neither the proxy's accept loop nor http.server exits on
# their own (the first cut hung every Linux CI leg for the
# full job timeout). Self-skips when python3/curl are absent.

find_program(PY3 python3)
find_program(CURL curl)
if(NOT PY3 OR NOT CURL)
	message(STATUS "netobserve smoke: python3 or curl absent, skipping")
	return()
endif()

set(WORK ${CMAKE_CURRENT_BINARY_DIR}/netobserve-smoke)
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})

execute_process(COMMAND /bin/sh -c
	"${PY3} -m http.server 18100 --bind 127.0.0.1 --directory ${WORK} >/dev/null 2>&1 &"
	RESULT_VARIABLE bg1 TIMEOUT 5)
execute_process(COMMAND /bin/sh -c
	"${NETOBSERVE} 127.0.0.1 18099 >${WORK}/proxy.out 2>/dev/null &"
	RESULT_VARIABLE bg2 TIMEOUT 5)
execute_process(COMMAND sleep 1 TIMEOUT 5)

execute_process(COMMAND ${CURL} -sS -x http://127.0.0.1:18099
	--max-time 10 http://127.0.0.1:18100/ RESULT_VARIABLE curl_rc
	OUTPUT_QUIET ERROR_QUIET TIMEOUT 20)

execute_process(COMMAND /bin/sh -c
	"pkill -f 'http.server 18100'; pkill -f 'retrace-netobserve 127.0.0.1 18099'"
	RESULT_VARIABLE kill_rc TIMEOUT 10)

if(NOT curl_rc EQUAL 0)
	message(FATAL_ERROR "curl through the proxy failed: ${curl_rc}")
endif()
if(NOT EXISTS ${WORK}/proxy.out)
	message(FATAL_ERROR "proxy produced no output")
endif()
file(READ ${WORK}/proxy.out out)
if(NOT out MATCHES "http-request")
	message(FATAL_ERROR "no http-request record in proxy output:\n${out}")
endif()
if(NOT out MATCHES "127.0.0.1:18100")
	message(FATAL_ERROR "record missing the upstream host:\n${out}")
endif()
message(STATUS "netobserve smoke: request recorded through the chain")
