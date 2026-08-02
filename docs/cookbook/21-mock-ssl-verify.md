# 21 — Mock OpenSSL certificate verification

## Problem

You're testing a client that connects to a TLS server. You want to
verify how it handles different certificate verification outcomes
(valid, expired, wrong host, self-signed, revoked) without standing
up six different servers. retrace can force OpenSSL's verification
result to any value you choose.

## Config

`mock-ssl-verify.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "SSL_get_verify_result",
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": 10 } }
      ]
    }
  ]
}
```

`retval_int` is the X509_V_* error code. Common values:

| Code | Constant                    | Meaning                                  |
|------|-----------------------------|------------------------------------------|
| 0    | X509_V_OK                   | Certificate is valid (success).          |
| 2    | X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT | Issuer cert not found.            |
| 9    | X509_V_ERR_CERT_NOT_YET_VALID | Certificate's not-before date is future. |
| 10   | X509_V_ERR_CERT_HAS_EXPIRED | Certificate's not-after date is past.    |
| 18   | X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT | Self-signed root in chain.     |
| 24   | X509_V_ERR_INVALID_CA       | CA certificate is invalid.               |

Without `call_real`, the real `SSL_get_verify_result` never runs —
the caller immediately sees the synthesized code.

## Invocation

```sh
$ retrace run --config docs/cookbook/mock-ssl-verify.json -- ./your-client https://example.com
SSL_get_verify_result() → 10 (certificate has expired)
your-client: error: certificate verification failed
```

## Variations

### Test the happy path

Force success even for a self-signed cert:

```json
{ "action_name": "modify_return_value_int",
  "action_params": { "retval_int": 0 } }
```

### Mock SSL_CTX_set_verify instead

Some clients install a custom verify callback via `SSL_CTX_set_verify`.
The callback receives a `X509_STORE_CTX` pointer; mocking its return
is more involved and may require a custom action. For most clients,
mocking `SSL_get_verify_result` is sufficient.

### BoringSSL / LibreSSL / mbed TLS

retrace intercepts symbols by name. If the binary links BoringSSL,
the same function name (`SSL_get_verify_result`) is intercepted.
mbed TLS uses different names (`mbedtls_ssl_get_verify_result`); add
those to the script.

## How it works

`SSL_get_verify_result` returns a `long` (X509_V_*). retrace's
`modify_return_value_int` overrides it. The TLS handshake still
completes (or fails) on its own merits; only the post-handshake
verification code is overridden. So if the handshake itself fails
(e.g., cipher mismatch), this mock does not help — the client never
calls `SSL_get_verify_result`.

For broader SSL mocking — including handshake-level interception —
consider tracing `SSL_do_handshake`, `SSL_connect`, and the
underlying `read`/`write` on the socket fd.

## See also

- [12 — Fail specific syscalls](12-fail-specific.md) — same pattern,
  applied to libc functions.
- [05 — Mock `getuid()` for root checks](05-mock-getuid.md) — same
  pattern, applied to a different function.
