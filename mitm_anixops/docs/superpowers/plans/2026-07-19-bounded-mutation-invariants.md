# Bounded Mutation Invariants Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve body/header representation integrity and bounded execution when the MITM shim mutates managed HTTP traffic.

**Architecture:** The shim keeps the inbound `Content-Encoding` as the sole source for decoding. Header rewrites may request an identity representation, but only after a bounded decode has succeeded; a decode error or expansion limit restores the original compressed representation and its matching header. The C policy core prewarms its per-engine JQ cache in the parent before bounded child execution, so the public cache metrics remain true across POSIX isolation.

**Tech Stack:** Go standard compression readers and HTTP shim, C99/libjq POSIX isolation, Go/Rust FFI bindings, existing C harness, shell E2E contract, GitHub Actions.

## Global Constraints

- Keep the Plan B boundary: policy core owns policy metadata only; this change must not add TLS, CA/trust-store, capture, socket, framing, QUIC, JavaScript, storage, permission, or consent ownership.
- Do not log payloads or header values. Existing metadata-only debug logs may remain.
- `maxBodyRewriteBuffer` remains exactly `32 * 1024 * 1024`; decoded gzip/deflate data must never exceed that size before fail-open relay.
- A header rewrite that deletes or replaces `Content-Encoding` must never cause compressed bytes to be forwarded with a mismatching encoding declaration.
- Decode failure, unsupported encoding, or decoded-byte overflow must preserve the original body representation and matching `Content-Encoding`, while retaining unrelated header rewrites where safe.
- JQ memory-only isolation (`max_memory_bytes != 0`, `max_execution_time_ms == 0`) must never read an uninitialized clock value.
- The documented JQ cache remains per-engine, observable, and reusable for bounded POSIX isolation calls.
- The capability query remains closed-world: an ABI V1 result is compatible only when no bit outside `POLICY_CAPABILITY_ALL_V1` is present.
- This is one corrective feature commit on `feat/plan-b-mitm-contract`; do not run local builds or tests. GitHub Actions is the sole acceptance gate before any later feature.

---

### Task 1: Harden representation, isolation, and closed-world capability boundaries

**Files:**
- Create: `mitm_anixops/docs/superpowers/plans/2026-07-19-bounded-mutation-invariants.md`
- Modify: `mitm_anixops/e2e/shim/main.go:1049-1285,1741-1770`
- Modify: `mitm_anixops/e2e/scripts/script-contract-check.sh:61-78,278-335`
- Modify: `mitm_anixops/src/mitm_anixops.c:470-535,7448-7695`
- Modify: `mitm_anixops/tests/test_rewrite.c:2380-2668,anixops_register_rewrite_tests`
- Modify: `mitm_anixops/bindings/go/anixops/anixops.go:policy capability helpers`
- Modify: `mitm_anixops/bindings/go/anixops/anixops_test.go:TestGoBindingEvaluatesPolicy`
- Modify: `mitm_anixops/bindings/rust/mitm-anixops/src/lib.rs:PolicyCapabilitySet,rust_binding_evaluates_policy`
- Modify: `mitm_anixops/docs/compatibility/policy-capability-query.md`
- Modify: `mitm_anixops/docs/compatibility/body-mutation-common.md`
- Modify: `mitm_anixops/docs/compatibility/matrix.md`
- Modify: `mitm_anixops/docs/compatibility_matrix.md`
- Modify: `mitm_anixops/CHANGELOG.md`

**Interfaces:**
- Produces: an internal bounded decoder that accepts the original inbound encoding and returns `(decoded []byte, decoded bool, err error)` without reading more than `maxBodyRewriteBuffer + 1` decoded bytes.
- Produces: internal helpers that normalize successful identity bodies or restore raw compressed bodies with a matching `Content-Encoding` and correct `Content-Length`.
- Produces: `PolicyCapability.IsV1Compatible() bool` and `PolicyCapabilitySet::is_v1_compatible() -> bool`; both evaluate `available & ^ALL_V1 == 0` / `available & !ALL_V1 == 0` and do not replace ABI-version validation.

- [x] **Step 1: Add regression coverage before production changes**

Extend the dynamic E2E module config so its existing compressed request cases exercise both broken header orders:

```text
...request-compressed=gzip header-del Content-Encoding
...request-compressed=deflate header-replace Content-Encoding gzip
```

Keep `run_compressed_request_case gzip` and `deflate` asserting that the
origin receives identity JSON, `requestEncoding == ""`, static JSON mutation,
and request-script mutation. These assertions must fail on the current code
because it decodes from the rewritten header rather than the inbound header.

