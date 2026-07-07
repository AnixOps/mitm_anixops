# Script Runtime Contract

`mitm_anixops` does not embed a JavaScript engine. It decides whether a URL matches a configured script rule and
returns dispatch metadata. The platform adapter owns script downloading, caching, execution, body decoding, and HTTP
mutation.

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
4. Apply request body rewrites with `anixops_rewrite_apply_body` after buffering plain text.
5. Dispatch request scripts with `anixops_script_evaluate_url` after static request rewrites.
6. Send the final request upstream.

Request scripts run last so JavaScript sees the URL, headers, and body after static policy rewrites and can make the
final adapter-owned mutation before network IO.

### Response Pipeline

1. Dechunk and decompress the upstream response body when later stages need text.
2. Enumerate response header rewrites with `anixops_rewrite_evaluate_header`.
3. Apply response body rewrites with `anixops_rewrite_apply_body` after plain-text decoding.
4. Dispatch response scripts with `anixops_script_evaluate_url` after static response rewrites.
5. Recompute response framing before writing to the client.

Response scripts run last so JavaScript sees the response after static policy rewrites and can make the final
adapter-owned status, header, or body mutation. After any body mutation, remove stale transfer/content encodings or
recompute them consistently.

## Globals

The adapter should expose these AnixOps-compatible bindings:

- `$script`: metadata object. The E2E runner provides `startTime`, `phase`, and `type`.
- `$request`: request object with `url`, `method`, `headers`, and `body` when a body is buffered.
- `$response`: response object with `status`, `headers`, and `body`; only present during response-phase execution.
- `$argument`: resolved argument string from `anixops_script_result_t.argument`.
- `$persistentStore`: platform storage adapter with `read` and `write`.
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
- calling `anixops_rewrite_apply_body` for mock and regex body rewrite rules after a plain-text body is available
- calling `anixops_rewrite_evaluate_header` and applying returned operations to the platform HTTP header map
- dechunking HTTP/1.1 bodies before script execution
- decompressing gzip, brotli, deflate, or zstd payloads before scripts inspect text bodies
- recompressing or removing `Content-Encoding` after mutation
- updating `Content-Length` and transfer framing
- HTTP/2 frame assembly and response writeback

## Error Behavior

The adapter should enforce a timeout for `$done`. The E2E runner defaults to 5 seconds.

Recommended production behavior:

- Script timeout or exception: log the rule tag and continue with the original HTTP object unless the product chooses a
  stricter fail-closed policy.
- Invalid `$done` shape: ignore unsupported fields and preserve unchanged fields.
- Missing script asset: log and preserve the original HTTP object.

## Current E2E Proof

`make script-contract-e2e` starts `mihomo`, the Go MITM shim, a local HTTPS origin, and `curl`.

It proves this contract through the proxy path:

- request script dispatch through `ANIXOPS_PHASE_REQUEST`
- request header mutation
- request body mutation
- response script dispatch through `ANIXOPS_PHASE_RESPONSE`
- response status, header, and body mutation
- `$request.url`, `$request.headers`, `$argument`, and original `$response.body` propagation
