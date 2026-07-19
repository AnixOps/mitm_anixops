# Body Mutation Common Source Contract

Capability: request and response body mutation.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`, `shadowrocket`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for the common body mutation
subset. It covers parser and policy-core behavior for already-buffered plain
text or JSON bodies. It does not claim streaming, compression, charset
conversion, or production HTTP pipeline behavior.

## Input Forms

The current common subset accepts:

- `[Body Rewrite]`, `[Remote Body Rewrite]`, `[Rewrite]`, and `[URL Rewrite]`
  section aliases, including Shadowrocket-style `[URL Rewrite]` body mutation
  lines;
- request body regex replacement actions such as
  `request-body-replace-regex pattern replacement`;
- response body regex replacement actions such as
  `response-body-replace-regex pattern replacement`;
- request and response JSON replacement actions such as
  `request-body-json-replace $.enabled true`;
- request and response JQ action tokens such as
  `request-body-jq '.enabled = true'` and `response-body-jq '.enabled = false'`;
- HTTP-flavored JQ aliases such as `http-request-jq '.enabled = true'` and
  `http-response-jq '.enabled = false'`.

## Parser Output

The parser must produce:

- accepted diagnostics for supported body mutation lines;
- rejected diagnostics for invalid URL or body regexes;
- request or response phase body mutation rules observable through the public
  ABI.
- an optional engine-level max-body-bytes ceiling applies to already-buffered
  body mutations; `0` disables it, and over-limit mutations preserve the
  original body with a fail-open diagnostic;
- `anixops_rewrite_apply_body_bytes` and
  `anixops_rewrite_apply_body_chain_bytes` accept explicit body lengths and
  report explicit output lengths; empty bodies remain rewritable, while
  NUL-containing or invalid-UTF-8 bodies pass through without C-string
  truncation;

## Positive Case

Parser case:

```text
tests/fixtures/BodyMutation.Common.conf
tests/fixtures/BodyRequestJsonMutation.Common.conf
tests/fixtures/BodyJsonMutation.Common.conf
tests/fixtures/BodyJqMutation.Common.conf
tests/fixtures/BodyJqAliasMutation.Common.conf
tests/fixtures/BodyJqAdvanced.Common.conf
tests/fixtures/Loon.BodyMutation.plugin
tests/fixtures/Loon.BodyJsonMutation.plugin
tests/fixtures/Loon.ResponseBodyJsonMutation.plugin
tests/fixtures/QuantumultX.BodyMutation.snippet
tests/fixtures/Surge.BodyMutation.sgmodule
tests/fixtures/Surge.BodyJsonMutation.sgmodule
tests/fixtures/Surge.BodyJqMutation.sgmodule
tests/fixtures/Shadowrocket.BodyMutation.conf
```

Expected behavior:

- config load succeeds;
- three body mutation rules are registered;
- request body regex mutation is observable through `anixops_rewrite_apply_body`;
- response body regex mutation is observable through
  `anixops_rewrite_apply_body`;
- request JSON body mutation is observable through `anixops_rewrite_apply_body`;
- dedicated common request JSON body mutation is observable through
  `anixops_rewrite_apply_body`;
- common response JSON body mutation is observable through
  `anixops_rewrite_apply_body`;
- common request and response JQ body mutation action-token matching is
  observable through `anixops_rewrite_apply_body`, with `JQ=1` rewriting and
  default builds failing open with the original body;
- common `http-request-jq` and `http-response-jq` aliases follow the same
  request/response phase separation and fail-open behavior;
- advanced optional-libjq fixture cases cover `select` predicates, array
  slices, recursive object selection, computed string filters, `del`, `map`,
  `with_entries`, `walk`, `test`, `capture`, assignment, and iterator pipes;
- optional `JQ=1` execution applies configurable input and output byte budgets;
  `0` disables each byte budget; it also applies a configurable output-value
  enumeration budget, where `0` disables the value budget; budget failures
  preserve the original body and report a fail-open diagnostic;
- POSIX `JQ=1` builds can apply a wall-clock execution timeout through child
  isolation; timeout failures preserve the original body, while `0` keeps the
  default in-process path;
- POSIX `JQ=1` builds can apply a configured child address-space memory ceiling;
  memory-limit failures preserve the original body, while `0` disables the
  ceiling; platforms without the isolation primitive fail open with a
  diagnostic;
- each libjq-enabled engine keeps a bounded configurable 1–16-entry
  compiled-filter LRU cache (default 4); the cache can be explicitly
  invalidated through `anixops_engine_clear_jq_filter_cache` or
  `anixops_engine_clear`, and count/hit metrics are observable through the
  public ABI;
- the generic already-buffered body ceiling is independently configurable
  through `anixops_engine_set_max_body_bytes`; it applies to single-body and
  body-chain mutation calls and preserves the original body when exceeded;
- Loon `[Body Rewrite]` request and response body regex mutations are
  observable through `anixops_rewrite_apply_body`;
- Loon `[Body Rewrite]` request body JSON mutation is observable through
  `anixops_rewrite_apply_body`;
- Loon `[Body Rewrite]` response body JSON mutation is observable through
  `anixops_rewrite_apply_body`;
- Quantumult X `url response-body-replace-regex` response body mutation is
  observable through `anixops_rewrite_apply_body`;
- Surge `[URL Rewrite]` `response-body-replace-regex` response body mutation
  is observable through `anixops_rewrite_apply_body`;
- Surge `[URL Rewrite]` `response-body-json-replace` response body mutation is
  observable through `anixops_rewrite_apply_body`;
- Surge `[URL Rewrite]` `http-request-jq` and `http-response-jq` body mutation
  phase mapping is observable through `anixops_rewrite_apply_body`, with
  optional `JQ=1` execution and default fail-open behavior;
- Shadowrocket `[URL Rewrite]` `response-body-replace-regex` response body
  mutation is observable through `anixops_rewrite_apply_body`;
- phase separation prevents request body rules from matching response phase
  evaluation.

## Negative Case

Parser case:

```text
tests/fixtures/BodyMutation.Common.Malformed.conf
tests/fixtures/BodyRequestJsonMutation.Common.Malformed.conf
tests/fixtures/BodyJsonMutation.Common.Malformed.conf
tests/fixtures/BodyJqMutation.Common.Malformed.conf
tests/fixtures/BodyJqAliasMutation.Common.Malformed.conf
tests/fixtures/BodyJqAdvanced.Common.Malformed.conf
tests/fixtures/Loon.BodyMutation.Malformed.plugin
tests/fixtures/Loon.BodyJsonMutation.Malformed.plugin
tests/fixtures/Loon.ResponseBodyJsonMutation.Malformed.plugin
tests/fixtures/QuantumultX.BodyMutation.Malformed.snippet
tests/fixtures/Surge.BodyMutation.Malformed.sgmodule
tests/fixtures/Surge.BodyJsonMutation.Malformed.sgmodule
tests/fixtures/Surge.BodyJqMutation.Malformed.sgmodule
tests/fixtures/Shadowrocket.BodyMutation.Malformed.conf
```

Expected behavior:

- the invalid body regex rejects config load;
- malformed common `request-body-json-replace` without a JSON path and
  replacement rejects config load under `ANIXOPS_COMPAT_LOON_STRICT`;
- malformed common `response-body-json-replace` without a JSON path and
  replacement rejects config load under `ANIXOPS_COMPAT_LOON_STRICT`;
- malformed common `request-body-jq` without a filter rejects config load under
  `ANIXOPS_COMPAT_LOON_STRICT`;
- malformed common `http-response-jq` without a filter rejects config load
  under `ANIXOPS_COMPAT_LOON_STRICT`;
- malformed advanced `response-body-jq` without a filter rejects config load
  under `ANIXOPS_COMPAT_LOON_STRICT`;
- the invalid Loon `[Body Rewrite]` body regex rejects config load;
- malformed Loon `[Body Rewrite]` `request-body-json-replace` without a JSON
  path and replacement rejects config load under `ANIXOPS_COMPAT_LOON_STRICT`;
- malformed Loon `[Body Rewrite]` `response-body-json-replace` without a JSON
  path and replacement rejects config load under `ANIXOPS_COMPAT_LOON_STRICT`;
- the invalid Quantumult X `url response-body-replace-regex` body regex rejects
  config load;
- the invalid Surge `[URL Rewrite]` response body regex rejects config load;
- malformed Surge `[URL Rewrite]` `response-body-json-replace` without a JSON
  path and replacement rejects config load under `ANIXOPS_COMPAT_SURGE_STRICT`;
- malformed Surge `[URL Rewrite]` `http-response-jq` without a filter rejects
  config load under `ANIXOPS_COMPAT_SURGE_STRICT`;
- the invalid Shadowrocket `[URL Rewrite]` response body regex rejects config
  load;
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
- JQ internal recursion/iteration limits and production cache refresh/reuse
  policy beyond the existing input/output byte, output-value, POSIX wall-clock
  timeout, optional POSIX child memory budgets, and bounded compiled-filter
  cache;
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
  `config/body_request_json_mutation_common_fixture_maps_request_body_json_replace`;
- `tests/test_config.c` registers
  `config/body_request_json_mutation_common_strict_fixture_rejects_missing_json_path`;
- `tests/test_config.c` registers
  `config/body_json_mutation_common_fixture_maps_response_body_json_replace`;
- `tests/test_config.c` registers
  `config/body_json_mutation_common_strict_fixture_rejects_missing_json_path`;
- `tests/test_config.c` registers
  `config/body_jq_mutation_common_fixture_maps_request_and_response_body_jq`;
- `tests/test_config.c` registers
  `config/body_jq_mutation_common_strict_fixture_rejects_missing_filter`;
- `tests/test_config.c` registers
  `config/body_jq_alias_mutation_common_fixture_maps_http_request_and_response_jq`;
- `tests/test_config.c` registers
  `config/body_jq_alias_mutation_common_strict_fixture_rejects_missing_filter`;
- `tests/test_config.c` registers
  `config/loon_body_mutation_fixture_maps_body_rewrites`;
- `tests/test_config.c` registers
  `config/loon_body_mutation_malformed_fixture_rejects_invalid_body_regex`;
- `tests/test_config.c` registers
  `config/loon_body_json_mutation_fixture_maps_request_body_json_replace`;
- `tests/test_config.c` registers
  `config/loon_body_json_mutation_malformed_fixture_rejects_missing_json_path`;
- `tests/test_config.c` registers
  `config/loon_response_body_json_mutation_fixture_maps_response_body_json_replace`;
- `tests/test_config.c` registers
  `config/loon_response_body_json_mutation_malformed_fixture_rejects_missing_json_path`;
- `tests/test_config.c` registers
  `config/quantumultx_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/quantumultx_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/surge_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/surge_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/surge_body_json_mutation_fixture_maps_response_body_json_replace`;
- `tests/test_config.c` registers
  `config/surge_body_json_mutation_malformed_fixture_rejects_missing_json_path`;
- `tests/test_config.c` registers
  `config/surge_body_jq_mutation_fixture_maps_request_and_response_jq`;
- `tests/test_config.c` registers
  `config/surge_body_jq_mutation_malformed_fixture_rejects_missing_filter`;
- `tests/test_config.c` registers
  `config/shadowrocket_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/shadowrocket_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_abi.c` registers
  `abi/jq_max_output_option_is_configurable`;
- `tests/test_abi.c` registers
  `abi/jq_max_output_values_option_is_configurable`;
- `tests/test_abi.c` registers
  `abi/jq_execution_timeout_option_is_configurable`;
- `tests/test_abi.c` registers
  `abi/jq_memory_option_is_configurable`;
- `tests/test_abi.c` registers
  `abi/jq_filter_cache_reuses_compiled_program`;
- `tests/test_abi.c` registers
  `abi/jq_filter_cache_policy_is_configurable`;
- `tests/test_abi.c` registers `abi/max_body_option_is_configurable`;
- `tests/test_rewrite.c` registers
  `rewrite/body_size_limit_fails_open_for_single_and_chain`;
- `tests/test_rewrite.c` registers
  `rewrite/body_bytes_api_preserves_binary_and_tracks_lengths`;
- `tests/test_config.c` registers
  `config/body_jq_advanced_common_fixture_maps_filter_edges` and
  `config/body_jq_advanced_common_strict_fixture_rejects_missing_filter`;
- `tests/test_rewrite.c` registers
  `rewrite/jq_backend_handles_output_and_error_policy`, covering byte and
  output-value budget fail-open behavior for both single-body and body-chain
  application, plus POSIX timeout and memory-limit fail-open behavior;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
body mutation
```

The row remains `partial` until streaming, compression/framing, charset matrix,
broader JSON/JQ corpus, and production adapter behavior are covered.
