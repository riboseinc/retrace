# retrace-log pp

Pretty-printer and summarizer for retrace JSON logs.

## Why

The raw retrace log is a single JSON array with one object per
intercepted call. For long runs that's thousands of dense JSON
lines — useful for tools but unreadable for humans.

`logpp` reformats it into one readable line per call, with optional
color, plus a summary of which libc functions were called most often.

## Install

No build step. Requires Python 3.7+.

```sh
$ chmod +x tools/logpp/logpp.py
$ ln -s "$(pwd)/tools/logpp/logpp.py" /usr/local/bin/retrace-logpp
```

## Usage

### Print a saved log file

```sh
$ retrace run --config cookbook/01-trace-all-calls.json \
    --log /tmp/trace.json -- /bin/ls
$ python3 tools/logpp/logpp.py /tmp/trace.json
```

### Pipe live output

```sh
$ retrace run --config cookbook/01-trace-all-calls.json -- /bin/ls 2>&1 \
    | python3 tools/logpp/logpp.py -
```

### Disable color

```sh
$ NO_COLOR=1 python3 tools/logpp/logpp.py /tmp/trace.json
```

## Output format

Per-call line:

```
[INFO ] FUNCS  Running action log_params, for malloc:0x0, tpid 0x...
[INFO ] ACT    ptr=0x7f8e2a size=64
[INFO ] FUNCS  call_duration_us=1 ret_val=5133861376
```

After all calls, a summary table:

```
Summary
  1247 calls intercepted
  pthread_mutex_lock                  85 (  6.8%)
  pthread_mutex_unlock                85 (  6.8%)
  memset                              72 (  5.8%)
  memcpy                              54 (  4.3%)
  ...
```

## See also

- [docs/cookbook/01-trace-all-calls.md](../../docs/cookbook/01-trace-all-calls.md)
