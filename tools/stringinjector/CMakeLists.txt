# SPDX-License-Identifier: BSD-2-Clause
#
# stringinjector — file/string injection helper used by test/strinject.sh.

add_executable(stringinjector stringinjector.c)
install(TARGETS stringinjector RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
