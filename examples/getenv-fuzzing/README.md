# getenv retrace fuzzing
Retrace can be used to fuzz environment variables to test for buffer overflows (similar to Dave Aitel's sharefuzz) and format string bugs.

## user controlled environment variables
Environment variables are mostly user controlled and are therefor excellent vectors to try to trigger buffer overflows in setuid binaries.
Unsafe usage of environment variables during copying (strcpy/sprintf/strcat etc) or displaying/logging without format specifiers could result in exploitation.

## environment variable fuzzing
`retrace` can be used to return environment variables containing:
* long strings of arbitrary length containing '\x41'
* format strings of arbitrary length containing '%s'
* garbage strings of arbitrary length

## getenv fuzz configuration syntax
```
fuzzing-getenv,[Allowed fuzzing envs],[Excluded fuzzing envs],[Fuzzing Type],[Length],[Fuzzing Rate]
```

1. Allowed Fuzzing Envs: all or environment variables (may combine using |), e.g.: HOME|PATH|TMPDIR
2. Excluded Fuzzing Envs: all or environment variables (may combine using |), e.g.: USER|LD_AUDIT|DISPLAY
3. Fuzzing Types: BUFFER_OVERFLOW, FORMAT_STRING or GARBAGE
4. Length: size of the returning buffer (used for triggering buffer overflows)
5. Fuzzing rate: rate of fuzzing (1: 100% of the time, 0.25: 25% of the time)

## Examples:
Always fuzz the 'FOOBAR' environment variable with a 1024 byte string
```
fuzzing-getenv,FOOBAR,all,BUFFER_OVERFLOW,1024,1
```

Always fuzz the 'FOOBAR' environment variable with a 10 byte format string:
```
fuzzing-getenv,FOOBAR,all,FORMAT_STRING,10,1
```

Always fuzz the 'FOOBAR' environment variable with a 1024 byte format string:
```
fuzzing-getenv,FOOBAR,all,FORMAT_STRING,1024,1
```

Randomly (with a rate of 0.25) fuzz ANY environment variable with a 1024 byte format long string (this kills two birds with one stone, buffer overflows and format strings):
```
fuzzing-getenv,all,all,FORMAT_STRING,1024,0.25
```

## Example runs:

Fuzz environment variable 'FOOBAR' in the test 'getenv' executable using different retrace configuration files:

```sh
$ ./retrace -f examples/getenv-fuzzing/getenvbof-retrace.conf examples/getenv/getenv
<SNIP>
(22606) getenv("FOOBAR", ) = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA[SNIP]" [[redirected]] [fuzzing seed: 1499654559]
(22606) printf("strcpy(buf(%d), FOOBAR);\n" -> "strcpy(buf(128), FOOBAR);\n", ) = 26
./retrace: line 68: 22606 Segmentation fault: 11  RETRACE_CONFIG=/Users/test/retrace/examples/getenv/getenvbof-retrace.conf DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES=.libs/libretrace.dylib /Users/test/retrace/examples/getenv/getenv
```

```sh
$ ./retrace -f examples/getenv-fuzzing/getenvfmt-retrace.conf examples/getenv/getenv
<SNIP>
(22616) getenv("FOOBAR", ) = "s%s%s%s%s%" [[redirected]] [fuzzing seed: 1499654559]
(22616) printf("strcpy(buf(%d), FOOBAR);\n" -> "strcpy(buf(128), FOOBAR);\n", ) = 26
(22616) printf("FOOBAR [bof] | %s\n" -> "FOOBAR [bof] | s%s%s%s%s%\n", ) = 26
(22616) printf("FOOBAR [fmt] | " -> "FOOBAR [fmt] | ", ) = 15
./retrace: line 68: 22616 Segmentation fault: 11  RETRACE_CONFIG=/Users/test/retrace/examples/getenv/getenvfmt-retrace.conf DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES=.libs/libretrace.dylib /Users/test/retrace/examples/getenv/getenv
```

```sh
$ ./retrace -f examples/getenv-fuzzing/getenvfmtbof-retrace.conf examples/getenv/getenv
<SNIP>
(22625) getenv("FOOBAR", ) = "s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%[SNIP]" [[redirected]] [fuzzing seed: 1499654559]
(22625) printf("strcpy(buf(%d), FOOBAR);\n" -> "strcpy(buf(128), FOOBAR);\n", ) = 26
./retrace: line 68: 22625 Segmentation fault: 11  RETRACE_CONFIG=/Users/test/retrace/examples/getenv/getenvfmtbof-retrace.conf DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES=.libs/libretrace.dylib /Users/test/retrace/examples/getenv/getenv
```

## Try it out yourself!

Configuration file `getenvfmtbof-all-retrace.conf` is a generic retrace config file to fuzz environment variables on macOS. It has several exclusions and a fuzzing rate of 0.25.

Use `test-getenv-fuzzing.sh` or experiment with different configuration files.

segfaults == profit
