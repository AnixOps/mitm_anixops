# Surge Module Common Config Source Contract

Capability: Surge module common config.

Ecosystem: `surge`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser milestone for the high-frequency Surge module
subset. It does not claim full Surge module compatibility.

Reference manual:
<https://manual.nssurge.com/http-processing/mitm.html>

## Input Forms

The current common-config subset accepts:

- `#!name`, `#!desc`, `#!arguments-desc`, and `#!requirement` metadata lines as
  tolerated diagnostics, with requirement metadata covered by
  [Surge Requirement Metadata](surge-requirement-metadata.md);
- `#!arguments = Feature:true,Mode:value` inline argument defaults;
- `[URL Rewrite]` URL redirect and reject rules;
- `[URL Rewrite]` response body regex and JSON mutation rules;
- `[URL Rewrite]` request and response header mutation rules;
- `[Script]` attr-list rules with `type`, `pattern`, `requires-body`,
  `timeout`, `timeout-ms`, `max-size`, `max_size`, `script-path`, and
  `argument`;
- `[Script]` cron and interval task metadata covered by
  [Surge Task Metadata](surge-task-metadata.md);
- `[MITM] hostname = ...` with exact, deny, `%APPEND%`, and `%INSERT%` host
  entries;
- `[MITM] ca-p12` and `ca-passphrase` as unsupported certificate-material
  diagnostics only.

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
tests/fixtures/Surge.RequestRewrite.sgmodule
tests/fixtures/Surge.ResponseRewrite.sgmodule
tests/fixtures/Surge.BodyMutation.sgmodule
tests/fixtures/Surge.BodyJsonMutation.sgmodule
tests/fixtures/Surge.HeaderMutation.sgmodule
```

Expected behavior:

- config load succeeds;
- two inline arguments are registered;
- two rewrite rules are registered;
- two URL redirect/reject rules are registered in the dedicated request-rewrite
  fixture;
- one response body regex rewrite rule is registered in the dedicated response
  rewrite fixture;
- one response body regex mutation rule is registered in the dedicated body
  mutation fixture;
- one response body JSON replacement rule is registered in the dedicated body
  mutation fixture;
- eight request/response header mutation rules are registered in the dedicated
  header mutation fixture;
- two script rules are registered;
- three MITM host patterns are registered;
- module metadata is tolerated and recorded as diagnostics;
- redirect, reject, response body regex mutation, response body JSON mutation,
  request script, response script, header mutation, argument resolution, and
  MITM allow/deny behavior are observable through public ABI calls.

## Negative Case

Parser case:

```text
tests/fixtures/Surge.CommonConfig.Malformed.sgmodule
tests/fixtures/Surge.RequestRewrite.Malformed.sgmodule
tests/fixtures/Surge.ResponseRewrite.Malformed.sgmodule
tests/fixtures/Surge.BodyMutation.Malformed.sgmodule
tests/fixtures/Surge.BodyJsonMutation.Malformed.sgmodule
tests/fixtures/Surge.HeaderMutation.Malformed.sgmodule
```

Expected behavior:

- `ANIXOPS_COMPAT_SURGE_STRICT` rejects the malformed script line;
- an invalid URL rewrite regex rejects config load;
- an invalid response body regex rejects config load;
- `ANIXOPS_COMPAT_SURGE_STRICT` rejects a malformed
  `response-body-json-replace` line without a JSON path and replacement value;
- an invalid response header regex rejects config load;
- the malformed script line records a rejected diagnostic with section `Script`
  and action `script`;
- the invalid response body regex records a rejected diagnostic with section
  `Rewrite` and action `rewrite`;
- last error reports parse failure at the malformed line.

Unsupported certificate-material parser case:

```text
tests/fixtures/Surge.MitmCertificateUnsupported.sgmodule
```

Expected behavior:

- config load succeeds because the known host policy remains valid;
- `ca-p12` and `ca-passphrase` record ignored diagnostics;
- certificate material does not change the adapter-supplied trust state;
- a matching host remains a conservative MITM bypass until the adapter supplies
  a trusted certificate state.

## Runtime And Security Boundary

This contract covers parser and policy-core output only.

It does not implement:

- TLS interception;
- certificate generation or trust installation;
- p12 material loading or passphrase handling;
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
- `tests/test_config.c` registers
  `config/surge_request_rewrite_fixture_maps_redirect_and_reject`;
- `tests/test_config.c` registers
  `config/surge_request_rewrite_malformed_fixture_rejects_invalid_url_regex`;
- `tests/test_config.c` registers
  `config/surge_response_rewrite_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/surge_response_rewrite_malformed_fixture_rejects_invalid_body_regex`;
- `tests/test_config.c` registers
  `config/surge_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/surge_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/surge_body_json_mutation_fixture_maps_response_body_json_replace`;
- `tests/test_config.c` registers
  `config/surge_body_json_mutation_malformed_fixture_rejects_missing_json_path`;
- `tests/test_config.c` registers
  `config/surge_header_mutation_fixture_maps_header_rewrites`;
- `tests/test_config.c` registers
  `config/surge_header_mutation_malformed_fixture_rejects_invalid_header_regex`;
- `tests/test_config.c` registers
  `config/surge_mitm_certificate_unsupported_fixture_keeps_material_ignored`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Surge module common config
```

The row remains `partial` until broader module grammar, requirement runtime
gating behavior, certificate lifecycle, broader body/header/JQ corpus coverage,
and scheduler/runtime behavior exist. Dedicated task descriptor parser evidence
lives in the `Surge task metadata` row.
