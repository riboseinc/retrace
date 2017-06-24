#!/bin/sh
../retrace -f ../retrace.conf.example ./dlopen
../retrace -f ../retrace.conf.example ./env
../retrace -f ../retrace.conf.example ./exit
../retrace -f ../retrace.conf.example ./file
../retrace -f ../retrace.conf.example ./fork
../retrace -f ../retrace.conf.example ./getaddrinfo
../retrace -f ../retrace.conf.example ./id
../retrace -f ../retrace.conf.example ./malloc
../retrace -f ../retrace.conf.example ./pipe
../retrace -f ../retrace.conf.example ./sock
../retrace -f ../retrace.conf.example ./sock_srv
../retrace -f ../retrace.conf.example ./str
../retrace -f ../retrace.conf.example ./time

