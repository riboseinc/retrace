# Introduction
`retrace` is Linux (shared object) and macOS (dynamic library) that displays C library calls and has the ability to redirect function inputs and outputs.

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

You need Autotools installed in your system (`autoconf`, `automake`, `libtool`, `gcc` packages).

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
```
$ export RETRACE_CONFIG="/home/test/retrace_redirect.conf"
$ retrace /usr/bin/id
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

# Status
[![Travis CI Build Status](https://travis-ci.org/riboseinc/retrace.svg?branch=master)](https://travis-ci.org/riboseinc/retrace)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/12840.svg)](https://scan.coverity.com/projects/riboseinc-retrace)
