# SPDX-License-Identifier: BSD-2-Clause
#
# retrace Grafana data source plugin (TODO.complete/32 MVP).
#
# Pure TypeScript; build separately via `npm install && npm run build`.
# CMake installs the source files so distro packagers can find them.

install(DIRECTORY . DESTINATION share/retrace/grafana-plugin
    FILES_MATCHING
    PATTERN "*.json"
    PATTERN "*.ts"
    PATTERN "*.md"
    PATTERN "CMakeLists.txt" EXCLUDE
    PATTERN "node_modules" EXCLUDE
    PATTERN "dist" EXCLUDE)
