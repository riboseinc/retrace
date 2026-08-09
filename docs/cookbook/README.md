# retrace cookbook

A recipe-driven guide to using retrace for tracing, fuzzing,
redirection, mocking, and security auditing. Each recipe is
self-contained: copy the JSON config, run the command, observe
the result.

## How to use the cookbook

Every recipe follows the same shape:

1. **Problem** — the debugging/testing scenario.
2. **Config** — a JSON file you can save and pass via `--config`.
3. **Invocation** — the exact `retrace run` command.
4. **Expected output** — what you should see.
5. **Variations** — knobs you can turn.

Recipes are independent — read them in any order.

## Quick start

```sh
$ cmake -B build -G Ninja && cmake --build build
$ sudo cmake --install build
$ retrace --help
```

To run any recipe below:

```sh
$ retrace run --config cookbook/<recipe>.json -- <your-program>
```

## Recipe index

### Tracing & observability

| # | Recipe | What it teaches |
|---|--------|-----------------|
| 01 | [Trace every libc call](01-trace-all-calls.md) | The default; log every interceptable call as JSON. |
| 02 | [Filter by function name](02-filter-by-function.md) | Use `RETRACE_LOGGER_ALLOWED_FUNCS` to focus on a handful of calls. |
| 03 | [Count calls per function](03-count-calls.md) | Aggregate stats: which libc calls dominate your program. |
| 04 | [Time each call](04-time-each-call.md) | Find hot spots via `call_duration_us` in the log. |

### Redirection & mocking

| # | Recipe | What it teaches |
|---|--------|-----------------|
| 05 | [Mock `getuid()` for root checks](05-mock-getuid.md) | Make a binary think it's root (or any uid). |
| 06 | [Redirect `open()` paths](06-redirect-open.md) | Swap one file for another without touching the binary. |
| 07 | [Redirect network connects](07-redirect-connect.md) | Point `connect()` at a different host/port. *(planned)* |
| 08 | [Mock `time()` for time-sensitive code](08-mock-time.md) | Freeze or shift time for replay / boundary tests. |

### Fuzzing & fault injection

| # | Recipe | What it teaches |
|---|--------|-----------------|
| 09 | [Fuzz `malloc` failures](09-fuzz-malloc.md) | Inject OOM into a target to test error paths. |
| 10 | [Partial I/O (short reads/writes)](10-incomplete-io.md) | Make `read`/`write` return short to exercise retry logic. |
| 11 | [Deterministic fuzzing seed](11-deterministic-fuzz.md) | Reproduce a fuzz run by pinning the RNG seed. |
| 12 | [Fail specific syscalls](12-fail-specific.md) | Force `connect`/`open`/etc. to return an error code. |

### Security audit

| # | Recipe | What it teaches |
|---|--------|-----------------|
| 13 | [Audit `system()` for injection](13-audit-system.md) | Find unsafe `system()` calls in setuid binaries. |
| 14 | [Trace `getenv()` reads](14-trace-getenv.md) | Map which env vars a binary consults. |
| 15 | [Capture network traffic](15-capture-network.md) | Log every `send`/`recv` to a pcap-style JSON stream. |

### Advanced

| # | Recipe | What it teaches |
|---|--------|-----------------|
| 16 | [Multi-function script](16-multi-script.md) | Compose actions across many functions in one config. |
| 17 | [Per-return-address routing](17-per-return-address.md) | Different behavior for different call sites of the same function. |
| 18 | [Fuzz enprot's EPT parser](18-fuzz-enprot.md) | Cross-project recipe: stress-test [engyon/enprot](https://github.com/engyon/enprot) via retrace — audit file access, fuzz malloc, simulate short reads. |
| 19 | [CI fuzzing](19-ci-fuzzing.md) | Drop-in `.github/workflows/retrace-fuzz.yml` that catches OOM + short-IO bugs on every PR. |
| 20 | [Sandbox a binary](20-sandbox.md) | Runtime file-access deny-list: block `/etc/shadow`, `/root/.ssh/`, etc. |
| 21 | [Mock SSL certificate verification](21-mock-ssl-verify.md) | Force `SSL_get_verify_result` to any X509_V_* code — test cert handling without standing up servers. |

### Protocol decoding

| # | Recipe | What it teaches |
|---|--------|-----------------|
| 22 | [Decode HTTP and DNS wire formats](22-decode-protocols.md) | Parse `send`/`recv` buffers as HTTP/DNS and log structured fields instead of raw bytes. |

## Action reference

Compact form. For the full schema and parameter reference, see
[configuration.md](../configuration.md#built-in-actions).

| Action | Effect |
|--------|--------|
| `log_params` | Log call args + return value as JSON. |
| `call_real` | Invoke the real libc implementation. |
| `modify_in_param_str` | Rewrite a string argument before the call. |
| `modify_in_param_int` | Rewrite an integer argument before the call. |
| `modify_in_param_arr` | Rewrite a byte-array argument before the call. |
| `modify_return_value_int` | Override the return value after the call. |
| `memory_fuzz` | Randomly fail `malloc`/`calloc`/`realloc` at a configurable rate. |
| `incomplete_io` | Truncate read/write return values at a configurable rate. |
| `fuzzing_seed` | Pin the RNG seed for deterministic fuzzing. |
| `delay` | Inject N ms of latency before the call returns. |
| `call_count_limit` | Fail the call once count crosses a per-function threshold. |
| `sandbox` | Deny file access by path deny-list at runtime. |
| `addr_deny` | Deny network connects to a list of IP addresses. |
| `filter` | Run the next actions only if a param matches an operator/value. |
| `decode_http` | Parse a buffer as HTTP/1.x and log method/path/status. |
| `decode_dns` | Parse a buffer as DNS wire format and log id/qname/qtype/answers. |

## Environment variables

Compact form. For the full CLI and env var reference, see
[cli.md](../cli.md#common-options).

| Variable | Effect |
|----------|--------|
| `RETRACE_JSON_CONFIG` | Path to the JSON config file. |
| `RETRACE_LOGGER_DEF_ENA` | `0` disables logging entirely (interception still runs). |
| `RETRACE_LOGGER_DEF_STDOUT_ENA` | `0` suppresses stdout log output (use with `--log`). |
| `RETRACE_LOGGER_DEF_FN` | Path to a log file (JSON appended). |
| `RETRACE_LOGGER_ALLOWED_FUNCS` | Comma-separated allowlist of function names. |
| `RETRACE_LOGGER_EXCLUDED_FUNCS` | Comma-separated denylist of function names. |
| `RETRACE_LIB` | Override the path to the retrace shared library. |

## Platform notes

- **macOS**: SIP-protected binaries (`/usr/bin/*`) silently skip
  `DYLD_INSERT_LIBRARIES`. Copy the target to `/tmp/` first, or use
  `retrace run --` which spawns via `execvp`.
- **Windows**: `LD_PRELOAD` doesn't exist; retrace uses inline hooking
  via `CreateRemoteThread` (ADR-0009). The JSON config format is the
  same.
- **Static binaries** (Linux): use the `ptrace` backend; see
  `src/backends/ptrace/`.

## See also

- [README.adoc](../../README.adoc) — platform support matrix, install.
- [examples/](../../examples/) — fully worked demos.
- [docs/adr/](../adr/) — architecture decisions.
