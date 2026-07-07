# Script Runtime Contract

`mitm_anixops` does not embed a JavaScript engine. It decides whether a URL matches a configured script rule and
returns dispatch metadata. The platform adapter owns script downloading, caching, execution, body decoding, and HTTP
mutation.

The Alpha package includes `anixops-script-runner.js`, the Node-based contract runner used by demos and E2E replay. It
is a reference adapter for the bindings below, not the final embedded QuickJS/JavaScriptCore runtime.

`anixops-mitm-runner replay` can execute the same contract without network IO when `--script-runner` and one or more
`--script-map <script-url=local-file>` entries are provided. In that mode, the runner feeds the matched script metadata
and replay body into the Node contract runner, captures `$done`, and applies a returned `body` field to the replay trace.
Pass `--script-store <file>` to share a JSON-backed `$persistentStore` across replayed script invocations. The packaged
Node contract runner exposes the same backend directly as `--store <file>`.

## Dispatch

For each intercepted HTTP request or response, the adapter should:

1. Build the canonical URL used for rule matching. Test fixtures strip local dynamic ports so
   `https://google.com:<port>/path` matches plugin rules written for `https://google.com/path`.
2. Call `anixops_script_evaluate_url(engine, url, phase, &result)` with `ANIXOPS_PHASE_REQUEST` or `ANIXOPS_PHASE_RESPONSE`.
3. If `result.kind == ANIXOPS_SCRIPT_NONE`, continue without running script code.
4. Resolve `result.script_path` to a local script asset. Network download and cache policy are outside this library.
5. Run the script with the globals below and apply the object passed to `$done`.

`result.argument` is an `&`-separated string generated from `[Argument]` defaults and runtime overrides, for example:

```text
Home.Switch=true&Storage=Argument&LogLevel=WARN
```

## Adapter Ordering

This is the recommended order when an adapter combines MITM decisions, URL rewrite, header rewrite, body rewrite, and
script dispatch in one request/response pass. The library exposes independent decisions; the adapter owns this ordering.

Terminal URL rewrite actions such as redirect or reject stop the pipeline for that HTTP object. Invalid or unsupported
rules are ignored unless the public API returns an error such as `ANIXOPS_ERR_REGEX`.

### Request Pipeline

1. Call `anixops_mitm_evaluate` before TLS interception.
2. Apply request URL rewrite with `anixops_rewrite_evaluate_url` before upstream routing.
3. Enumerate request header rewrites with `anixops_rewrite_evaluate_header`.
4. Apply request body rewrites with `anixops_rewrite_apply_body_chain` after buffering plain text.
5. Dispatch request scripts with `anixops_script_evaluate_url` after static request rewrites.
6. Send the final request upstream.

Request scripts run last so JavaScript sees the URL, headers, and body after static policy rewrites and can make the
final adapter-owned mutation before network IO.

### Response Pipeline

1. Dechunk and decompress the upstream response body when later stages need text.
2. Enumerate response header rewrites with `anixops_rewrite_evaluate_header`.
3. Apply response body rewrites with `anixops_rewrite_apply_body_chain` after plain-text decoding.
4. Dispatch response scripts with `anixops_script_evaluate_url` after static response rewrites.
5. Recompute response framing before writing to the client.

Response scripts run last so JavaScript sees the response after static policy rewrites and can make the final
adapter-owned status, header, or body mutation. After any body mutation, remove stale transfer/content encodings or
recompute them consistently.

The Alpha proxy shim follows this ordering for its HTTP/1.1 MITM demo path: request and response header/body rewrites
run before the Node contract runner is invoked, and response bodies decoded from gzip/deflate are returned as identity
after mutation.

## Globals

The adapter should expose these AnixOps-compatible bindings:

