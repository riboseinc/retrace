# SPDX-License-Identifier: BSD-2-Clause
#
# retrace VS Code extension (TODO.complete/31 MVP).
#
# Pure TypeScript; build separately via `npm install && npm run compile`.
# CMake just installs the source files so distro packagers can find them.

install(DIRECTORY . DESTINATION share/retrace/vscode-extension
    FILES_MATCHING
    PATTERN "*.json"
    PATTERN "*.ts"
    PATTERN "*.md"
    PATTERN "CMakeLists.txt" EXCLUDE
    PATTERN "node_modules" EXCLUDE
    PATTERN "out" EXCLUDE)
