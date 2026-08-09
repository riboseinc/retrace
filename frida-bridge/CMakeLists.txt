# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-frida: Frida bridge for retrace (TODO.complete/28 MVP).
#
# Just a JS script + README. No C build. The script lives at
# frida-bridge/retrace-frida.js and is invoked via `frida -l ...`.
#
# Install to share/retrace/frida-bridge/ so users find it next to
# the policies.

install(FILES retrace-frida.js README.md
    DESTINATION share/retrace/frida-bridge)
