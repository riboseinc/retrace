#!/bin/sh
#
# only tested on macOS and Ubuntu

readonly getenv="getenv"
readonly src="${getenv}.c"

if [ ! -f "${src}" ]; then
	echo "cannot open '${src}'"
	exit 1
fi

# this might generate some warnings because the source file
# contains two vulnerabilities
gcc "${src}" -o "${getenv}" 2>/dev/null
if [ $? -ne 0 ]; then
	echo "compilation '${src}' failed"
	exit 1
fi

readonly pwd="$(pwd)"

cd ../../

readonly retrace="./retrace"
if [ ! -x "${retrace}" ]; then
	echo "cannot execute '${retrace}'"
	exit 1
fi

readonly bofconf="getenvbof-retrace.conf"
echo "running retrace to overflow 'FOOBAR' in '${getenv}' with '${bofconf}', this will cause a segfault:"
if [ ! -f "${pwd}/${bofconf}" ]; then
	echo "cannot open '${pwd}/${bofconf}'"
	exit 1
fi
"${retrace}" -f "${pwd}/${bofconf}" "${pwd}/${getenv}" 2>&1 | egrep "FOOBAR|./retrace|fault"
echo

readonly fmtconf="getenvfmt-retrace.conf"
echo "running retrace to trigger a format string bug using 'FOOBAR' in '${getenv}' with '${fmtconf}', this will cause a segfault:"
if [ ! -f "${pwd}/${fmtconf}" ]; then
	echo "cannot open '${pwd}/${fmtconf}'"
	exit 1
fi
"${retrace}" -f "${pwd}/${fmtconf}" "${pwd}/${getenv}" 2>&1 | egrep "FOOBAR|./retrace|fault"
echo

readonly fmtbofconf="getenvfmtbof-retrace.conf"
echo "running retrace to overflow 'FOOBAR' in '${getenv}' with '${fmtbofconf}', this will cause a segfault:"
if [ ! -f "${pwd}/${fmtbofconf}" ]; then
	echo "cannot open '${pwd}/${fmtbofconf}'"
	exit 1
fi
"${retrace}" -f "${pwd}/${fmtbofconf}" "${pwd}/${getenv}" 2>&1 | egrep "FOOBAR|./retrace|fault"