Add `run_compressed_request_overflow_case` that creates a gzip request whose
decoded payload is `32 * 1024 * 1024 + 1` bytes. Add an origin summary mode
that reports only request encoding and byte count, never the payload. Assert
the request reaches the origin with `Content-Encoding: gzip` and a nonzero
compressed byte count, proving decoded overflow relays raw bytes fail-open.

Add a libjq/POSIX C regression named
`rewrite/jq_bounded_execution_reuses_parent_cache`: configure only a memory
limit, apply the same small JQ filter twice, then assert rewritten output,
cache count `1`, and cache-hit count `1`. This must fail before the parent
prewarm because child-only cache updates are lost after `fork`.

Extend Go and Rust policy capability tests with a synthetic mask containing
`ALL_V1 | (1 << 63)` and assert their new closed-world helper rejects it.

Do not run those tests locally. They are intentionally committed before the
production changes and are verified only by the subsequent GitHub Actions
run, per repository policy.

- [x] **Step 2: Restore a single body-representation invariant in the shim**

In both `applyRequestScript` and `applyResponseScript`, clone headers before
calling `applyHeaderRewrites` and save the original `Content-Encoding`. Decode
only from that original encoding. If a body rewrite/script needs a body, or a
header rewrite changed the encoding, decode the raw body using the bounded
helper. On success, make the outgoing body identity and call the existing
normalization logic so `Content-Encoding` and transfer encoding are removed
and content length matches the decoded bytes.

Use an explicit bounded read in the decoder:

```go
limited := io.LimitReader(reader, int64(maxBodyRewriteBuffer)+1)
decoded, err := io.ReadAll(limited)
if len(decoded) > maxBodyRewriteBuffer {
    return nil, false, errDecodedBodyTooLarge
}
```

When decoding fails, restore the original encoding on the already-rewritten
header, set content length to the raw byte length, and relay the raw body.
For a response, retain the original raw body and return `nil` rather than
turning a bounded decode failure into a 502. Do the same restoration if a
later response script fails after an otherwise successful decode. Apply this
symmetrically to request and response paths so a response `Content-Encoding`
header rewrite cannot produce the same mismatch.

- [x] **Step 3: Prewarm JQ cache in the parent and remove the unsafe clock read**

Before `anixops_apply_body_jq_replacement_isolated` forks, call
`anixops_jq_filter_cache_get(engine, filter, &compile_status)` in the parent
and return its failure status directly. The child then finds the inherited
compiled program, while the parent keeps cache count and hit metrics. Do not
add a second cache or alter cache capacity/eviction policy.

In the isolated polling loop, initialize `elapsed` to zero and call
`anixops_jq_elapsed_ms(&start)` only when `max_execution_time_ms != 0`:

```c
size_t elapsed = 0;
if (max_execution_time_ms != 0) {
    elapsed = anixops_jq_elapsed_ms(&start);
    if (elapsed == SIZE_MAX || elapsed >= max_execution_time_ms) {
        /* existing timeout cleanup */
    }
}
```

This leaves memory-only isolation blocking on `poll(..., -1)` without reading
an uninitialized `struct timespec`.

- [x] **Step 4: Expose closed-world convenience helpers and update contracts**

Add `IsV1Compatible` to the Go capability type and `bits` plus
`is_v1_compatible` to Rust. Keep `Supports`/`supports` as subset checks;
document that callers still require query ABI version `1` before accepting a
mask. Update both compatibility matrices to cite the new negative unknown-bit
tests. Update the body-mutation source contract with the raw/identity
representation rule, 32 MiB decoded ceiling, bounded-isolation cache evidence,
and fail-open behavior. Add one concise unreleased changelog entry.

- [ ] **Step 5: Review, commit, push, and wait for GitHub Actions**

Run only permitted non-build checks:

```bash
git diff --check
git status --short
git diff -- mitm_anixops/e2e/shim/main.go mitm_anixops/src/mitm_anixops.c \
  mitm_anixops/tests/test_rewrite.c mitm_anixops/bindings/go/anixops/anixops.go \
  mitm_anixops/bindings/rust/mitm-anixops/src/lib.rs
```

Review that no payload/header values are logged, no TLS/CA/capture/permission
ownership was introduced, and every modified test is directly tied to the
invariant. Commit exactly once:

```bash
git add mitm_anixops
git commit -m "fix: preserve bounded mutation invariants"
```

Push `feat/plan-b-mitm-contract` to `AnixOps/mitm_anixops`. Monitor the full
GitHub Actions workflow. Do not begin the next feature until every required job
succeeds. On any failure, trace the exact failing assertion, add one
corrective commit for that root cause, and wait for a replacement green run.
