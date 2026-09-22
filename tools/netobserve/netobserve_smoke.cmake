# SPDX-License-Identifier: BSD-2-Clause
#
# netobserve smoke (hermetic): a local upstream (python3
# http.server), the proxy in front of it, curl through the
# chain -- the JSON record is the assertion. Self-skips when
# python3 or curl is absent.

find_program(PY3 python3)
if(NOT PY3)
	message(STATUS "netobserve smoke: python3 absent, skipping")
	return()
endif()

set(WORK ${CMAKE_CURRENT_BINARY_DIR}/netobserve-smoke)
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})

# upstream: serve the work dir on 18100
execute_process(COMMAND ${PY3} -m http.server 18100 --bind 127.0.0.1
	--directory ${WORK} OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND sleep 1)

# the proxy on 18099
execute_process(COMMAND ${NETOBSERVE} 127.0.0.1 18099
	OUTPUT_FILE ${WORK}/proxy.out ERROR_QUIET)
execute_process(COMMAND sleep 1)

# curl through the chain
execute_process(COMMAND ${CURL} -sS -x http://127.0.0.1:18099
	--max-time 10 http://127.0.0.1:18100/ RESULT_VARIABLE curl_rc
	OUTPUT_QUIET ERROR_QUIET)

execute_process(COMMAND pkill -F /dev/null
	COMMAND sh -c "pkill -f 'http.server 18100'; pkill -f 'retrace-netobserve 127.0.0.1 18099'")

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
