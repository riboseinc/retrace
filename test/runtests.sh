#!/bin/sh

# retrace configuration var
RTR_CONF_VAR=`cat ../retrace.conf.example`

./strinject.sh
LC_ALL="POSIX" retrace ./setlocale
retrace ./exec
retrace ./env
retrace ./exit
retrace ./file
retrace ./fork
retrace ./getaddrinfo
./httpredirect.sh
retrace ./id
retrace ./malloc
retrace ./pipe
retrace ./sock
retrace ./sock_srv
retrace ./str
retrace ./time
retrace ./dlopen
retrace ./dir
retrace ./popen
retrace ./char
retrace ./perror
retrace ./printf
retrace ./scanf
retrace ./char
retrace ./trace
retrace ./log
retrace ./writev
retrace ./readv
export RETRACE_CONFIG_VAR="${RTR_CONF_VAR}"
retrace ./config
retrace --config ../retrace.conf.example ./config
