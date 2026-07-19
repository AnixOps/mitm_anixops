# Multithreaded Body Mutation Safety Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove unsafe JQ process isolation from the reusable policy core and preserve arbitrary binary body bytes through the reference HTTP shim.

**Architecture:** The policy core must not run libjq, allocation, or resource-limit code in a child produced by `fork()` from a potentially multithreaded host. Until a future host-owned exec worker exists, a configured JQ execution-time or memory limit is reported as unavailable and leaves the original body unchanged. The shim delegates arbitrary bodies to the existing explicit-length C bytes API, which deliberately bypasses text mutation for NUL-containing or invalid-UTF-8 data and preserves exact byte length.

**Tech Stack:** C99/libjq policy core, C explicit-length ABI, cgo Go shim, shell/Python E2E contract, Markdown source contracts, GitHub Actions.

## Global Constraints

- Plan B boundaries remain unchanged: policy core owns deterministic policy metadata and bounded text mutation only; the host owns TLS, CA/trust stores, certificates, capture, socket IO, HTTP framing, QUIC transport, JavaScript execution, storage, permissions, and consent.
- Do not use `fork()` to execute libjq, `malloc`, `free`, `setrlimit`, or any non-async-signal-safe code in a reusable library child path.
- A nonzero JQ execution-time or memory limit must fail open with the original body and the stable `jq execution limit unavailable` or `jq memory limit unavailable` diagnostic. It must not hang, kill a host thread, or silently run unbounded work under a claimed limit.
- Keep unbounded optional JQ behavior, input/output/value limits, and the existing per-engine direct-path compiled-filter cache unchanged when both process limits are zero.
- The Go shim must call `anixops_rewrite_apply_body_chain_bytes` for all bodies and use the returned explicit byte length; it must not use `C.CString(string(body))` or `C.GoString` for body buffers.
- Binary body handling is passthrough-only. NUL-containing and invalid-UTF-8 bodies may not be text-mutated, truncated, logged, or re-encoded.
- Do not log payloads or header values. The binary E2E origin may expose only length/digest metadata in a test response header.
- This is one corrective feature commit on `feat/plan-b-mitm-contract`; no local build, test, formatter, or executable is allowed. GitHub Actions is the only acceptance gate before later work.

---

### Task 1: Fail open on unsafe JQ limits and use explicit-length shim mutation

**Files:**
- Create: `mitm_anixops/docs/superpowers/plans/2026-07-19-multithreaded-body-mutation-safety.md`
- Modify: `mitm_anixops/src/mitm_anixops.c:1-35,7293-7706`
- Modify: `mitm_anixops/tests/test_rewrite.c:JQ limit regressions,registration`
- Modify: `mitm_anixops/e2e/shim/main.go:imports,applyBody,startOrigin`
- Modify: `mitm_anixops/e2e/scripts/script-contract-check.sh:dynamic config,binary body regression`
- Modify: `mitm_anixops/README.md`
- Modify: `mitm_anixops/TODO.md`
- Modify: `mitm_anixops/CHANGELOG.md`
- Modify: `mitm_anixops/docs/TODO.md`
- Modify: `mitm_anixops/docs/alpha_release_notes.md`
- Modify: `mitm_anixops/docs/compatibility/body-mutation-common.md`
- Modify: `mitm_anixops/docs/compatibility/matrix.md`
- Modify: `mitm_anixops/docs/compatibility/source-contracts.md`
- Modify: `mitm_anixops/docs/compatibility_matrix.md`
- Modify: `mitm_anixops/docs/loon_mitm_full_compat_brief_zh.md`
- Modify: `mitm_anixops/docs/loon_mitm_full_compat_plan.md`
- Modify: `mitm_anixops/docs/networkcore_integration.md`
- Modify: `mitm_anixops/docs/superpowers/plans/2026-07-19-bounded-mutation-invariants.md`

**Interfaces:**
- Keeps existing `anixops_engine_set_jq_max_execution_time_ms` and `anixops_engine_set_jq_max_memory_bytes` APIs, but a nonzero setting returns a fail-open unavailable diagnostic unless a future safe worker is added.
- Consumes `anixops_rewrite_apply_body_chain_bytes(engine, url, phase, body, body_len, out, out_cap, out_len, chain)` from cgo and returns exactly `out_len` bytes.
- Produces no new public C ABI, runtime process, worker binary, TLS/CA behavior, or host activation capability.

