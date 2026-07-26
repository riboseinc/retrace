# SPDX-License-Identifier: BSD-2-Clause
#
# spawn — concurrent-process spawner for stress-testing.

add_executable(spawn main.c spawn.c)
target_link_libraries(spawn PRIVATE Threads::Threads)
install(TARGETS spawn RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
