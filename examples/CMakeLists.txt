# SPDX-License-Identifier: BSD-2-Clause
#
# retrace examples.
#
# Examples are self-contained demos with their own .conf files. Most have
# their own test-*.sh script (exercised by the modernization plan/08 integration tests).

# Windows (TODO.windows/04 examples parity): the socket-based
# demos (dns-fuzz, http-server-overflow, net-fuzzing,
# stringinject) need winsock wrappers and stay POSIX-only for
# now; root-check is a POSIX-privileges concept. The portable
# set + the escape-hunting demo build everywhere.
if(RETRACE_PLATFORM_WINDOWS)
	add_executable(getenv getenv-fuzzing/getenv.c)
	add_executable(escape-demo escape-hunting/escape-demo.c)
	add_executable(quickstart-app
		trace-profile-quickstart/app.c)
	return()
endif()

add_executable(quickstart-app trace-profile-quickstart/app.c)

# dns-fuzz: axfr.c, query.c
add_executable(axfr  dns-fuzz/axfr.c)
add_executable(query dns-fuzz/query.c)

# getenv-fuzzing
add_executable(getenv getenv-fuzzing/getenv.c)

# http-server-overflow
add_executable(http-server http-server-overflow/http-server.c)
target_link_libraries(http-server PRIVATE Threads::Threads)

# id-redirection
add_executable(root-check id-redirection/root-check.c)

# net-fuzzing
add_executable(net-fuzzing-client net-fuzzing/test_client.c)

# stringinject
add_executable(strinject_test stringinject/strinject_test.c)

# escape-hunting (TODO.windows/04 examples parity): the recipe-33
# demo. Pure stdio -- POSIX and Windows alike.
add_executable(escape-demo escape-hunting/escape-demo.c)

# unsafe-system: no binary to build (config-only example)
