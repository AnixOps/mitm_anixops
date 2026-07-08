# Loon Script Metadata Source Contract

Capability: Loon `[Script]` request/response metadata.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for Loon-style HTTP script metadata.
The policy core selects request and response script rules and exposes dispatch
metadata; it does not execute JavaScript or fetch remote script assets.

## Input Forms

The parser accepts script rules inside `[Script]`:

```text
[Script]
http-request ^https:\/\/api\.example\/v1 requires-body=1, timeout=2, max-size=4096, script-path=https://scripts.example/request.js, tag=request.tag, argument=Mode=request
http-response ^https:\/\/api\.example\/v1 requires-body=0, timeout-ms=750, max_size=2048, script-path=https://scripts.example/response.js, tag=response.tag, argument=Mode=response
```

Supported fields:

- `http-request` and `http-response` rule kinds;
- URL regex pattern;
- `script-path`;
- `tag`;
- `argument`;
- `requires-body` and `requires_body`;
- `timeout` seconds and `timeout-ms`/`timeout_ms` milliseconds;
- `max-size` and `max_size`;
- `enable` and `enabled` dispatch flags.

## Parser Output

For valid script lines, the parser must:

- register one script rule per supported line;
- emit an accepted diagnostic with section `Script`, action `script`, and
  message `script rule accepted`;
- expose phase, script path, tag, resolved argument, `requires_body`,
  `timeout_ms`, and `max_size` through `anixops_script_evaluate_url`;
- skip disabled script rules during dispatch without deleting the rule.

Malformed non-empty script lines without a valid `script-path` are rejected by
strict compatibility profiles.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.ScriptMetadata.plugin
```

Expected behavior:

- config load succeeds;
- three script rules are registered;
- request and response rules expose script metadata through the public ABI;
- a disabled request rule does not dispatch;
- the fixture MITM hostname still parses as a normal host pattern.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.ScriptMetadata.Malformed.plugin
```

Expected behavior:

- `ANIXOPS_COMPAT_LOON_STRICT` rejects the malformed script line;
- no script rules or MITM host patterns are registered after the failure;
- a rejected diagnostic is recorded with section `Script`, action `script`, and
  strict-profile rejection context.

## Runtime And Security Boundary

Loon script metadata must not:

- execute JavaScript inside the C policy core;
- download, cache, or validate remote script assets;
- mutate request or response objects by itself;
- bypass timeout, max-size, body availability, or user permission policy owned
  by the adapter/runtime;
- alter MITM, certificate, routing, or trust behavior.

Runtime execution is covered by
[Script Runtime Common Source Contract](script-runtime-common.md).

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_script_metadata_fixture_exposes_dispatch_fields`;
- `tests/test_config.c` registers
  `config/loon_script_metadata_malformed_fixture_rejects_missing_path`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon script metadata
```

The row remains `partial` because script dispatch metadata is covered while
JavaScript execution, remote script fetching, and runtime policy remain adapter
owned.
