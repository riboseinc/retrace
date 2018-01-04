#!/bin/sh

set -e

PYTHON=/usr/bin/python
CURL=/usr/bin/curl
RETRACE=retrace
CONFIG=httpredirect.config
URL1=http://127.0.0.1:8000/hello.txt
URL2=http://127.0.0.1:8000/index.txt

[ -x "$PYTHON" -a -x "$CURL" ] || (echo Python or curl not found; exit 1)
SCRIPTNAME=$(basename $0)

cd $(dirname $0)

(cd http.site && $PYTHON -m SimpleHTTPServer >/dev/null 2>&1) &
PYTHONPID=$!

# RESPONSE1 redirected url without http redirection
RESPONSE1=$($RETRACE $CURL $URL1 2>/dev/null)

# RESPONSE2 redirected url with http redirection
RESPONSE2=$($RETRACE --config $CONFIG $CURL $URL1 2>/dev/null)

# RESPONSE3 non-redirected url with http redirection
RESPONSE3=$($RETRACE --config $CONFIG $CURL $URL2 2>/dev/null)

kill $PYTHONPID

if [ "$RESPONSE1" != "Hello, world!" ]; then
	echo $SCRIPTNAME: Failed to fetch $URL1
	exit 1
fi

if [ "$RESPONSE2" != "Hello, retrace!" ]; then
	echo $SCRIPTNAME: Failed to redirect $URL1
	exit 1
fi

if [ "$RESPONSE3" != "Hello, world!" ]; then
	echo $SCRIPTNAME: Failed to fetch $URL2
	exit 1
fi
