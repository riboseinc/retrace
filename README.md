# Retrace

NEWS: 2017-08-09: We are happy to announce the Retrace Challenge! See below!

`retrace` is a versatile security vulnerability / bug discovery tool
through monitoring and modifying the behavior of compiled binaries on
Linux, OpenBSD/FreeBSD/NetBSD (shared object) and macOS (dynamic library).

`retrace` can be used to assist reverse engineering / debugging
dynamically-linked ELF (Linux/OpenBSD/FreeBSD/NetBSD) and
Mach-O (macOS) binary executables.

[![Build Status (Travis CI)](https://img.shields.io/travis/riboseinc/retrace/master.svg)](https://travis-ci.org
/riboseinc/retrace)
[![Coverity Scan](https://img.shields.io/coverity/scan/12840.svg)](https://scan.coverity.com/projects/riboseinc-retrace)

# The Retrace Challenge + Rewards

Posted: 2017-08-09

We are happy to announce the Retrace Challenge!

The challenge is composed of three categories:

* Bug Challenge
* Improvement Challenge
* Usage Challenge

Our goal of the challenge is to:

* encourage security researchers to use `retrace` during their vulnerability assessment efforts;
* encourage developers to use `retrace` for code stability efforts; and
* make `retrace` more useful!

The main category is the "Bug Challenge": to find bugs (any bug AND security vulnerabilities)
in well-known software (OSS or proprietary) using `retrace`.


## Important Information

* Participation: anyone can participate
* Organizer: Ribose `retrace` team
* Eligibility period: From 2017-08-09 00:00:00 to 2017-10-10 00:00:00 (GMT) inclusive
* Results:
  * Winners will be announced on 2017-10-16 (GMT) on the same page as this announcement.
  * The selection of prizes will be determined solely by the organizer. We may not select winner(s) in a category if the quality of submissions is below our bottom line.
  * Reward amounts are in USD.
  * Winning challengers will receive their award via Amazon gift cards or PayPal transfer.

Please understand that while we aim to fully implement the spirit of fairness, in the case of any unresolved ambiguity or dispute, the organizer has the sole rights to make all decisions.


## The Bug Challenge

The Bug Challenge encourages finding bugs (any bug AND security vulnerabilities) in
well-known software (OSS / proprietary) using `retrace`.

### Eligibility

* The bug *MUST* be reported to the target software author AND the organizer during the "eligibility period". You must provide evidence to demonstrate this to the organizer (e.g., provide the public / private bug report).
* The bug will be eligible to the reward levels based on its CVE CVSS score.
* There is no absolute definition of "well-known software" -- it will be determined by the organizer -- please contact us to confirm prior to submission.
* Challenge submissions *ARE* allowed to be also submitted to other bounty programs under the condition that those bug reports *ALSO* fulfill our bug report eligibility requirements listed above. If a submission is discovered to be reported to other bounty programs that for example, do not mention `retrace`, it will be disqualified from our challenge.

### Eligibility Of Bug Report

* The bug report *MUST* be confirmed and accepted by the software author to be eligible for this challenge.
* The bug report *MUST*
  * Describe how `retrace` is used to discover / reproduce it
  * Include a link to `https://riboseinc.github.io/retrace/`
  * Mention "This submission is in response to the Ribose Retrace Challenge"

### CVE CVSS Score And Using retrace

* Critical: remote exploitable buffer overflows etc.
  * For example, can be discovered through Socket and File IO fuzzing (work in progress in retrace) or the `stringinject` utility.
* High: local exploitable buffer overflows etc.
  * For example, can be discovered through `getenv` fuzzing or the `stringinject` utility.
* Medium: severe crashes, buffer overflow etc.
  * Easy to discover using various retrace options, `stringinject`.
* Low: your average issues such as memory bugs.
  * Easy to discover using various retrace options, `stringinject`.

### Bug Challenge Rewards

Challenge rewards are given according to the CVE CVSS score of the entry:

* Critical: 1 grand prize of $1,000
* High: 2 prizes of $500
* Medium: 3 prizes of $200
* Low: a public name mention, the journey is the reward!

The `retrace` team will decide among all submissions of the same class (e.g., Medium, High), which discovered bugs would receive what prize, according to criteria derived from the following angles:

* Impact of the target software and impact of the bug in target software. We seek bugs that are more general in impact; rarely occuring bugs in obscure software will be de-prioritized.
* Creativity of retrace usage.

### Submitting To The Bug Challenge

Send an email to retrace@ribose.com with subject "Retrace Bug Challenge Submission" providing the following information:

1. Your particulars
  * Name (Title and Company if any)
  * Email

2. Bug details
  * Description
  * CVE score
  * CVE link and bug report link
  * Evidence of bug report acknowledged and confirmed by software author
  * Evidence of fulfillment of challenge eligibility criteria (e.g., inclusion of `retrace` usage) in the bug report



## The Improvement Challenge

The "improvement challenge" is to improve the actual `retrace` tool in form of code.

The challenger should write code that improves retrace (library or CLI) to do something useful.

### Eligibility

* The improvement *MUST* work on all supported platforms (unless it is a platform-specific improvement)
* The improvement *MUST* be accompanied with code to demonstrate "how the improvement is useful", and the results must be reproducible.
* The PR *MUST* NOT fail our builds on any platforms (Linux, \*BSD)
* Remember that this is an open source project. Your PR may be accepted and included in the `retrace` distribution.
* If the submission is interesting yet the quality of it needs improvement, the retrace team will provide feedback to you.
* Your submission *MUST* be able to be cleanly rebased by the close of submission period.
* Your code *MUST* be free to use by the `retrace` project, such as it does not violate any intellectual property rights (e.g., license agreements or patents) of third parties. Since `retrace` is released under the 2-clause BSD license, the submitted code will also be provided publicly under the 2-clause BSD license.

### Improvement Challenge Rewards

* Best Improvement Winner: 1 grand prize of $1,000
* Runner-Ups: 2 prizes of $500
* Commendable: 3 prizes of $50 each
* Worthy: incorporation into the `retrace` repo with a public name mention.

### Submitting To The Improvement Challenge

Submission is through GitHub Pull Requests to the https://github.com/riboseinc/retrace[`retrace` git repo].

* Your sample code *MUST* be in form of a PR.
* The PR title *MUST* start with "Retrace Improvement Challenge Submission: " with a brief description of improvement following that.
* The PR description must contain the following paragraph:

> I confirm that this submission does not infringe upon any intellectual property rights of any third party, and I have full rights to grant any rights and licenses of this work. I hereby assign the retrace project and its successors, a royalty-free, irrevocable, worldwide, non-exclusive, perpetual right and license to use, distribute, reproduce, modify and prepare derivative works of this submission, to perform and display publicly this submission, and to practice inventions in or associated with this submission, with (for each of the foregoing) full rights to authorize others to do the same.

## The Usage Challenge

The "usage challenge" is to discover creative and interesting ways of using `retrace` in form of code.

The challenger should write code that utilizes and incorporates retrace (lib or CLI) to do something useful AND interesting. The results will be incorporated in the `/examples` directory of the `retrace` repo for public usage, for the benefit of all.

### Eligibility

* Submitted code *MUST* be immediately runnable and results reproducable for the organizer (i.e. include script to install any dependencies, how to run the code and verify usage).
* Submitted code *MUST* be runnable across all supported platforms (unless it is platform-specific).
* The PR must not fail our builds on any platforms (Linux, \*BSD)
* Remember that this is an open source project. Your PR may be accepted and included in the `retrace` distribution.
* If the submission is interesting yet the quality of it needs improvement, the retrace team will provide feedback to you.
* Your code *MUST* be free to use by the `retrace` project, such as it does not violate any intellectual property rights (e.g., license agreements or patents) of third parties. Since `retrace` is released under the 2-clause BSD license, the submitted code will also be provided publicly under the 2-clause BSD license.

### Usage Challenge Rewards

* Best Usage Winner: 1 grand prize of $500
* Runner-Ups: 2 grand prizes of $200
* Commendable: 3 prizes of $50 each
* Worthy: incorporation into the `retrace` repo with a public name mention.

Your submission will be judged on how useful it is to the `retrace` target audience. The term "useful" is defined by its common English definition, with any decisions solely decided by the organizer.

### Submitting To The Usage Challenge

Submission is through GitHub Pull Requests to the [`retrace` git repo](https://github.com/riboseinc/retrace).

* Your sample code must be in form of a PR to the `/examples` directory, with a unique directory path in form of `/examples/{your-github-handle}/{your-entry-name}`.
* The PR title *MUST* start with "Retrace Usage Challenge Submission: " with a brief description of usage following that.
* The PR description must contain this phrase: "I confirm this submission is original work and will not infringe upon any intellectual property rights of any third party, and I have full rights to grant to the retrace project any rights and licenses."



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
logtofile,retrace.log
```

Will send the log file to a file rather than `stderr`. You can configure
log output to write to `/dev/null` disable logging completely.

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


