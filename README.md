# Introduction
`retrace` is Linux (shared object) and macOS (dynamic library) that displays C library calls and has the ability to redirect function inputs and outputs in a number of interesting ways

`retrace` can be used to assist reverse engineering/debugging dynamically linked binary Linux/FreeBSD ELF and MacOS Mach-O executables.

# Build Instructions

For platforms with Autotools, the generic way to build and install `retrace` is:

```
$ sh autogen.sh
$ ./configure --enable-tests
$ make
$ make check
$ sudo make install
```

You need Autotools installed in your system (`autoconf`, `automake`, `libtool`, `make`, `gcc` packages).

OpenSSL library and headers are automatically detected, you can specify an optional flag `--with-openssl=[PATH]` (to use a non standard OpenSSL installation root).


In order to build tests: run configure script with `--enable-tests` flag.
To build cmocka tests you can specify an optional flag `--with-cmocka=[PATH]` (to use a non standard cmocka installation root).


By the default `retrace` is installed in `/usr/bin` directory.

# Running retrace

```
$ retrace [-f configuration file location] <executable>
```

Configuration file path can be set either in `RETRACE_CONFIG` environment variable or by specifying `-f [path]` command line argument.


# Trace usage example

In its most basic form `retrace` will just print all calls that are made (and that are supported by retrace):

```
$ retrace /usr/bin/id
(2051) geteuid();
(2051) getuid();
(2051) getegid();
(2051) getgid();
(2051) fopen("/etc/passwd", "rce"); [3]
(2051) fclose(3);
(2051) fopen("/etc/group", "rce"); [3]
(2051) fclose(3);
(2051) fopen("/etc/group", "rce"); [3]
(2051) fclose(3);
(2051) fopen("/etc/group", "rce"); [3]
(2051) fclose(3);
uid=1000(test) gid=1000(test) groups=1000(test),10(wheel)
(2051) exit(0);
(2051) fileno(1);
(2051) fclose(1);
(2051) fileno(2);
(2051) fclose(2);
```

# Redirect usage example

The power of `retrace` lies its its ability to modify the behavouir of the standard system calls in a number of different ways. 
This is done using a config file.

An easy example is redirecting the output of the getuid() call:

```
$ cat retrace.conf
getuid,0
geteuid,0
getegid,0
getgid,0

$ retrace -f retrace.conf /usr/bin/id
(4982) geteuid(); [redirection in effect: '0']
(4982) getuid(); [redirection in effect: '0']
(4982) getegid(); [redirection in effect: '0']
(4982) getgid(); [redirection in effect: '0']
(4982) fopen("/etc/passwd", "rce"); [3]
(4982) fclose(3);
(4982) fopen("/etc/group", "rce"); [3]
(4982) fclose(3);
(4982) fopen("/etc/group", "rce"); [3]
(4982) fclose(3);
(4982) fopen("/etc/group", "rce"); [3]
(4982) fclose(3);
uid=0(root) gid=0(root) groups=0(root)
(4982) exit(0);
(4982) fileno(1);
(4982) fclose(1);
(4982) fileno(2);
(4982) fclose(2);
```

Other useful config file options are:

```
connect,127.0.0.1,8080,192.168.1.110,9090
```

Will redirect a connect() call from 127.0.0.1:8080 to 192.168.1.110:9090

```
fopen,/etc/passwd,/tmp/passwd
```

Will redirect a fopen() call from /etc/passwd to /tmp/passwd

```
logtofile,retrace.log
```

Will send the log file to a file rathern than stderr. This can be send to /dev/null to disable logging completly.

```
SSL_get_verify_result,10
```

Will cause the OpenSSL function SSL_get_verify_result to return any desired value.

```
memoryfuzzing,0.05
```

This option will cause a percentage of malloc(),realloc() and calloc() calls to fail. The percentage is specified in a number
from 0 (no fail) to 1 (all fail). This is useful to discover places in your code where you are not checking the return value
of allocators. The seed of the random generator can be controlled with the `fuzzingseed,1498729252` option, so that the results
are repeteable.

```
incompleteio,10
```

This option will cause the read()/write() calls to randomly write/read less bytes than was asked. A common sceneario that people
forgets to check.

```
showtimestamp
showcalltime,0.0001
```

This two options will cause a timestamp (since the begining of the tracing) to be shown and the time a call took if its bigger 
than the specified time in float seconds.


# Status
[![Travis CI Build Status](https://travis-ci.org/riboseinc/retrace.svg?branch=master)](https://travis-ci.org/riboseinc/retrace)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/12840.svg)](https://scan.coverity.com/projects/riboseinc-retrace)
