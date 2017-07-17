# getenv retrace fuzzing
Retrace can be used to fuzz system calls related with networking.

## getenv fuzz configuration syntax
```
fuzzing-net,[function name],[fuzzing type],[fuzzing rate]
```
## Function names and Fuzzing types:

1. socket, accept
NO_MEMORY, LIMIT_SOCKET

2. connect
ADDR_INUSE, NET_UNREACHABLE, CONN_TIMEOUT

3. bind, listen
ADDR_INUSE

4. send, sendto, sendmsg
CONN_RESET, NO_MEMORY

5. recv, recvfrom, recvmsg
CONN_REFUSE, NO_MEMORY

6. gethostbyname, gethostbyaddr
HOST_NOT_FOUND, SERVICE_NOT_AVAIL

7. getaddrinfo
HOST_NOT_FOUND, SERVICE_NOT_AVAIL, NO_MEMORY

## Example runs:

```sh
$ cc -o test_client test_client.c
$ cd ../../
$ ./retrace -f examples/net-fuzzing/netfuzzing.conf ./examples/net-fuzzing/test_client