- [x] **Step 1: Add failing regression coverage before production changes**

Update the existing `JQ=1` limit regression so configured execution time and
memory limits expect the stable unavailable diagnostics and the original body,
not a child-enforced timeout/memory result. This fails on the current POSIX
implementation because it still enters the forked child path.

Add a binary E2E case under the existing MITM shim contract. Its dynamic
plugin config must attach no-match request and response body-regex rules to a
dedicated `/contract/binary-body` endpoint so the shim invokes its body-mutation
adapter in both directions. An identity text-rule sentinel follows those rules:
the legacy C-string bridge applies it after truncating at NUL, while the bytes
API bypasses all text rules for binary input. The test must:

```text
1. create bytes containing NUL and invalid UTF-8;
2. gzip the request bytes and send them with Content-Encoding: gzip;
3. make the origin return those received bytes gzip-encoded;
4. assert the origin's length/digest metadata matches the expected bytes;
5. assert the client output is byte-for-byte identical to the original file.
```

The current `C.CString`/`C.GoString` adapter truncates this case at the first
NUL, so the exact-byte assertion is the required RED regression.

Do not run local tests. Repository policy reserves RED/GREEN execution for
GitHub Actions; stage tests before production changes and document that
constraint in the implementation report.

- [x] **Step 2: Remove the unsafe post-fork execution path**

Delete `ANIXOPS_JQ_POSIX_ISOLATION`, its POSIX-only includes, child result
frame, `fork`/pipe/poll helpers, resource-limit helper, and
`anixops_apply_body_jq_replacement_isolated`. Do not retain an unused guarded
copy or an opt-in unsafe build flag.

In `anixops_apply_body_jq_replacement`, retain the existing direct path when
both process limits are zero. For any nonzero execution-time or memory limit,
set the corresponding `*_limit_unavailable` output flags and return
`ANIXOPS_ERR_CAPACITY` before compiling, allocating, or executing JQ. The
existing caller must then preserve the original body and report the stable
unavailable diagnostic. No child must be created.

- [x] **Step 3: Switch the shim to the explicit-length bytes ABI**

Replace the text C-string bridge in `engine.applyBody` with caller-owned byte
buffers. Allocate input with `C.CBytes` only when `len(body) != 0`, allocate
an unsigned-byte output buffer, call `anixops_rewrite_apply_body_chain_bytes`,
and create the Go result with `C.GoBytes(unsafe.Pointer(out), C.int(outLen))`.
Validate `outLen <= outCap` before converting; treat an invalid C length as an
adapter error. Preserve the existing message, no-rewrite, and truncation
semantics, but compare the explicit-byte result rather than a C string.

Extend the test origin with `/contract/binary-body`: read request bytes, emit
only length and SHA-256 metadata headers, gzip the same bytes for its response,
and never serialize the payload into JSON or logs.

- [x] **Step 4: Correct all public contracts**

Replace every source-contract statement that claims POSIX child timeout or
memory enforcement with the actual current state: the setters are accepted,
but in-process policy-core execution rejects nonzero process limits fail-open
until a future host-owned exec worker provides a safe isolation boundary.
State that explicit-length bytes APIs preserve empty, NUL-containing, and
invalid-UTF-8 bodies, while text mutations intentionally bypass binary bodies.
Update old bounded-isolation plan text with a visible superseded note pointing
to this plan. Keep NetworkCore wording clear that any future timeout/memory
worker belongs to the host adapter, not the policy core. Add a concise
unreleased changelog entry.

- [ ] **Step 5: Static review, one commit, push, and GitHub Actions**

Run only permitted checks:

```bash
git diff --check
git status --short
git diff -- mitm_anixops/src/mitm_anixops.c mitm_anixops/e2e/shim/main.go \
  mitm_anixops/e2e/scripts/script-contract-check.sh mitm_anixops/tests/test_rewrite.c
```

Review that no `fork` remains in the policy core, no body C-string conversion
remains in `applyBody`, no raw body is added to E2E response JSON/logs, and no
new public ABI or host-data-plane responsibility appears. Commit exactly once:

```bash
git add mitm_anixops
git commit -m "fix: harden multithreaded body mutation"
```

