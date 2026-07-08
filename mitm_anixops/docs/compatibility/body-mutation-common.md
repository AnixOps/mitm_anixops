# Body Mutation Common Source Contract

Capability: request and response body mutation.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for the common body mutation
subset. It covers parser and policy-core behavior for already-buffered plain
text or JSON bodies. It does not claim streaming, compression, charset
conversion, or production HTTP pipeline behavior.

## Input Forms

The current common subset accepts:

- `[Body Rewrite]`, `[Remote Body Rewrite]`, `[Rewrite]`, and `[URL Rewrite]`
  section aliases;
- request body regex replacement actions such as
  `request-body-replace-regex pattern replacement`;
- response body regex replacement actions such as
  `response-body-replace-regex pattern replacement`;
- request and response JSON replacement actions such as
  `request-body-json-replace $.enabled true`;
- selected JQ action tokens are parsed by the policy core, but JQ runtime
  behavior is not part of this contract.

## Parser Output

The parser must produce:

- accepted diagnostics for supported body mutation lines;
- rejected diagnostics for invalid URL or body regexes;
- request or response phase body mutation rules observable through the public
  ABI.

## Positive Case

Parser case:

```text
tests/fixtures/BodyMutation.Common.conf
tests/fixtures/QuantumultX.BodyMutation.snippet
tests/fixtures/Surge.BodyMutation.sgmodule
```

Expected behavior:

- config load succeeds;
- three body mutation rules are registered;
- request body regex mutation is observable through `anixops_rewrite_apply_body`;
- response body regex mutation is observable through
  `anixops_rewrite_apply_body`;
- request JSON body mutation is observable through `anixops_rewrite_apply_body`;
- Quantumult X `url response-body-replace-regex` response body mutation is
  observable through `anixops_rewrite_apply_body`;
- Surge `[URL Rewrite]` `response-body-replace-regex` response body mutation
  is observable through `anixops_rewrite_apply_body`;
- phase separation prevents request body rules from matching response phase
  evaluation.

## Negative Case

Parser case:

```text
tests/fixtures/BodyMutation.Common.Malformed.conf
tests/fixtures/QuantumultX.BodyMutation.Malformed.snippet
tests/fixtures/Surge.BodyMutation.Malformed.sgmodule
```

Expected behavior:

- the invalid body regex rejects config load;
- the invalid Quantumult X `url response-body-replace-regex` body regex rejects
  config load;
- the invalid Surge `[URL Rewrite]` response body regex rejects config load;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers bounded policy-core body mutation decisions.

It does not implement:

- HTTP parser, request/response serialization, or network I/O;
- streaming or partial-body mutation;
- gzip, deflate, brotli, or zstd handling;
- charset conversion;
- production JQ runtime limits beyond existing fail-open policy;
- production JavaScript execution;
- TLS interception or certificate lifecycle.

Those remain adapter, runner, JQ, or script runtime contracts.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/body_mutation_common_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/body_mutation_common_fixture_rejects_invalid_body_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/surge_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/surge_body_mutation_malformed_fixture_rejects_invalid_regex`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
body mutation
```

The row remains `partial` until streaming, compression/framing, charset matrix,
broader JSON/JQ corpus, and production adapter behavior are covered.
