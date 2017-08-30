# HTTP server overflow

This is an example on how you can use retrace to fuzz http servers.

I included a sample http server (```http-server.c```) which runs as a single process, not forked, binds to TCP:8080 and can accept one connection and then exits.

Disclaimer: this was written for macOS

# Preparation

Compile http-server.c and copy curl to /tmp/ to bypass SIP

```sh
$ uname -a
Darwin OF-OSX-007 16.6.0 Darwin Kernel Version 16.6.0: Fri Apr 14 16:21:16 PDT 2017; root:xnu-3789.60.24~6/RELEASE_X86_64 x86_64
$ pwd
/Users/test/src/riboseinc/retrace/examples/http-server-overflow
$ ls -la
total 16
drwxr-xr-x  4 test  staff   136 Aug 30 10:30 .
drwxr-xr-x  7 test  staff   238 Aug 30 10:27 ..
-rw-r--r--  1 test  staff   458 Aug 30 10:29 README.md
-rw-r--r--  1 test  staff  1478 Aug 30 10:30 http-server.c
$ gcc http-server.c -o /tmp/http-server -Wall
$ cp /usr/bin/curl /tmp/curl
```

# Test run
(This assumes you have already compiled retrace)

Start the server.

Terminal 1:
```sh
$ /tmp/http-server
```

Create a retrace configuration file to only show network related calls.

Terminal 2:
```sh
$ echo "logging-global,LOG_GROUP_NET,LOG_LEVEL_ALL" > /tmp/retrace.conf
```

Now retrace curl connecting to 127.0.0.1:8080

Terminal 2:
```sh
$ pwd
/Users/test/src/riboseinc/retrace
$ ./retrace -f /tmp/retrace.conf /tmp/curl -v http://127.0.0.1:8080
curl(35544,0x7fffd0aa63c0) malloc: *** malloc was initialized without entropy
(35544) socket(30, 2, 0, ) = 4
* Rebuilt URL to: http://127.0.0.1:8080/
(35544) (thread: 249864192) getaddrinfo("127.0.0.1", "8080", AI_FAMILY:0]
, 0x7fff4be00480, ) = 127.0.0.1]

(35544) (thread: 249864192) freeaddrinfo(0x7fff4be00480, )
(35544) socket(2, 1, 6, ) = 4
*   Trying 127.0.0.1...
(35544) setsockopt(4 [socket], 6, 1, 0x7fff5e9a0dec, 4, ) = 0
* TCP_NODELAY set
(35544) setsockopt(4 [socket], 65535, 4130, 0x7fff5e9a0dec, 4, ) = 0
(35544) setsockopt(4 [socket], 65535, 8, 0x7fff5e9a0f30, 4, ) = 0
(35544) setsockopt(4 [socket], 6, 257, 0x7fff5e9a0f30, 4, ) = 0
(35544) setsockopt(4 [socket], 6, 16, 0x7fff5e9a0f30, 4, ) = 0
(35544) connect(4, 127.0.0.1:8080[AF_INET], 16, ) = -1
* Connected to 127.0.0.1 (127.0.0.1) port 8080 (#0)
(35544) sendto(4 [socket[127.0.0.1:8080]], 0x7fff4bc0ca10, 78, 0, 0x0, 0, ) = 78
	0000000	 4745 5420 2f20 4854 5450 2f31 2e31 0d0a 486f 7374 | GET / HTTP/1.1..Host
	0000020	 3a20 3132 372e 302e 302e 313a 3830 3830 0d0a 5573 | : 127.0.0.1:8080..Us
	0000040	 6572 2d41 6765 6e74 3a20 6375 726c 2f37 2e35 312e | er-Agent: curl/7.51.
	0000060	 300d 0a41 6363 6570 743a 202a 2f2a 0d0a 0d0a      | 0..Accept: */*....
(35544) send(4 [socket[127.0.0.1:8080]], 0x7fff4bc0ca10, 78, ) = 78
	0000000	 4745 5420 2f20 4854 5450 2f31 2e31 0d0a 486f 7374 | GET / HTTP/1.1..Host
	0000020	 3a20 3132 372e 302e 302e 313a 3830 3830 0d0a 5573 | : 127.0.0.1:8080..Us
	0000040	 6572 2d41 6765 6e74 3a20 6375 726c 2f37 2e35 312e | er-Agent: curl/7.51.
	0000060	 300d 0a41 6363 6570 743a 202a 2f2a 0d0a 0d0a      | 0..Accept: */*....
> GET / HTTP/1.1
> Host: 127.0.0.1:8080
> User-Agent: curl/7.51.0
> Accept: */*
>
(35544) recv(4 [socket[127.0.0.1:8080]], 0x7fff4c004f88, 16384, 0, ) = 74
	0000000	 4854 5450 2f31 2e31 2033 3032 2046 6f75 6e64 0a43 | HTTP/1.1 302 Found.C
	0000020	 6f6e 7465 6e74 2d4c 656e 6774 683a 2033 350a 0a3c | ontent-Length: 35..<
	0000040	 6874 6d6c 3e3c 626f 6479 3e3c 703e 6869 3c2f 703e | html><body><p>hi</p>
	0000060	 3c2f 626f 6479 3e3c 2f68 746d 6c3e                | </body></html>
< HTTP/1.1 302 Found
< Content-Length: 35
<
* Curl_http_done: called premature == 0
* Connection #0 to host 127.0.0.1 left intact
<html><body><p>hi</p></body></html>
```