- [x] Static source and diff review completed; no local build, test, formatter,
  or executable was run.
- [ ] Push only after task review approval.
- [ ] GitHub Actions acceptance run.

Push only after task review approval. Then wait for a new fully successful
GitHub Actions run before any later feature or release discussion.

---

### Task 2: Complete binary script passthrough and runner-error redaction

**Files:**
- Modify: `mitm_anixops/e2e/shim/main.go:applyRequestScript,applyResponseScript,runScript`
- Modify: `mitm_anixops/e2e/scripts/script-contract-check.sh:module config,shim invocation,redaction regression,binary regression`
- Modify: `mitm_anixops/e2e/script_runtime/response_mutator.js`
- Modify: `mitm_anixops/docs/architecture/adapter-redaction-policy.md`
- Modify: `mitm_anixops/docs/compatibility/script-runtime-common.md`
- Modify: `mitm_anixops/docs/compatibility/source-contracts.md`
- Modify: `mitm_anixops/docs/compatibility/matrix.md`
- Modify: `mitm_anixops/docs/compatibility_matrix.md`
- Modify: `mitm_anixops/CHANGELOG.md`
- Modify: `mitm_anixops/docs/superpowers/plans/2026-07-19-multithreaded-body-mutation-safety.md`

**Interfaces:**
- Produces an internal text predicate that accepts only bodies with no NUL byte and valid UTF-8 for script dispatch.
- Keeps script runner failure diagnostics as a fixed, redacted classification; runner stderr and raw runner output never leave `runScript` as an error string.
- Produces no new public ABI, script capability, logging surface, or host data-plane behavior.

- [x] **Step 1: Add failing script-boundary regressions before implementation**

Add request and response script rules for `/contract/binary-body`, using the
existing local response mutator. Make that test mutator return a recognizable
replacement body whenever its URL is the binary endpoint. The binary E2E
must continue asserting byte-for-byte equality; it fails on the current code
if either binary script hook runs.

Start the E2E shim with `--debug` and add a `redaction=1` response-script case
that intentionally throws an error whose message contains a unique test body
secret and a unique request-header secret. Assert the request remains fail-open
and the shim log contains only the generic runner failure classification, never
either secret. This fails on the current code because `runScript` embeds child
stderr in the error later handed to `debugf`.

Do not run those tests locally. Stage these assertions before production edits;
GitHub Actions is the only RED/GREEN acceptance mechanism for this repository.

- [x] **Step 2: Bypass all script dispatch for binary bodies**

Add an internal helper equivalent to:

```go
func isScriptTextBody(body []byte) bool {
    return bytes.IndexByte(body, 0) == -1 && utf8.Valid(body)
}
```

After static body handling, before calling `runScript`, return the unchanged
request body when a matching script sees a non-text body. For responses, restore
the already-decoded identity response with `replaceResponseBody` and return
without dispatching a script when `scriptBody` is non-text. Keep static header
rewrites and byte-API body passthrough behavior intact. Debug messages may use
URL, tag, phase, and byte count only.

- [x] **Step 3: Redact runner failures at the adapter boundary**

When `cmd.Output()` returns an `*exec.ExitError`, discard `Stderr` and return a
fixed error such as `script runner failed`. Return a separate fixed
classification for malformed runner output. Do not include raw body bytes,
serialized headers, runner stderr, `$done` output, or script source in any
returned error. Existing fail-open callers continue to log the fixed
classification and preserve the applicable body representation.

- [x] **Step 4: Update source contracts and amend the unpushed commit**

State that the reference Node runtime receives only text bodies: NUL-containing
or invalid-UTF-8 bodies bypass every request/response script hook and retain
the byte API's passthrough semantics. State that runner stderr is not propagated
into adapter diagnostics or debug logs. Update E2E evidence references and the
unreleased changelog.

Run only `git diff --check`, `git status --short`, focused diff/source review,
and documentation searches. Amend the unpushed Task 1 commit rather than
creating a second feature commit:

```bash
git add mitm_anixops
git commit --amend --no-edit
```

Leave push and GitHub Actions unchecked until task review approves the amended
commit.

- [x] Static source, documentation, and diff review completed; no local build,
  test, formatter, or executable was run.
- [ ] Push only after task review approval.
- [ ] GitHub Actions acceptance run.
