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
- URL-prefixed `echo-response` response body rules;
- URL-prefixed response body regex mutation rules;
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
tests/fixtures/QuantumultX.RequestRewrite.snippet
tests/fixtures/QuantumultX.EchoResponse.snippet
tests/fixtures/QuantumultX.BodyMutation.snippet
tests/fixtures/QuantumultX.HeaderAdd.snippet
tests/fixtures/QuantumultX.HeaderReplace.snippet
tests/fixtures/QuantumultX.ResponseHeaderAdd.snippet
tests/fixtures/QuantumultX.ResponseHeaderReplace.snippet
tests/fixtures/QuantumultX.ResponseHeaderDelete.snippet
tests/fixtures/QuantumultX.HeaderMutation.snippet
tests/fixtures/QuantumultX.RequestHeaderMutation.snippet
tests/fixtures/QuantumultX.HeaderDelete.snippet
```

Expected behavior:

- config load succeeds;
- the common fixture registers three rewrite rules;
- two `url` request redirect/reject rules are registered in the dedicated
  request-rewrite fixture;
- one `url echo-response` rule is registered in the dedicated echo-response
  fixture;
- one `url response-body-replace-regex` rule is registered in the dedicated
  body-mutation fixture;
- one `url header-add` rule is registered in the dedicated header-add fixture;
- one `url header-replace` rule is registered in the dedicated header-replace
  fixture;
- one `url response-header-add` rule is registered in the dedicated response
  header-add fixture;
- one `url response-header-replace` rule is registered in the dedicated
  response header-replace fixture;
- one `url response-header-del` rule is registered in the dedicated response
  header-delete fixture;
- one `url response-header-replace-regex` rule is registered in the dedicated
  header-mutation fixture;
- one `url header-replace-regex` rule is registered in the dedicated request
  header-mutation fixture;
- one `url header-del` rule is registered in the dedicated header-delete
  fixture;
- two script rules are registered;
- four MITM host patterns are registered;
- host-list `skip-server-cert-verify` is exposed to adapters;
- redirect, reject, `echo-response`, response body regex mutation, request
  header add, request header replace, request header regex mutation, request
  header delete, response header add, response header replace, response header
  delete, response header regex mutation, request script, response script, and
  MITM decisions are observable through public ABI calls.

## Negative Case

Parser case:

```text
tests/fixtures/QuantumultX.CommonConfig.Malformed.snippet
tests/fixtures/QuantumultX.RequestRewrite.Malformed.snippet
tests/fixtures/QuantumultX.EchoResponse.Malformed.snippet
tests/fixtures/QuantumultX.BodyMutation.Malformed.snippet
tests/fixtures/QuantumultX.HeaderAdd.Malformed.snippet
tests/fixtures/QuantumultX.HeaderReplace.Malformed.snippet
tests/fixtures/QuantumultX.ResponseHeaderAdd.Malformed.snippet
tests/fixtures/QuantumultX.ResponseHeaderReplace.Malformed.snippet
tests/fixtures/QuantumultX.ResponseHeaderDelete.Malformed.snippet
tests/fixtures/QuantumultX.HeaderMutation.Malformed.snippet
tests/fixtures/QuantumultX.RequestHeaderMutation.Malformed.snippet
tests/fixtures/QuantumultX.HeaderDelete.Malformed.snippet
```

Expected behavior:

- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects the malformed rewrite line;
- an invalid `url` request rewrite URL regex rejects config load;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects a malformed `echo-response`
  line without body content;
- an invalid `response-body-replace-regex` pattern rejects config load;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects a malformed `header-add` line
  without a header name;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects a malformed `header-replace` line
  without a header name;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects a malformed
  `response-header-add` line without a header name;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects a malformed
  `response-header-replace` line without a header name;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects a malformed
  `response-header-del` line without a header name;
- an invalid `response-header-replace-regex` pattern rejects config load;
- an invalid `header-replace-regex` pattern rejects config load;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects a malformed `header-del` line
  without a header name;
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
- `tests/test_config.c` registers
  `config/quantumultx_request_rewrite_fixture_maps_redirect_and_reject`;
- `tests/test_config.c` registers
  `config/quantumultx_request_rewrite_malformed_fixture_rejects_invalid_url_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_echo_response_fixture_maps_response_body`;
- `tests/test_config.c` registers
  `config/quantumultx_echo_response_malformed_fixture_rejects_missing_body`;
- `tests/test_config.c` registers
  `config/quantumultx_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_header_add_fixture_maps_request_header_add`;
- `tests/test_config.c` registers
  `config/quantumultx_header_add_malformed_fixture_rejects_missing_header_name`;
- `tests/test_config.c` registers
  `config/quantumultx_header_replace_fixture_maps_request_header_replace`;
- `tests/test_config.c` registers
  `config/quantumultx_header_replace_malformed_fixture_rejects_missing_header_name`;
- `tests/test_config.c` registers
  `config/quantumultx_response_header_add_fixture_maps_response_header_add`;
- `tests/test_config.c` registers
  `config/quantumultx_response_header_add_malformed_fixture_rejects_missing_header_name`;
- `tests/test_config.c` registers
  `config/quantumultx_response_header_replace_fixture_maps_response_header_replace`;
- `tests/test_config.c` registers
  `config/quantumultx_response_header_replace_malformed_fixture_rejects_missing_header_name`;
- `tests/test_config.c` registers
  `config/quantumultx_response_header_delete_fixture_maps_response_header_delete`;
- `tests/test_config.c` registers
  `config/quantumultx_response_header_delete_malformed_fixture_rejects_missing_header_name`;
- `tests/test_config.c` registers
  `config/quantumultx_header_mutation_fixture_maps_response_header_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_header_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_request_header_mutation_fixture_maps_request_header_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_request_header_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_header_delete_fixture_maps_request_header_delete`;
- `tests/test_config.c` registers
  `config/quantumultx_header_delete_malformed_fixture_rejects_missing_header_name`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Quantumult X rewrite/task/mitm common config
```

The row remains `partial` because rewrite and MITM parser/policy-core behavior
are covered for the documented subset, while task scheduling/runtime behavior,
HTTP serialization, content-type writeback, and streaming remain adapter-owned.
