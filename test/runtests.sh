#!/bin/sh

if $(uname | grep -q ^Darwin); then
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./env
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./exit
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./file
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./fork
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./id
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./malloc
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./pipe
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./sock
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./sock_srv
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./str
        DYLD_INSERT_LIBRARIES=../retrace.dylib ./time
else
        LD_PRELOAD=../retrace.so ./env
        LD_PRELOAD=../retrace.so ./exit
        LD_PRELOAD=../retrace.so ./file
        LD_PRELOAD=../retrace.so ./fork
        LD_PRELOAD=../retrace.so ./id
        LD_PRELOAD=../retrace.so ./malloc
        LD_PRELOAD=../retrace.so ./pipe
        LD_PRELOAD=../retrace.so ./sock
        LD_PRELOAD=../retrace.so ./sock_srv
        LD_PRELOAD=../retrace.so ./str
        LD_PRELOAD=../retrace.so ./time
        LD_PRELOAD=../retrace.so ./pledge
fi

