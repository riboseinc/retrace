# SPDX-License-Identifier: BSD-2-Clause
#
# retrace examples. See TODO.roadmap/11-documentation.md.
#
# Examples are self-contained demos with their own .conf files. Most have
# their own test-*.sh script (exercised by TODO.roadmap/08 integration tests).

# dns-fuzz: axfr.c, query.c
add_executable(axfr  examples/dns-fuzz/axfr.c)
add_executable(query examples/dns-fuzz/query.c)

# getenv-fuzzing
add_executable(getenv examples/getenv-fuzzing/getenv.c)

# http-server-overflow
add_executable(http-server examples/http-server-overflow/http-server.c)
target_link_libraries(http-server PRIVATE Threads::Threads)

# id-redirection
add_executable(root-check examples/id-redirection/root-check.c)

# net-fuzzing
add_executable(net-fuzzing-client examples/net-fuzzing/test_client.c)

# stringinject
add_executable(strinject_test examples/stringinject/strinject_test.c)

# unsafe-system: no binary to build (config-only example)
