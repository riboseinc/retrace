#!/bin/sh

set -e

cd $(dirname $0)

READFN="fread|read|recv"
WRITEFN="fwrite|write|send"

strinject () {
	for fn in $READFN $WRITEFN; do
		echo $1,$fn,$2
		echo "(ABCD)" -\> "($3)"
		echo logtofile,/dev/null >strinject.conf
		echo stringinject,$1,$fn,$2,1 >>strinject.conf
		../retrace -f strinject.conf ./strinject ABCD "$3"
	done
}

strinject_1 () {
	for fn in $READFN $WRITEFN; do
		echo $fn,$1,$2
		echo "(ABCD)" -\> "($3)"
		echo logtofile,/dev/null >strinject.conf
		echo stringinject,$fn,$1,$2,1 >>strinject.conf
		../retrace -f strinject.conf ./strinject ABCD "$3"
	done
}

strinject INJECT_SINGLE_HEX 0x58:0 XBCD
strinject INJECT_SINGLE_HEX 0x58:1 AXCD

strinject INJECT_FORMAT_STR 1:0 %sABCD
strinject INJECT_FORMAT_STR 2:0 %s%sABCD
strinject INJECT_FORMAT_STR 1:2 AB%sCD

strinject INJECT_BUF_OVERFLOW 2:0 AAABCD
strinject INJECT_BUF_OVERFLOW 1:2 ABACD
strinject INJECT_BUF_OVERFLOW 1:0 AABCD

strinject INJECT_FILE_LINE strinject.txt:0 XXXXABCD
strinject INJECT_FILE_LINE strinject.txt:2 ABXXXXCD

strinject_1 "chr(1234)" 0 1234
strinject_1 "chr(1234)" 1 A1234
strinject_1 "chr(1234)" 1:3:3 A123
strinject_1 'chr(\\20234)' 1:3:3 "A 23"
strinject_1 'chr(1)' 0:4 1111
strinject_1 'chr(12)' 0:8 12121212
strinject_1 'or(\\20)' 0:4 abcd
strinject_1 'and(\\fe)' 0:4 @BBD
strinject_1 'xor(\\01)' 0:4 @CBE
