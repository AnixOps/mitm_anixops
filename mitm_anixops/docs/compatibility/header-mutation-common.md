# Header Mutation Common Source Contract

Capability: request and response header mutation.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for the common header mutation
subset. It covers parser and policy-core output for bounded header operations;
it does not claim platform HTTP parser, unbounded header map, or network
serialization behavior.

## Input Forms

The current common subset accepts:

- `[Header Rewrite]`, `[Remote Header Rewrite]`, `[Rewrite]`, and
  `[URL Rewrite]` section aliases;
- request header actions: `header-add`, `header-replace`, `header-del`, and
  `header-replace-regex`;
- response header actions: `response-header-add`, `response-header-replace`,
  `response-header-del`, and `response-header-replace-regex`;
- URL capture expansion in non-regex replacement values;
- current-header-value regex replacement for regex actions.

## Parser Output

The parser must produce:

- accepted diagnostics for supported header mutation lines;
- rejected diagnostics for invalid URL or header regexes;
- request or response phase header rewrite rules observable through the public
  ABI.

## Positive Case

Parser case:

```text
tests/fixtures/HeaderMutation.Common.conf
tests/fixtures/HeaderResponseMutation.Common.conf
tests/fixtures/Loon.HeaderMutation.plugin
tests/fixtures/QuantumultX.HeaderAdd.snippet
tests/fixtures/QuantumultX.HeaderReplace.snippet
tests/fixtures/QuantumultX.ResponseHeaderAdd.snippet
tests/fixtures/QuantumultX.ResponseHeaderReplace.snippet
tests/fixtures/QuantumultX.ResponseHeaderDelete.snippet
tests/fixtures/QuantumultX.HeaderMutation.snippet
tests/fixtures/QuantumultX.RequestHeaderMutation.snippet
tests/fixtures/QuantumultX.HeaderDelete.snippet
tests/fixtures/Surge.HeaderMutation.sgmodule
```

Expected behavior:

- config load succeeds;
- five header mutation rules are registered;
- request header add, replace, and delete behavior are observable through
  `anixops_rewrite_evaluate_header`;
- response header regex replacement and delete behavior are observable through
  `anixops_rewrite_evaluate_header`;
- portable response header add and replace behavior are observable through
  `anixops_rewrite_evaluate_header`;
- Loon `[Header Rewrite]` request and response header mutation actions are
  observable through `anixops_rewrite_evaluate_header`;
- Quantumult X `url header-add` request header mutation is observable through
  `anixops_rewrite_evaluate_header`;
- Quantumult X `url header-replace` request header mutation is observable
  through `anixops_rewrite_evaluate_header`;
- Quantumult X `url response-header-add` response header mutation is observable
  through `anixops_rewrite_evaluate_header`;
- Quantumult X `url response-header-replace` response header mutation is
  observable through `anixops_rewrite_evaluate_header`;
- Quantumult X `url response-header-del` response header deletion is
  observable through `anixops_rewrite_evaluate_header`;
- Quantumult X `url response-header-replace-regex` response header mutation is
  observable through `anixops_rewrite_evaluate_header`;
- Quantumult X `url header-replace-regex` request header mutation is observable
  through `anixops_rewrite_evaluate_header`;
- Quantumult X `url header-del` request header deletion is observable through
  `anixops_rewrite_evaluate_header`;
- Surge `[URL Rewrite]` request and response header mutation actions are
  observable through `anixops_rewrite_evaluate_header`;
- phase separation prevents request header rules from matching response phase
  evaluation.

## Negative Case

Parser case:

```text
tests/fixtures/HeaderMutation.Common.Malformed.conf
tests/fixtures/HeaderResponseMutation.Common.Malformed.conf
tests/fixtures/Loon.HeaderMutation.Malformed.plugin
tests/fixtures/QuantumultX.HeaderAdd.Malformed.snippet
tests/fixtures/QuantumultX.HeaderReplace.Malformed.snippet
tests/fixtures/QuantumultX.ResponseHeaderAdd.Malformed.snippet
tests/fixtures/QuantumultX.ResponseHeaderReplace.Malformed.snippet
tests/fixtures/QuantumultX.ResponseHeaderDelete.Malformed.snippet
tests/fixtures/QuantumultX.HeaderMutation.Malformed.snippet
tests/fixtures/QuantumultX.RequestHeaderMutation.Malformed.snippet
tests/fixtures/QuantumultX.HeaderDelete.Malformed.snippet
tests/fixtures/Surge.HeaderMutation.Malformed.sgmodule
```

Expected behavior:

- the invalid header regex rejects config load;
- the malformed portable response header add rule without a header name is
  rejected under `ANIXOPS_COMPAT_LOON_STRICT`;
- the invalid Loon `[Header Rewrite]` header regex rejects config load;
- the malformed Quantumult X `url header-add` rule without a header name is
  rejected under `ANIXOPS_COMPAT_QUANTUMULTX_STRICT`;
- the malformed Quantumult X `url header-replace` rule without a header name is
  rejected under `ANIXOPS_COMPAT_QUANTUMULTX_STRICT`;
- the malformed Quantumult X `url response-header-add` rule without a header
  name is rejected under `ANIXOPS_COMPAT_QUANTUMULTX_STRICT`;
- the malformed Quantumult X `url response-header-replace` rule without a
  header name is rejected under `ANIXOPS_COMPAT_QUANTUMULTX_STRICT`;
- the malformed Quantumult X `url response-header-del` rule without a header
  name is rejected under `ANIXOPS_COMPAT_QUANTUMULTX_STRICT`;
- the invalid Quantumult X `url response-header-replace-regex` pattern rejects
  config load;
- the invalid Quantumult X `url header-replace-regex` pattern rejects config
  load;
- the malformed Quantumult X `url header-del` rule without a header name is
  rejected under `ANIXOPS_COMPAT_QUANTUMULTX_STRICT`;
- the invalid Surge `[URL Rewrite]` response header regex rejects config load;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers bounded policy-core header mutation decisions.

It does not implement:

- HTTP parsing, request serialization, or network I/O;
- unbounded platform header map behavior;
- hop-by-hop header filtering;
- cookie store semantics;
- streaming response framing;
- TLS interception or certificate lifecycle.

Those remain adapter, runner, or separate policy contracts.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/header_mutation_common_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/header_mutation_common_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/header_response_mutation_common_fixture_maps_response_header_add_replace`;
- `tests/test_config.c` registers
  `config/header_response_mutation_common_strict_fixture_rejects_missing_header_name`;
- `tests/test_config.c` registers
  `config/loon_header_mutation_fixture_maps_header_rewrites`;
- `tests/test_config.c` registers
  `config/loon_header_mutation_malformed_fixture_rejects_invalid_header_regex`;
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
- `tests/test_config.c` registers
  `config/surge_header_mutation_fixture_maps_header_rewrites`;
- `tests/test_config.c` registers
  `config/surge_header_mutation_malformed_fixture_rejects_invalid_header_regex`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
header mutation
```

The row remains `partial` until adapter-owned header map behavior, broader
ecosystem grammar, and full cookie/header edge-case corpus are covered.