- `$script`: metadata object. The E2E runner provides `startTime`, `phase`, and `type`.
- `$request`: request object with `url`, `method`, `headers`, and `body` when a body is buffered.
- `$response`: response object with `status`, `headers`, and `body`; only present during response-phase execution.
- `$argument`: resolved argument string from `anixops_script_result_t.argument`.
- `$persistentStore`: platform storage adapter with `read`, `write`, and optional `remove`. The Alpha Node contract
  runner supports a JSON file backend through `--store <file>`.
- `$done(value)`: completion callback.

## Request Result

For `http-request` scripts, `$done` may return:

```json
{
  "url": "https://example.com/new-path",
  "headers": { "X-Feature": "on" },
  "body": "{\"changed\":true}"
}
```

Fields are optional. Missing fields mean unchanged. A present empty `body` means the body should be replaced with an
empty body.

If `url` changes, the adapter must update upstream request routing and Host handling consistently.

## Response Result

For `http-response` scripts, `$done` may return:

```json
{
  "status": 201,
  "headers": { "Content-Type": "application/json" },
  "body": "{\"ok\":true}"
}
```

Fields are optional. Missing fields mean unchanged. A present empty `body` means the response body should be replaced
with an empty body. When a body is replaced, the adapter must remove stale transfer/content encodings or recompute them.

## Body Handling

The platform adapter, not this library, is responsible for:

- buffering request/response bodies when `requires_body` is true
- calling `anixops_rewrite_apply_body_chain` for mock, regex, JSON path, and optional JQ body rewrite rules after a
  plain-text body is available; `anixops_rewrite_apply_body` remains available for legacy single-rule evaluation
- calling `anixops_rewrite_apply_headers` for the bounded Alpha header-list helper, or calling
  `anixops_rewrite_evaluate_header` / `anixops_rewrite_evaluate_named_header` and applying returned operations to a
  platform-owned HTTP header map
- dechunking HTTP/1.1 bodies before script execution
- decompressing gzip, brotli, deflate, or zstd payloads before scripts inspect text bodies
- recompressing or removing `Content-Encoding` after mutation
- updating `Content-Length` and transfer framing
- HTTP/2 frame assembly and response writeback

The Alpha proxy shim implements the response-script subset for gzip and deflate by decoding before the script runner and
returning the mutated response as identity with corrected `Content-Length`. The Alpha Node contract runner also supports
file-backed `$persistentStore` state shared across request and response script invocations.

## Error Behavior

The adapter should enforce a timeout for `$done`. The Node contract runner defaults to 5 seconds, `anixops-mitm-runner
replay` exposes this as `--timeout-ms`, and the Alpha proxy shim exposes it as `--script-timeout-ms`. When a matched
rule returns `timeout_ms`, the Alpha runner and proxy shim use it instead of the global default. When a matched rule
returns `max_size` and the buffered body is larger, they fail open without executing the script.

Recommended production behavior:

- Script timeout, max-size overflow, or exception: log the rule tag and continue with the original HTTP object unless
  the product chooses a stricter fail-closed policy.
- Invalid `$done` shape: ignore unsupported fields and preserve unchanged fields.
- Missing script asset: log and preserve the original HTTP object.

## Current E2E Proof

The Alpha proxy shim follows this policy for script runner failures. `make script-contract-e2e` includes a response
script timeout case and asserts that the client receives the static-rewritten response instead of a proxy 502.

`make script-contract-e2e` starts `mihomo`, the Go MITM shim, a local HTTPS origin, and `curl`.

It proves this contract through the proxy path:

- request script dispatch through `ANIXOPS_PHASE_REQUEST`
- request header mutation
- request body mutation
- response script dispatch through `ANIXOPS_PHASE_RESPONSE`
- response status, header, and body mutation
- `$request.url`, `$request.headers`, `$argument`, and original `$response.body` propagation

`make runner-check` also proves the no-network replay path:

- script dispatch through `ANIXOPS_PHASE_REQUEST` and `ANIXOPS_PHASE_RESPONSE`
- local script-path mapping for remote script URLs
- `$argument` propagation
- `$done.body` writeback into the replay JSON body field
