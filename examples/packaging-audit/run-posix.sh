#!/bin/sh
# packaging-audit: declared-vs-behavior at the PACKAGING layer
# (TODO.trace-profile/19). A snap/flatpak DECLARED surface
# (snapcraft.yaml plugs / flatpak finish-args) becomes the
# inside.json shape; retrace-profile grades the OBSERVED
# behavior against it -- accesses outside the granted surface
# are confinement violations. Plus the jail exported as
# container policy (retrace-profile harden).
#
# usage: run-posix.sh [path-to-build-dir]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/../../build-test}
case "$BUILD" in /*) ;; *) BUILD=$(cd "$BUILD" && pwd);; esac
WORK=$(mktemp -d)
cd "$WORK"

printf 'declared content\n' > app.dat
cat > app.c <<'EOF'
#include <stdio.h>
int main(int c, char **v) {
	FILE *f = fopen(v[1], "rb");
	if (f) {
		fclose(f);
		printf("read declared\n");
	}
	f = fopen("/etc/hosts", "rb");
	if (f) {
		fclose(f);
		printf("read /etc/hosts -- ESCAPE\n");
	}
	return 0;
}
EOF
cc -O1 -o demo app.c

LIB="$BUILD/src/v2/libretrace.so"
[ -f "$LIB" ] || LIB="$BUILD/src/v2/libretrace.dylib"
export RETRACE_V2_LIB="$LIB"

# 1. the DECLARED surface: a snap granting only $HOME + network
cat > snapcraft.yaml <<'EOF'
name: demo
apps:
  demo:
    command: bin/demo
    plugs:
      - home
      - network
EOF

echo "=== 1. snapcraft.yaml -> the declared set"
"$BUILD/tools/retrace-snap2inside" -o inside.json snapcraft.yaml

echo "=== 2. capture the OBSERVED behavior"
"$BUILD/tools/retrace-profile" capture -o profile.json \
	-- ./demo "$WORK" >/dev/null

echo "=== 3. grade: declared vs observed (violations = escapes)"
"$BUILD/tools/retrace-profile" --libc profile.json \
	--inside inside.json -o graded.json 2>&1 \
	| grep -aE "VIOLATIONS|covers|^  " || true
"$BUILD/tools/retrace-profile" jail profile.json \
	--inside inside.json -o jail.json 2>&1 | tail -1

echo "=== 4. the jail as container policy"
"$BUILD/tools/retrace-profile" harden profile.json -o compose.yaml 2>/dev/null
head -14 compose.yaml

echo "=== done: $WORK (graded.json, jail.json, compose.yaml)"
