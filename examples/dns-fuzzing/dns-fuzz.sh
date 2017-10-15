#!/bin/bash
#
# dns-fuzz.sh: very crude DNS fuzzer for macOS using Retrace
#
# ONLY USE THIS CODE AS A REFERENCE, IT CONTAINS A HARDCODED NAMESERVER IP
# ADRESS AND HOSTNAME. IT IS NOT MEANT AS A STANDALONE FUZZING TOOL. YOU CAN USE
# IT FOR INSPIRATION AND TO GIVE YOU AN IDEA HOW RETRACE CAN BE USED TO FUZZ
# NAMESERVERS.
# 
# Dependencies:
# 1) the executable 'query' which can be compiled with: 'gcc query.c -o query'
# 2) gtimeout which can be installed with 'brew install coreutils'
#    macOS does not come with 'timeout' and Retrace might hang because of some
#    invalid DNS query
# 3) a functional nameserver (with a configured zone) that accepts queries
#    I installed NSD on Ubuntu for this purpose on VirtualBox
#
# query.c is nameserver query tool that uses sendto() making it a perfect
# example for Retrace string injection
#
# Inner workings and result:
# 1) Go through the entire hex space 0x00:0xff inserting a hex char at an offset
# 2) Inject long strings (AAAAAA..) inserting at an offset
# 3) Inject format strings (%s%s%s%s..) inserting at an offset

rm -rf /tmp/nslookup.conf*

readonly query="examples/dns-fuzzing/query"
readonly gtimeout="/usr/local/bin/gtimeout"
readonly packetsize="36"

# this is the IP of my virtual machine running NSD
readonly dnsserver="192.168.99.100"
# this is the hostname my NSD is serving
readonly hostname="www.ribose-test.nl"

# go into the retrace root
cd ../..

# verify query and gtimeout are there
if [ ! -x "${query}" ]; then
	echo "cannot execute '${query}', hint: 'gcc query.c -o query -Wall'"
	exit 1
fi

if [ ! -x "${gtimeout}" ]; then
	echo "cannot execute '${gtimeout}', hint: 'brew install coreutils'"
	exit 1
fi

# inject a hex char, start at offset 4 here
for ((pos=4; pos < ${packetsize}; pos++)); do
	for a in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do
		for b in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do
			hex="0x${a}${b}"
			conf="/tmp/nslookup.conf.${hex}"
			echo -e "logging-global,LOG_GROUP_NET,LOG_LEVEL_ALL\nstringinject,INJECT_SINGLE_HEX,sendto,${hex}:${pos},1" > "${conf}"
			${gtimeout} 1 ./retrace -f "${conf}" "${query}" "${dnsserver}" "${hostname}"
			rm -rf /tmp/nslookup.conf*
		done
	done
done

# inject buffer overflows
for ((pos=0; pos < ${packetsize}; pos++)); do
	for len in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096; do
		conf="/tmp/nslookup.conf.${pos}.${len}"
		echo -e "logging-global,LOG_GROUP_NET,LOG_LEVEL_ALL\nstringinject,INJECT_BUF_OVERFLOW,sendto,${len}:${pos},1" > "${conf}"
		${gtimeout} 2 ./retrace -f "${conf}" "${query}" "${dnsserver}" "${hostname}"
		rm -rf /tmp/nslookup.conf*
	done
done

# inject format strings
for ((pos=0; pos < ${packetsize}; pos++)); do
	for len in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096; do
		conf="/tmp/nslookup.conf.${pos}.${len}"
		echo -e "logging-global,LOG_GROUP_NET,LOG_LEVEL_ALL\nstringinject,INJECT_FORMAT_STR,sendto,${len}:${pos},1" > "${conf}"
		${gtimeout} 2 ./retrace -f "${conf}" "${query}" "${dnsserver}" "${hostname}"
	done
done
