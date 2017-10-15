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
#    by default macOS does not come with 'timeout' and Retrace might hang
#    because of some invalid DNS query
# 3) a functional nameserver (with a configured zone) that accepts queries
#    I installed NSD on Ubuntu for this purpose on VirtualBox
#
# query.c is nameserver query tool that uses sendto() making it a perfect
# example for Retrace string injection
#
# Inner workings of dns-fuzz and result:
# 1) Go through the entire hex space 0x00:0xff inserting a hex char at an offset
#    of the DNS query packet. For loop through each offset of the DNS query
#    packet length.
#
# Result of hex injection starting at offset 4, hex 0x00 to 0xff:
#                    |
#                    ▼
# 0000000 3120 0100 0001 0000 0000 0000 0377 7777 0b72 6962 | 1 ...........www.rib
# 0000000 3129 0100 0101 0000 0000 0000 0377 7777 0b72 6962 | 1)...........www.rib
# 0000000 3132 0100 0201 0000 0000 0000 0377 7777 0b72 6962 | 12...........www.rib
# 0000000 313b 0100 0301 0000 0000 0000 0377 7777 0b72 6962 | 1;...........www.rib
# 0000000 3144 0100 0401 0000 0000 0000 0377 7777 0b72 6962 | 1D...........www.rib
# 0000000 314d 0100 0501 0000 0000 0000 0377 7777 0b72 6962 | 1M...........www.rib
# 0000000 3156 0100 0601 0000 0000 0000 0377 7777 0b72 6962 | 1V...........www.rib
# <SNIP>
# 0000000 4e96 0100 f901 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4e9f 0100 fa01 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4ea8 0100 fb01 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4eb1 0100 fc01 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4eba 0100 fd01 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4ec3 0100 fe01 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4ecc 0100 ff01 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
#
# Result of hex injection starting at offset 5, hex 0x00 to 0xff:
#                      |
#                      ▼
# 0000000 4ed5 0100 0000 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4ede 0100 0001 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4ee7 0100 0002 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4ef0 0100 0003 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4ef9 0100 0004 0000 0000 0000 0377 7777 0b72 6962 | N............www.rib
# 0000000 4f02 0100 0005 0000 0000 0000 0377 7777 0b72 6962 | O............www.rib
# 0000000 4f0b 0100 0006 0000 0000 0000 0377 7777 0b72 6962 | O............www.rib
# 0000000 4f14 0100 0007 0000 0000 0000 0377 7777 0b72 6962 | O............www.rib
# 0000000 4f1d 0100 0008 0000 0000 0000 0377 7777 0b72 6962 | O............www.rib
# 0000000 4f26 0100 0009 0000 0000 0000 0377 7777 0b72 6962 | O&...........www.rib
# <END>
#
# 2) Inject a long string (AAAAAA..) at an offset of the DNS query packet. For
#    loop through each offset of the DNS query packet length.
#
# Result of buffer overflow injection starting at offset 0, length 1
#         |
#         ▼
# 0000000 4153 e301 0000 0100 0000 0000 0003 7777 770b 7269 | AS............www.ri
# 0000020 626f 7365 2d74 6573 7402 6e6c 0000 0100 01        | bose-test.nl.....
#
# Result of buffer overflow injection starting at offset 0, length 2
#         |
#         ▼
# 0000000 4141 53ec 0100 0001 0000 0000 0000 0377 7777 0b72 | AAS............www.r
# 0000020 6962 6f73 652d 7465 7374 026e 6c00 0001 0001      | ibose-test.nl.....
#
# Result of buffer overflow injection starting at offset 0, length 4
#         |
#         ▼
# 0000000 4141 4141 53f5 0100 0001 0000 0000 0000 0377 7777 | AAAAS............www
#
# Result of buffer overflow injection starting at offset 0, length 8
#         |
#         ▼
# 0000000 4141 4141 4141 4141 53fe 0100 0001 0000 0000 0000 | AAAAAAAAS...........
# 0000020 0377 7777 0b72 6962 6f73 652d 7465 7374 026e 6c00 | .www.ribose-test.nl.
# 0000040 0001 0001                                         | ....
#
# Result of buffer overflow injection starting at offset 0, length 16
#         |
#         ▼
# 0000000 4141 4141 4141 4141 4141 4141 4141 4141 5407 0100 | AAAAAAAAAAAAAAAAT...
# 0000020 0001 0000 0000 0000 0377 7777 0b72 6962 6f73 652d | .........www.ribose-
# 0000040 7465 7374 026e 6c00 0001 0001                     | test.nl.....
#
# Result of buffer overflow injection starting at offset 0, length 32
#         |
#         ▼
# 0000000 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000020 4141 4141 4141 4141 4141 4141 5410 0100 0001 0000 | AAAAAAAAAAAAT.......
# 0000040 0000 0000 0377 7777 0b72 6962 6f73 652d 7465 7374 | .....www.ribose-test
# 0000060 026e 6c00 0001 0001                               | .nl.....
#
# Result of buffer overflow injection starting at offset 0, length 64
#         |
#         ▼
# 0000000 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000020 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000040 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000060 4141 4141 5419 0100 0001 0000 0000 0000 0377 7777 | AAAAT............www
#
# Result of buffer overflow injection starting at offset 0, length 128
#         |
#         ▼
# 0000000 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000020 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000040 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000060 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000080 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000100 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
# 0000120 4141 4141 4141 4141 5422 0100 0001 0000 0000 0000 | AAAAAAAAT"..........
# 0000140 0377 7777 0b72 6962 6f73 652d 7465 7374 026e 6c00 | .www.ribose-test.nl.
# 0000160 0001 0001                                         | ....
#
# 3) Inject format strings (%s%s%s%s..) at an offset of the DNS query packet.
#    For loop through each offset of the DNS query packet length.
#
# TODO: FORMAT STRING INJECT RESULT
# <END>
#
# NOTE: THIS IMPLEMENTATION CURRENTLY ENLARGES THE DNS QUERY PACKET. WORK IS
#       ONGOING TO MAKE RETRACE OVERWRITE INSTEAD OF INJECT, ETA IS VERY SOON!
#
# TODOS:
# 1) Add zone transfer requests
# 2) Add DNSSEC operations
# 3) Replace query.c with actual nslookup and dig as these have much more
#    capabilities that can be used as fuzzing vehicles
#
#

rm -rf /tmp/nslookup.conf*

readonly query="examples/dns-fuzzing/query"
readonly gtimeout="/usr/local/bin/gtimeout"

# the size of a UDP DNS request packet, the for loops depend on this
readonly packetsize="36"

# this is the IP of my virtual machine running NSD
readonly dnsserver="192.168.99.100"
# this is the hostname my NSD is serving
readonly hostname="www.ribose-test.nl"

# go into the retrace root
cd ../..

# verify query and gtimeout are there
if [ ! -x "${query}" ]; then
	echo "cannot execute '${query}' (hint: 'gcc query.c -o query -Wall')"
	exit 1
fi

if [ ! -x "${gtimeout}" ]; then
	echo "cannot execute '${gtimeout}' (hint: 'brew install coreutils')"
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
