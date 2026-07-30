# Network fuzzing

Retrace can be used to fuzz network related system calls.

## v2 JSON config

`retrace.conf.json` logs every network call and applies a 5% random
failure rate to `send`/`recv` (via the `memory_fuzz` action) so you can
see how the client copes with transport-level errors.

```sh
$ cc -o test_client test_client.c
$ RETRACE_JSON_CONFIG=retrace.conf.json \
    LD_PRELOAD=../../build/src/v2/libretrace.so ./test_client
```

Tune the failure rate by editing `fail_rate` in the JSON; pair with
`fuzzing_seed` for deterministic runs.

## v1 syntax (legacy, no longer wired up)

The original text-config syntax exposed typed failure modes per call
(`fuzzing-net,connect,ADDR_INUSE,0.5`). v2 collapses the universe of
"force-fail this call" behaviors into the `memory_fuzz` action, which
returns ENOMEM-style failures at a configurable rate. The typed modes
(ADDR_INUSE, CONN_RESET, ...) can be reintroduced as additional
actions; see `TODO.complete/10-v1-actions-port.md`.

```
fuzzing-net,[function name],[fuzzing type],[fuzzing rate]
```


## Function names and fuzzing types:

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
```
