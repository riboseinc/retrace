#!/bin/sh

../retrace ./env
../retrace ./exit
../retrace ./file
../retrace ./fork
../retrace ./getaddrinfo
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
../retrace -f ../retrace.conf.example ./config
