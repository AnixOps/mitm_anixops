# Script Runtime Common Source Contract

This contract defines the v1.0.0 source truth for the portable script runtime
adapter boundary. It does not claim that `mitm_anixops` embeds a production
JavaScript engine. The C library selects script rules and returns dispatch
metadata; an adapter or runner executes JavaScript and writes HTTP mutations
back to the request or response object.

## Capability

Run matched request and response scripts with a small AnixOps-compatible global
surface:

- `$script`;
- `$request`;
- `$response`;
- `$argument`;
- `$persistentStore`;
- `$done(value)`;
- timeout, exception, max-size, missing asset, digest mismatch, and double
  `$done` behavior.

## Ecosystem

Portable adapter boundary for Loon, Quantumult X, Surge, and AnixOps-style
script rules.

## Input Form

- Script metadata parsed from Loon/AnixOps `http-request` and `http-response`
  rules.
- Script metadata parsed from Surge attr-list rules.
- Script metadata parsed from Quantumult X url-prefixed script rules.
- Rule-level `argument`, `timeout`, `max-size`, `requires-body`, `tag`, and
  script-path fields when the grammar exposes them.
- Local script assets supplied by `--script-map` or `--script-bundle` in the
  Alpha no-network replay runner.

## Runtime Bindings

The adapter must provide these bindings when executing a matched script:

- `$script`: metadata object with at least `startTime`, `phase`, and `type` in
  the Alpha runner.
- `$request`: request object with `url`, `method`, `headers`, and buffered
  `body` when available.
- `$response`: response object with `status`, `headers`, and buffered `body`
  during response-phase execution only.
- `$argument`: resolved argument string from `anixops_script_result_t.argument`.
- `$persistentStore`: storage object with `read`, `write`, and `remove`. The
  Alpha runner uses a JSON file backend when `--script-store` is supplied.
- `$done(value)`: completion callback. The first call wins; later calls must
  not overwrite the already accepted result.

## Parser Output

The policy core returns `anixops_script_result_t` with:

- script kind and phase;
- script path;
- tag;
- resolved argument string;
- `requires_body`;
- `timeout_ms`;
- `max_size`;
- dispatch disabled/enabled state.

The runtime adapter owns script fetching, cache validation, JavaScript
execution, body decoding, header map mutation, and HTTP writeback.

## Positive Cases

- Request scripts receive `$request`, `$argument`, `$persistentStore`, and
  `$done`.
- Response scripts receive `$request`, `$response`, `$argument`,
  `$persistentStore`, and `$done`.
- `$done.body` writes back to the no-network replay trace body.
- `$persistentStore` state is shared across request and response script
  invocations when a store file is configured.
- Offline script bundles validate sha256 digests before execution.

## Negative Cases

- Missing script assets fail open with a cache-miss diagnostic.
- Digest mismatches fail open without executing the script.
- Body larger than rule-level `max-size` fails open without executing the
  script.
- Script exceptions fail open and preserve the static-rewritten object.
- Script timeout fails open and preserves the static-rewritten object.
- Double `$done` is first-wins; a later `$done` call must not replace the first
  accepted result.

## Runtime And Matching Behavior

Adapters should run static URL, header, and body rewrite stages before script
dispatch for the same phase, then execute the matched script with the rewritten
HTTP object. Terminal URL rewrite actions such as reject or redirect stop the
pipeline for that HTTP object.

The Alpha Node runner executes no-network replay scripts through
`anixops-mitm-runner replay` using `--script-map` or `--script-bundle`. It is
reference evidence for this boundary, not a final embedded runtime selection.
The v1.0.0 policy core dependency decision is documented in
`docs/architecture/script-runtime-dependency.md`.

## Diagnostics

Runtime traces must distinguish at least:

- script executed;
- script runner not configured;
- script cache miss;
- script digest unavailable;
- script digest mismatch;
- body unavailable;
- body exceeds script max-size;
- script execution failed.

## Security Boundary

The policy core must not download remote scripts, mutate platform trust stores,
or expose credentials. A production adapter must decide cache refresh policy,
script sandboxing, log redaction, persistent storage location, and whether
network APIs are exposed to JavaScript.

## CI Evidence

- `make runner-check` covers request and response script replay,
  `$persistentStore`, `$argument`, `$done.body`, bundle sha256 match,
  digest-mismatch fail-open, cache-miss fail-open, throwing script fail-open,
  and double `$done` first-wins replay evidence.
- `make script-contract-e2e` covers the Alpha proxy path for request/response
  script mutation, static rewrite ordering, `$persistentStore`,
  rule-level timeout fail-open, exception fail-open, and gzip/deflate response
  decode with identity writeback.
- GitHub Actions `linux-test` runs `sh scripts/check.sh`, which invokes both
  checks.
- GitHub Actions `governance` requires this source contract and the
  double-done fixture to remain present.

## Compatibility Matrix Row

The matrix row is `script runtime contract` in
[matrix.md](matrix.md).

## Unimplemented Items

- Production embedded QuickJS, JavaScriptCore, or other JavaScript runtime
  selection. The current v1.0.0 policy-core decision is no embedded JavaScript
  engine.
- Production script download, cache refresh, and signature policy.
- Full platform Web API compatibility.
- Streaming body runtime.
- Binary body mutation.
- Production memory, CPU, and storage quotas.
- Adapter-specific log redaction policy.
