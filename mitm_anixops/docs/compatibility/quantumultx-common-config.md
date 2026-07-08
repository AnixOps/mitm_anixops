# Quantumult X Common Config Source Contract

Capability: Quantumult X rewrite and MITM common config.

Ecosystem: `quantumultx`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser milestone for the high-frequency Quantumult X
rewrite and MITM subset. Local cron and event task parser metadata is covered
separately by [Quantumult X Task Metadata](quantumultx-task-metadata.md);
scheduler, event dispatch, and runtime behavior remain adapter-owned.

## Input Forms

The current common-config subset accepts:

- `#!name` and other `#!` metadata lines as tolerated diagnostics;
- `[rewrite_local]`, `#[rewrite_local]`, `[rewrite_remote]`, and
  `#[rewrite_remote]` section aliases;
- URL rewrite lines using the `url` prefix for redirects and reject variants;
- URL-prefixed request/response header mutation rules;
- URL-prefixed request/response script trigger rules;
- `[mitm]` and `#[mitm]` section aliases;
- `hostname`, `force-http-engine-hosts`, `skip-server-cert-verify`, `h2`,
  and QUIC fallback options covered by
  [Quantumult X MITM Options](quantumultx-mitm-options.md);
- `#[task_local]` cron and event descriptor lines covered by
  [Quantumult X Task Metadata](quantumultx-task-metadata.md).

## Parser Output

The parser must produce:

- accepted diagnostics for supported rewrite, script, and MITM lines;
- ignored diagnostics for tolerated metadata or unsupported non-rule lines;
- rejected diagnostics for malformed supported-section rules under
  `ANIXOPS_COMPAT_QUANTUMULTX_STRICT`;
- URL rewrite rules, header rewrite rules, script trigger rules, task
  descriptors, and MITM host patterns observable through the public ABI.

## Positive Case

Parser case:

```text
tests/fixtures/QuantumultX.CommonConfig.snippet
```

Expected behavior:

- config load succeeds;
- three rewrite rules are registered;
- two script rules are registered;
- four MITM host patterns are registered;
- host-list `skip-server-cert-verify` is exposed to adapters;
- redirect, reject, header mutation, request script, response script, and MITM
  decisions are observable through public ABI calls.

## Negative Case

Parser case:

```text
tests/fixtures/QuantumultX.CommonConfig.Malformed.snippet
```

Expected behavior:

- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects the malformed rewrite line;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports parse failure at the malformed line.

## Task And Cron Boundary

`#[task_local]` cron and event lines are parser metadata only. They are
documented and tested in
[Quantumult X Task Metadata](quantumultx-task-metadata.md), but the following
remain unimplemented:

- task scheduler/runtime dispatch semantics;
- event dispatch runtime behavior;
- JavaScript task bindings;
- permission, concurrency, and cancellation policy.

## Runtime And Security Boundary

This contract covers parser and policy-core output only.

It does not implement:

- TLS interception;
- certificate generation or trust installation;
- HTTP parser or body streaming;
- JavaScript execution;
- remote script download or cache refresh;
- scheduled task execution.

Those remain runner, runtime, scheduler, or adapter responsibilities.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/quantumultx_common_config_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/quantumultx_common_config_strict_fixture_rejects_malformed_rule`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Quantumult X rewrite/task/mitm common config
```

The row remains `partial` because rewrite and MITM are covered by this
contract, while task scheduling/runtime behavior remains adapter-owned.
