# Compatibility Source Contracts

Source contracts define expected behavior before implementation. A feature is
not ready for implementation until its source contract states inputs, outputs,
errors, and CI evidence.

## Contract Template

Each new capability contract must include:

```text
Capability:
Ecosystem: loon | quantumultx | surge | stash | shadowrocket | portable
Input form:
Parser output:
Positive case:
Negative case:
Runtime/matching behavior:
Diagnostics:
Security boundary:
CI evidence:
Compatibility matrix row:
Unimplemented items:
```

## Current Baseline Contracts

### Loon Plugin Common Subset

Detailed contract: [Loon Plugin Common Fields Source Contract](loon-plugin-common-fields.md).

Capability: parse and evaluate the currently implemented Loon/AnixOps subset.

Input form:

- `[MITM] hostname = ...`
- `[Rewrite]`, `[URL Rewrite]`, `[Header Rewrite]`, `[Body Rewrite]`
- `[Script]`
- `[Argument]`
- `[Plugin]` metadata tolerance

Parser output:

- MITM host config;
- URL/header/body rewrite rules;
- script dispatch metadata;
- argument defaults and overrides;
- accepted, ignored, and rejected diagnostics.

Current CI evidence:

- C parser and rewrite tests under `tests/`;
- runner corpus entry `Representative.Loon.plugin`;
- BiliUniverse Loon fixture in `tests/fixtures/corpus/manifest.json`;
- GitHub Actions `linux-test` job running `sh scripts/check.sh`.

Unimplemented items:

- complete Loon grammar;
- broader real plugin corpus;
- cron/task trigger contract;
- platform JS runtime beyond Alpha Node runner.

### Quantumult X Common Subset

Capability: parse and evaluate the currently implemented Quantumult X subset.

Input form:

- `#[rewrite_local]`
- `#[rewrite_remote]`
- `#[mitm]`
- `url`-prefixed rewrite/script/header/body forms;
- `force-http-engine-hosts`;
- host-list `skip-server-cert-verify`.

Current CI evidence:

- C parser/script/rewrite tests;
- runner corpus entry `Representative.QuantumultX.snippet`.

Unimplemented items:

- task/cron behavior;
- full rewrite/task grammar;
- broader corpus and migration notes.

### Surge Module Common Subset

Capability: parse and evaluate the currently implemented Surge module subset.

Input form:

- `#!` metadata diagnostics;
- `#!arguments`;
- `%APPEND%` and `%INSERT%`;
- `[Script]` attr-list fields including type, pattern, script-path,
  requires-body, timeout, max-size, tag, and argument;
- selected body/JQ rewrite forms.

Current CI evidence:

- C parser/script tests;
- runner corpus entry `Representative.Surge.sgmodule`;
- optional `JQ=1` GitHub Actions coverage when libjq headers are installed.

Unimplemented items:

- full Surge module grammar;
- broader body rewrite and JQ corpus;
- requirement metadata behavior.

### MITM Hostname Policy

Capability: evaluate whether a host is eligible for MITM and whether QUIC should
be rejected for fallback.

Input form:

- exact hostnames;
- wildcard/suffix hostnames;
- deny host entries;
- host with port;
- IPv6 literal;
- certificate state supplied by adapter.

Current CI evidence:

- `tests/test_mitm.c`;
- strategy-chain demo;
- runner and proxy E2E paths.

Unimplemented items:

- platform certificate installation;
- trust store mutation;
- platform-specific revocation and expiry probes.

### Script Trigger Metadata

Capability: select request/response script rules and expose script metadata.

Input form:

- Loon/AnixOps style script rules;
- Surge attr-list rules;
- Quantumult X url-prefixed script forms.

Current CI evidence:

- `tests/test_script.c`;
- runner replay with Node contract runner;
- script contract E2E;
- script exception and timeout fail-open coverage.

Unimplemented items:

- production embedded JS runtime;
- cron/task triggers;
- remote script cache refresh policy;
- memory limits.
