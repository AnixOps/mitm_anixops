# Surge Module Common Config Source Contract

Capability: Surge module common config.

Ecosystem: `surge`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser milestone for the high-frequency Surge module
subset. It does not claim full Surge module compatibility.

## Input Forms

The current common-config subset accepts:

- `#!name`, `#!desc`, `#!arguments-desc`, and `#!requirement` metadata lines as
  tolerated diagnostics, with requirement metadata covered by
  [Surge Requirement Metadata](surge-requirement-metadata.md);
- `#!arguments = Feature:true,Mode:value` inline argument defaults;
- `[URL Rewrite]` URL redirect and reject rules;
- `[Script]` attr-list rules with `type`, `pattern`, `requires-body`,
  `timeout`, `timeout-ms`, `max-size`, `max_size`, `script-path`, and
  `argument`;
- `[Script]` cron and interval task metadata covered by
  [Surge Task Metadata](surge-task-metadata.md);
- `[MITM] hostname = ...` with exact, deny, `%APPEND%`, and `%INSERT%` host
  entries.

## Parser Output

The parser must produce:

- accepted diagnostics for supported arguments, rewrite rules, script rules, and
  MITM host entries;
- ignored diagnostics for tolerated module metadata;
- rejected diagnostics for malformed supported-section rules under
  `ANIXOPS_COMPAT_SURGE_STRICT`;
- argument defaults, rewrite rules, script trigger metadata, and MITM host
  patterns observable through the public ABI.

## Positive Case

Parser case:

```text
tests/fixtures/Surge.CommonConfig.sgmodule
```

Expected behavior:

- config load succeeds;
- two inline arguments are registered;
- two rewrite rules are registered;
- two script rules are registered;
- three MITM host patterns are registered;
- module metadata is tolerated and recorded as diagnostics;
- redirect, reject, request script, response script, argument resolution, and
  MITM allow/deny behavior are observable through public ABI calls.

## Negative Case

Parser case:

```text
tests/fixtures/Surge.CommonConfig.Malformed.sgmodule
```

Expected behavior:

- `ANIXOPS_COMPAT_SURGE_STRICT` rejects the malformed script line;
- a rejected rule diagnostic is recorded with section `Script` and action
  `script`;
- last error reports parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers parser and policy-core output only.

It does not implement:

- TLS interception;
- certificate generation or trust installation;
- HTTP parser or body streaming;
- JavaScript execution;
- remote script download or cache refresh;
- Surge requirement runtime gating;
- scheduler/runtime execution for task descriptors.

Those remain runner, runtime, scheduler, or adapter responsibilities.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/surge_common_config_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/surge_common_config_strict_fixture_rejects_malformed_rule`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Surge module common config
```

The row remains `partial` until broader module grammar, requirement runtime
gating behavior, body/JQ corpus coverage, and scheduler/runtime behavior exist.
Dedicated task descriptor parser evidence lives in the `Surge task metadata`
row.