You can see the response ```<html><body><p>hi</p></body></html>``` from the server.

Now check the server.

Terminal 1:
```sh
$ /tmp/http-server
buf[78]: GET / HTTP/1.1
Host: 127.0.0.1:8080
User-Agent: curl/7.51.0
Accept: */*


$ echo $?
0
```

The server exited nicely.

# Now start fuzzing :)

Start the server again.

Terminal 1:
```sh
$ /tmp/http-server
```

Create a retrace configuration file with stringinject for the ```send()``` and ```sendto()``` functions.

Terminal 2:

```sh
$ echo "logging-global,LOG_GROUP_NET,LOG_LEVEL_ALL" > /tmp/retrace.conf
$ echo "stringinject,INJECT_BUF_OVERFLOW,send|sendto,1024:3,1" >> /tmp/retrace.conf
```

This configuration file will cause retrace to inject a 1024 byte string at offset 3 in any ```send()``` or ```sendto()``` calls.

Now retrace ```curl``` again using the configuration file:
```sh
$ ./retrace -f /tmp/retrace.conf /tmp/curl -v http://127.0.0.1:8080
curl(35748,0x7fffd0aa63c0) malloc: *** malloc was initialized without entropy
(35748) socket(30, 2, 0, ) = 4
* Rebuilt URL to: http://127.0.0.1:8080/
(35748) (thread: 253882368) getaddrinfo("127.0.0.1", "8080", AI_FAMILY:0]
, 0x7fff4be00250, ) = 127.0.0.1]

(35748) (thread: 253882368) freeaddrinfo(0x7fff4be00250, )
(35748) socket(2, 1, 6, ) = 4
*   Trying 127.0.0.1...
(35748) setsockopt(4 [socket], 6, 1, 0x7fff55775dec, 4, ) = 0
* TCP_NODELAY set
(35748) setsockopt(4 [socket], 65535, 4130, 0x7fff55775dec, 4, ) = 0
(35748) setsockopt(4 [socket], 65535, 8, 0x7fff55775f30, 4, ) = 0
(35748) setsockopt(4 [socket], 6, 257, 0x7fff55775f30, 4, ) = 0
(35748) setsockopt(4 [socket], 6, 16, 0x7fff55775f30, 4, ) = 0
(35748) connect(4, 127.0.0.1:8080[AF_INET], 16, ) = -1
* Connected to 127.0.0.1 (127.0.0.1) port 8080 (#0)
(35748) sendto(4 [socket[127.0.0.1:8080]], 0x7fff4bc00ac0, 303, 0, 0x0, 0, ) = 303 [[redirected]] [fuzzing seed: 1504061940]
	0000000	 0000 0000 0000 0000 0000 0000 0000 0000 1300 4141 | ..................AA
	0000020	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000040	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000060	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000080	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000100	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000120	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000140	 4141 4141 4141 4141 4141 4141 41                  | AAAAAAAAAAAAA
(35748) send(4 [socket[127.0.0.1:8080]], 0x7fff4bc00a20, 153, ) = 303 [[redirected]] [fuzzing seed: 1504061940]
	0000000	 4745 5441 4141 4141 4141 4141 4141 4141 4141 4141 | GETAAAAAAAAAAAAAAAAA
	0000020	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000040	 4141 4141 4141 4141 4141 4141 4141 4141 4141 4141 | AAAAAAAAAAAAAAAAAAAA
	0000060	 4141 4141 4141 4141 4141 4141 4141 4141 4141      | AAAAAAAAAAAAAAAAAA
> GET / HTTP/1.1
> Host: 127.0.0.1:8080
> User-Agent: curl/7.51.0
> Accept: */*
>
* upload completely sent off: 225 out of 0 bytes
(35748) recv(4 [socket[127.0.0.1:8080]], 0x7fff4c804388, 16384, 0, ) = 0
* Curl_http_done: called premature == 0
* Empty reply from server
* Connection #0 to host 127.0.0.1 left intact
curl: (52) Empty reply from server
```

You can clearly see the injected AAAA's, but there was no response. Now take a look at the server.

Terminal 1:

```sh
$ /tmp/http-server
buf[127]: GETAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
triggering BOF:
Abort trap: 6
$ echo $?
134
$
```

A nice coredump.

# Conclusion

This example demonstrates how you can retrace any network client to fuzz network servers and try to trigger buffer overflows.
