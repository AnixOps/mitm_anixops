# Body Mutation Common Source Contract

Capability: request and response body mutation.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`, `shadowrocket`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for the common body mutation
subset. It covers parser and policy-core behavior for already-buffered plain
text or JSON bodies. Explicit-length APIs preserve arbitrary bytes, but text
mutations intentionally bypass NUL-containing or invalid-UTF-8 binary bodies
rather than reinterpreting them as C strings. It does not claim
streaming, compression, charset conversion, or production HTTP pipeline
behavior.

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
  truncation; host adapters must consume the explicit output length rather than
  converting body buffers through text strings;

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
- a nonzero optional-libjq execution-time or memory process limit preserves the
  original body and reports `jq execution limit unavailable` or
  `jq memory limit unavailable`; the reusable policy core does not fork a child
  to execute libjq, and a future host-owned exec worker is required before such
  limits can be enforced safely;
- each libjq-enabled engine keeps a bounded configurable 1–16-entry
  compiled-filter LRU cache (default 4); the cache can be explicitly
  invalidated through `anixops_engine_clear_jq_filter_cache` or
  `anixops_engine_clear`, and count/hit metrics are observable through the
  public ABI for direct optional-libjq execution; the C engine remains
  externally synchronized, and the Alpha shim serializes every call on its
  shared engine handle before optional-libjq body-chain execution;
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

The Alpha proxy-shim adapter has a separate, bounded representation rule for
managed gzip and deflate traffic. It decodes only from the original inbound
`Content-Encoding`; a static header rewrite that deletes or replaces that
header cannot select a different decoder. A successful decode writes an
identity body, removes `Content-Encoding` and transfer encoding, and sets the
matching content length. The decoder reads at most `32 MiB + 1` decoded bytes;
before any body, script, or representation mutation, the shim also reads at
most `32 MiB + 1` raw bytes. Raw-source overflow relays the captured prefix and
unread source unchanged, restores the original headers, preserves client
`Accept-Encoding` negotiation, and skips every request/response header, body,
and script mutation for that message. For raw request overflow it also retains
the original content length or chunked transfer framing and streams the same
trailer map through the upstream transport; for plain HTTP raw response overflow
the adapter predeclares and publishes the received trailers after the relay
reaches EOF. The transport disables implicit compression so it cannot invent a
different upstream representation. Unsupported encoding, decode failure,
stacked encoding, or decoded-byte overflow likewise relays the raw body with
its original encoding and inbound fixed/chunked framing plus trailers. The
same preservation applies to header-only forwarding, no-op body rules,
identity binary bypass, and script failure when the emitted bytes are
unchanged. A successful decode, encoding change, or body mutation instead
writes a new identity representation and clears stale trailers. A later
response-script failure after a compressed decode restores the raw
representation. This adapter behavior is bounded fail-open handling, not
policy-core compression ownership.

It does not implement:

- HTTP parser, request/response serialization, or network I/O;
- streaming or partial-body mutation;
- gzip, deflate, brotli, or zstd handling;
- charset conversion;
- JQ internal recursion/iteration limits and production cache refresh/reuse
  policy beyond the existing input/output byte, output-value, and bounded
  compiled-filter cache budgets; a host-owned exec worker is required for safe
  JQ execution-time or memory process limits;
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
  application, plus unavailable execution-time and memory-limit fail-open
  behavior;
- `e2e/shim/main_test.go` asserts the Alpha shim keeps its per-engine policy
  core mutex held for an entire call; `make proxy-shim-check` runs that test
  under the Go race detector;
- `e2e/scripts/script-contract-check.sh` covers gzip/deflate request and
  response header rewrites, identity normalization, decoded request overflow
  raw-relay behavior, raw request/response overflow relay without header/body/
  script mutation, fixed-length plus header-only/decode-fail chunked request
  framing, response decode/script fail-open, no-op/binary/header-only-script
  trailer preservation, preserved client encoding negotiation, and exact binary
  request/response body passthrough through the explicit-length shim bridge;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
body mutation
```

The row remains `partial` until streaming, compression/framing, charset matrix,
broader JSON/JQ corpus, and production adapter behavior are covered.
