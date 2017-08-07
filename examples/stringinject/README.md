
# Description

stringinjector inserts strings and/or replaces bytes in ascii and binary files. As the files can be ascii in nature (XML, JSON etc) or binary (think ELF) it needs to be able to write both. It needs to be fast as some files can be several megabytes big.

# List of the functions to apply stringinject

- File I/O functions
```
fread(), fwrite(), fgets(), fprintf(), fputs(), read()/write()
```

- Socket I/O functions
```
send(), sendto(), sendmsg(), recv(), recvfrom(), recvmsg(), read()/write()
```

# Retrace stringinject configuration

The syntax for stringinject configuration is as the following:

```
stringinject,[stringinject type],[function name list],[param],[injection rate]
```

The field 'stringinject type' may have the following values.

```
- INJECT_SINGLE_HEX
- INJECT_FORMAT_STR
- INJECT_BUF_OVERFLOW
- INJECT_FILE_LINE
```

The function names may be combined by '|' character
For example:

```
send|recv|read|write
```

## Injection of single hex

```
stringinject,INJECT_SINGLE_HEX,[function name list],[hex value:offset],[injection rate]
```
The hex value may be 0x00 ~ 0xFF or RANDOM.
The offset may be integer or RANDOM.

## Injection of format string

This configuration injects format string into specific offset.

```
stringinject,INJECT_FORMAT_STR,[function name list],[format string count:offset],[injection rate]
```

## Injection of buffer overflow

This configuration injects the string for buffer overflow.(For example, AAAAA)

```
stringinject,INJECT_BUF_OVERFLOW,[function name list],[buffer overflow:offset],[injection rate]
```

## Injection of line buffer from given file

This configuration injects a single line buffer which is randomly choosed from given file.

```
stringinject,INJECT_FILE_LINE,[function name list],[file path to be injected:offset],[injection rate]
```
