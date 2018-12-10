# Retrace

`retrace` is a versatile security vulnerability / bug discovery tool
through monitoring and modifying the behavior of compiled binaries on
Linux, OpenBSD/FreeBSD/NetBSD (shared object) and macOS (dynamic library).

`retrace` can be used to assist reverse engineering / debugging
dynamically-linked ELF (Linux/OpenBSD/FreeBSD/NetBSD) and
Mach-O (macOS) binary executables.

[![Build Status (Travis CI)](https://img.shields.io/travis/riboseinc/retrace/master.svg)](https://travis-ci.org
/riboseinc/retrace)
[![Coverity Scan](https://img.shields.io/coverity/scan/12840.svg)](https://scan.coverity.com/projects/riboseinc-retrace)

## Who is Ribose?

We are [Ribose](https://www.ribose.com), the secure sharing company. We believe privacy and security form the foundation of liberty.

Our goal is to empower individuals and organizations alike to freely communicate and achieve productivity for the greater good, through our deep security and technology expertise, creating highly-secure products validated to the world’s most stringent requirements and regulations.

We created `retrace` to aid developers and security researchers to develop better code that leads the world to a better place.


## Contacting The Organizer

The Ribose `retrace` team can be reached at retrace@ribose.com. We will answer questions to the best of our efforts.


# Building retrace

For platforms with Autotools, the generic way to build and install
`retrace` is:

``` console
$ sh autogen.sh
$ ./configure --enable-tests
$ make
$ make check
$ sudo make install
```

You need Autotools installed in your system (`autoconf`, `automake`,
`libtool`, `make`, `gcc` packages).

OpenSSL library and headers are automatically detected, you can specify
an optional flag `--with-openssl=[PATH]` (to use a non-standard OpenSSL
installation root).

In order to build tests: run configure script with `--enable-tests`
flag.  To build cmocka tests you can specify an optional flag
`--with-cmocka=[PATH]` (to use a non-standard cmocka installation root).

By the default `retrace` is installed in `/usr/bin` directory.


# Running retrace

``` console
$ retrace [-f configuration file location] <executable>
```

Configuration file path can be set either in `RETRACE_CONFIG`
environment variable or by specifying `-f [path]` command line argument.

# Interactive user interface

Interactive control over retrace can be exercised using cli control feature
over pseudo terminal (currently available on Linux platforms only).
In order to enable the feature, export `RETRACE_CLI=1` environment variable.
When enabled, the name of the pseudo device will be printed with a 3 seconds delay during the start up:

``` console
RETRACE-INIT [INFO]: cli pts is at: /dev/pts/1
```

After that, a terminal emulator can be connected to that device:

``` console
minicom -p /dev/pts/1
```

Hit the main Enter key, and the command menu with prompt will appear:

``` console
Welcome to minicom 2.7.1

OPTIONS: I18n
Compiled on Aug 13 2017, 15:25:34.
Port /dev/tty8, 13:10:04

Press CTRL-A Z for help on special keys


Failed to get command id, retry
Retrace command menu
--------------------
[0] <Main> Say Hi
[1] <Main> Terminate process
Enter command id>>
```

The following are the characteristics of the terminal:
* Raw mode - no line processing will be performed on the input.
* Characters are echoed back.
* Main enter key is used to mark the end of input.

## Note to developers
In order to expose interactive commands from your module, use API defined in retrace_cli.h.
To register commads use cli_register_command_blk(). To interact with the user use cli_printf() and cli_scanf().
Refer to retrace_main.c for reference.

# Examples

## Trace usage example

In its most basic form `retrace` will just print all calls that are made
(and that are supported by retrace):

``` console
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

## Redirect usage example

The power of `retrace` lies its its ability to modify the behavior of
the standard system calls in a number of different ways.
This is done using a config file.

An easy example is redirecting the output of the `getuid()` call:

``` console
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


# Config Options

Other useful config file options are listed below.

## Connect

``` sh
connect,127.0.0.1,8080,192.168.1.110,9090
```

Will redirect a `connect()` call from `127.0.0.1:8080` to
`192.168.1.110:9090`.

## File-related

``` sh
fopen,/etc/passwd,/tmp/passwd
```

Will redirect a `fopen()` call from `/etc/passwd` to `/tmp/passwd`.

## Logging

``` sh
logtofile,retrace.log,1
```

Will send the log file to a file rather than `stderr`. You can configure
log output to write to `/dev/null` disable logging completely.

The second parameter indicates whether  we should flush the output after every
write. Disabling it (setting it to 0) has a considerable performace gain, but
runs the risk of losing the last part of the output if the program crashes.

``` sh
logperthread
```
This setting with no parameters, in combination with `logtofile`, will cause
one log to be written per every spawned process and thread. In this case the
path passed to logtofile will be used as the base for the file names, appening
the process id and thread id (only for non main threads).

## OpenSSL

``` sh
SSL_get_verify_result,10
```

Will cause the OpenSSL function `SSL_get_verify_result` to return any
desired value.

## Memory Fuzzing

``` sh
memoryfuzzing,0.05
```

This option will cause a percentage of `malloc()`, `realloc()` and
`calloc()` calls to fail.

The percentage is specified in a number (`float`) from `0` (no fail) to
`1` (all fail). This is useful to discover places in your code where you
are not checking the return value of allocators.

The seed of the random generator can be controlled with the
`fuzzingseed,1498729252` option, so that the results are repeatable.

## Incomplete I/O

``` sh
incompleteio,10
```

This option will cause the `read()` / `write()` calls to randomly
write/read less bytes than was asked. A common scenario that people
forget to check.

## Time tracking

``` sh
showtimestamp
showcalltime,0.0001
```

These two options will cause a timestamp (since the beginning of the
tracing) to be shown and the time a call took if it's larger than
the specified time in float seconds.

## Logging

``` sh
logging-global,LOG_GROUP_FILE|LOG_GROUP_MEM,RTR_LOG_LEVEL_ALL
logging-excluded-funcs,free|memcpy|malloc
logging-allowed-funcs,strlen
stacktrace-groups,LOG_GROUP_MEM
stacktrace-disabled-funcs,calloc
```

These options will enable or disable logging options by group or level.
The each group, level or function may be combined by `|` character.

``` sh
logging-global,[logging group],[logging level]
	groups: LOG_GROUP_ALL,LOG_GROUP_MEM,LOG_GROUP_FILE,LOG_GROUP_NET,LOG_GROUP_SYS,
		LOG_GROUP_STR,LOG_GROUP_SSL,LOG_GROUP_PROC
	levels: LOG_LEVEL_ALL,LOG_LEVEL_NOR,LOG_LEVEL_ERR,LOG_LEVEL_FUZZ,LOG_LEVEL_REDIRECT

logging-excluded-funcs,[functions list]
logging-allowed-funcs,[functions list]
stacktrace-groups,[logging groups]
stacktrace-disabled-funcs,[functions list]
```

## Data dump

``` sh
disabledatadump
```

By default `retrace` will dump the full buffers passed to functions such as `write()` and `read()`.
If this option (without parameters) is present, no buffers will be dumped. This can be useful to get
a cleaner output. It will even speed up the tracing if there are a lot of buffers are being printed.

# Notes

## macOS System Integrity Protection

We use the DYLD_INSERT_LIBRARIES enviroment variable to insert `retrace` into binaries.
Starting on Mac OS X El Capitan Apple removes the DYLD_INSERT_LIBRARIES variable for
the enviroment for binaries in system directories. This means you can't trace system binaries
using `retrace` by default.

You can disable this behaviour by running `csrutil disable` and rebooting.


# Feedback

`retrace` is under heavy development and we are always looking to implement new
and useful features that allows debugging and reverse engineering programs in
new and interesting ways.

Please send feedback and improvement suggestions either as GitHub issues or to
[retrace@ribose.com](mailto:retrace@ribose.com).

