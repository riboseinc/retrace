#!/bin/sh
#
# only tested on macOS!

readonly rootcheck="root-check"
readonly src="${rootcheck}.c"

if [ ! -f "${src}" ]; then
	echo "cannot open '${src}'"
	exit 1
fi

gcc "${src}" -o "${rootcheck}" 2>/dev/null
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

echo "running '${pwd}/${rootcheck}':"
"${pwd}/${rootcheck}"
echo

readonly getuidconf="getuid-redirect.conf"
echo "running retrace to redirect 'getuid()' calls with '${getuidconf}' on '${pwd}/${rootcheck}':"
if [ ! -f "${pwd}/${getuidconf}" ]; then
	echo "cannot open '${pwd}/${getuidconf}'"
	exit 1
fi
"${retrace}" -f "${pwd}/${getuidconf}" "${pwd}/${rootcheck}" 2>&1 | grep -e "^[a-z]" | grep uid
echo

readonly geteuidconf="geteuid-redirect.conf"
echo "running retrace to redirect 'geteuid()' calls with '${geteuidconf}' on '${pwd}/${rootcheck}':"
if [ ! -f "${pwd}/${geteuidconf}" ]; then
	echo "cannot open '${pwd}/${geteuidconf}'"
	exit 1
fi
"${retrace}" -f "${pwd}/${geteuidconf}" "${pwd}/${rootcheck}" 2>&1 | grep -e "^[a-z]" | grep uid
echo

readonly getbothconf="get-both-redirect.conf"
echo "running retrace to both redirect 'getuid()' and 'geteuid()' calls with '${getbothconf}' on '${pwd}/${rootcheck}', the protection should now be fully bypassed:"
if [ ! -f "${pwd}/${getbothconf}" ]; then
	echo "cannot open '${pwd}/${getbothconf}'"
	exit 1
fi
"${retrace}" -f "${pwd}/${getbothconf}" "${pwd}/${rootcheck}" 2>&1 | grep -e "^[a-z]" | grep uid
echo
