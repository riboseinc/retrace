#!/bin/sh

# make sure ../retrace itself can fail
../retrace -f nonexistent ls || ../retrace nonexistent || echo ok

./strinject.sh
LC_ALL="POSIX" ../retrace ./setlocale
../retrace ./exec
../retrace ./env
../retrace ./exit
../retrace ./file
../retrace ./fork
../retrace ./getaddrinfo
./httpredirect.sh
../retrace ./id
../retrace ./malloc
../retrace ./pipe
../retrace ./sock
../retrace ./sock_srv
../retrace ./str
../retrace ./time
../retrace ./dlopen
../retrace ./dir
../retrace ./popen
../retrace ./char
../retrace ./perror
../retrace ./printf
../retrace ./scanf
../retrace ./char
../retrace ./trace
../retrace ./log
../retrace ./writev
../retrace -f ../retrace.conf.example ./config
