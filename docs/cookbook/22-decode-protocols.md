# 22 — Decode protocols (HTTP, DNS)

## Problem

You captured every `send` / `recv` / `sendto` / `recvfrom` call, but
the log is full of raw byte buffers. To debug an HTTP client or DNS
resolver you have to mentally parse the wire format for each entry.

The `decode_http` and `decode_dns` actions parse the buffer of a
named param up front and log the decoded fields (method, path,
status, qname, qtype, etc.) as a structured log line.

## Config

### HTTP — `decode-http.json`

```json
{
  "intercept_scripts": [
    {
      "func_name": "send",
      "actions": [
        { "action_name": "decode_http",
          "action_params": { "param_name": "buf" } },
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "recv",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "decode_http",
          "action_params": { "param_name": "buf" } },
        { "action_name": "log_params" }
      ]
    }
  ]
}
```

Note the ordering for `recv`: `decode_http` runs **after** `call_real`
so the buffer contains the response the kernel just wrote.

### DNS — `decode-dns.json`

```json
{
  "intercept_scripts": [
    {
      "func_name": "sendto",
      "actions": [
        { "action_name": "decode_dns",
          "action_params": { "param_name": "buf" } },
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "recvfrom",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "decode_dns",
          "action_params": { "param_name": "buf" } },
        { "action_name": "log_params" }
      ]
    }
  ]
}
```

## Invocation

```sh
$ retrace run --config cookbook/decode-http.json -- curl -sS https://example.com
$ retrace run --config cookbook/decode-dns.json  -- dig example.com A
```

## Expected output

HTTP (excerpt):

```
decode_http: GET / HTTP/1.1 (request)
decode_http: HTTP/1.1 200 OK (status=200)
```

DNS (excerpt):

```
decode_dns: query  id=0x4f3a qname=example.com qtype=A
decode_dns: response id=0x4f3a qname=example.com qtype=A answers=1
```

If the buffer does not contain HTTP/DNS data (e.g. TLS handshake
bytes), the action silently returns 0 — no abort, no spurious log
lines.

## Variations

- **Decode both ends of a proxy**: apply `decode_http` to `send` on
  the upstream and to `recv` on the downstream in the same config.
- **Combine with `filter`**: only decode HTTP responses with status
  ≥ 500:
  ```json
  {
    "func_name": "recv",
    "actions": [
      { "action_name": "call_real" },
      { "action_name": "decode_http",
        "action_params": { "param_name": "buf" } },
      { "action_name": "filter",
        "action_params": { "param_name": "status_code",
                           "op": ">=", "value": 500 } },
      { "action_name": "log_params" }
    ]
  }
  ```
- **Capture for offline decode**: log `buf` to JSON (via `log_params`)
  and post-process with the OTLP converter
  (`tools/retrace-to-otlp`) for ingestion into Loki / Honeycomb /
  Datadog.

## Caveats

- The decoders read the first line / first question only. Header
  parsing for HTTP/2 and DNS-over-HTTPS (which are binary, framed,
  and typically encrypted via TLS) is out of scope — intercept the
  TLS layer if you need those.
- For TLS-encrypted HTTP, `decode_http` will see ciphertext and
  silently no-op. Pair with `mock_ssl_verify` (recipe 21) if you
  want plaintext without crypto handshake overhead, or attach a
  key-log to extract session keys.

## See also

- Recipe 15 — Capture network traffic (raw byte logging)
- Recipe 21 — Mock OpenSSL verify (for plaintext TLS interception)
- TODO.complete/23 — Protocol decoders roadmap
