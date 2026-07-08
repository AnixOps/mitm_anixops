# Response Rewrite Common Source Contract

Capability: response rewrite.

Ecosystem: `portable`, `loon`, `quantumultx`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for the common response rewrite
subset. It covers parser and policy-core response-phase rewrite decisions for
already-buffered bodies. It does not claim production HTTP framing, streaming,
compression, or platform network behavior.

## Input Forms

The current common subset accepts:

- `[Rewrite]`, `[URL Rewrite]`, `[Remote Rewrite]`, and Quantumult X
  `[rewrite_local]` section aliases;
- response mock body actions such as `mock-response-body`;
- Quantumult X style `url echo-response content-type body` forms;
- response body regex replacement actions such as
  `response-body-replace-regex pattern replacement`;
- URL capture expansion for mock/echo response bodies.

## Parser Output

The parser must produce:

- accepted diagnostics for supported response rewrite lines;
- rejected diagnostics for invalid URL or body regexes;
- response-phase rewrite rules observable through the public ABI.

## Positive Case

Parser case:

```text
tests/fixtures/ResponseRewrite.Common.conf
tests/fixtures/Loon.ResponseRewrite.plugin
```

Expected behavior:

- config load succeeds;
- the common fixture registers three rewrite rules;
- the Loon fixture registers two rewrite rules;
- request phase does not trigger response rewrite rules;
- response mock body and echo-response behavior are observable through
  `anixops_rewrite_apply_body`;
- response body regex replacement is observable for an already-buffered body.
- Loon `[URL Rewrite]` response echo and response body regex rules are
  observable through `anixops_rewrite_apply_body`.

## Negative Case

Parser case:

```text
tests/fixtures/ResponseRewrite.Common.Malformed.conf
tests/fixtures/Loon.ResponseRewrite.Malformed.plugin
```

Expected behavior:

- the invalid response body regex rejects config load;
- the invalid Loon `[URL Rewrite]` response body regex rejects config load;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers bounded policy-core response rewrite decisions.

It does not implement:

- HTTP parser, response serialization, or network I/O;
- response streaming or partial-body mutation;
- gzip, deflate, brotli, or zstd handling;
- charset conversion;
- production JavaScript execution;
- TLS interception or certificate lifecycle.

Those remain adapter, runner, or separate body/script runtime contracts.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/response_rewrite_common_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/response_rewrite_common_fixture_rejects_invalid_body_regex`;
- `tests/test_config.c` registers
  `config/loon_response_rewrite_fixture_maps_response_echo_and_body`;
- `tests/test_config.c` registers
  `config/loon_response_rewrite_malformed_fixture_rejects_invalid_body_regex`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
response rewrite
```

The row remains `partial` until streaming, compression/framing, broader body
rewrite corpus, and production adapter behavior are covered.
