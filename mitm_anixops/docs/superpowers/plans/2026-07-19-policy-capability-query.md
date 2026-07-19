# Policy Capability Query Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish a small, versioned C ABI capability query so a NetworkCore-style host can reject an incompatible policy core before enabling managed MITM policy.

**Architecture:** The policy core exports two primitive functions: a query ABI version and a fixed 64-bit capability mask. The mask describes only existing policy decisions and metadata outputs; its deliberately closed set does not advertise TLS, CA/trust-store, capture, socket, HTTP framing, QUIC transport, or platform-permission capabilities. Go and Rust bindings expose the same query without adding a runtime dependency or network behavior.

**Tech Stack:** C99 public ABI, existing C unit harness, cgo Go binding, Rust FFI binding, Markdown compatibility contracts, GitHub Actions.

## Global Constraints

- This is policy-core metadata only; NetworkCore or another host owns socket IO, TLS, CA lifecycle, trust-store mutation, HTTP framing, QUIC behavior, traffic capture, and user consent.
- The capability set must be deterministic, process-global, and must not inspect configuration, traffic, certificates, or host state.
- Unknown capability bits are unsupported; hosts must fail closed when their required query ABI version or capability subset is absent.
- No new dependency, local build, local test, host mutation, CA mutation, network capture, or payload/header logging is allowed.
- The feature is one commit on `feat/plan-b-mitm-contract`; GitHub Actions is the acceptance source of truth before the next feature begins.

---

### Task 1: Publish the V1 policy capability query

**Files:**
- Create: `mitm_anixops/docs/superpowers/plans/2026-07-19-policy-capability-query.md`
- Create: `mitm_anixops/docs/compatibility/policy-capability-query.md`
- Modify: `mitm_anixops/include/mitm_anixops.h:5-20,254-255`
- Modify: `mitm_anixops/src/mitm_anixops.c:669-674`
- Modify: `mitm_anixops/ci/abi_exports.txt`
- Modify: `mitm_anixops/tests/test_abi.c:18-25,final registry`
- Modify: `mitm_anixops/bindings/go/anixops/anixops.go:16-35,Version helpers`
- Modify: `mitm_anixops/bindings/go/anixops/anixops_test.go:TestGoBindingEvaluatesPolicy`
- Modify: `mitm_anixops/bindings/rust/mitm-anixops/src/lib.rs:imports,extern block,public helpers,test module`
- Modify: `mitm_anixops/docs/compatibility/matrix.md`
- Modify: `mitm_anixops/docs/compatibility_matrix.md`
- Modify: `mitm_anixops/docs/networkcore_integration.md`
- Modify: `mitm_anixops/CHANGELOG.md`

**Interfaces:**
- Produces: `unsigned int anixops_policy_capability_query_abi_version(void)` returning `ANIXOPS_POLICY_CAPABILITY_QUERY_ABI_VERSION`.
- Produces: `uint64_t anixops_policy_capability_flags(void)` returning exactly `ANIXOPS_POLICY_CAPABILITY_ALL_V1`.
- Produces: `PolicyCapabilityABIVersion() uint`, `PolicyCapabilities() PolicyCapability`, and `PolicyCapability.Supports(PolicyCapability) bool` for Go consumers.
- Produces: `policy_capability_query_abi_version() -> u32`, `policy_capabilities() -> PolicyCapabilitySet`, and `PolicyCapabilitySet::supports(u64) -> bool` for Rust consumers.

- [ ] **Step 1: Add failing API coverage before the implementation**

Add the following C test to `tests/test_abi.c` and register it as `abi/policy_capability_query_is_stable`:

```c
static void policy_capability_query_is_stable(void)
{
	uint64_t flags = anixops_policy_capability_flags();
	ANIXOPS_EXPECT_EQ_INT(
		anixops_policy_capability_query_abi_version(),
		ANIXOPS_POLICY_CAPABILITY_QUERY_ABI_VERSION);
	ANIXOPS_EXPECT_TRUE((flags & ANIXOPS_POLICY_CAPABILITY_ALL_V1) == ANIXOPS_POLICY_CAPABILITY_ALL_V1);
	ANIXOPS_EXPECT_TRUE((flags & ~ANIXOPS_POLICY_CAPABILITY_ALL_V1) == 0);
	ANIXOPS_EXPECT_TRUE((flags & (UINT64_C(1) << 63)) == 0);
}
```

Add Go and Rust binding assertions that the V1 ABI version is returned, all seven V1 policy flags are supported, and bit 63 is not supported. Do not run a local build or test: repository policy reserves that evidence for GitHub Actions.

- [ ] **Step 2: Add the smallest stable C ABI**

In `include/mitm_anixops.h`, include `<stdint.h>` and define these constants with fixed values:

```c
#define ANIXOPS_POLICY_CAPABILITY_QUERY_ABI_VERSION 1u
#define ANIXOPS_POLICY_CAPABILITY_MITM_DECISION (UINT64_C(1) << 0)
#define ANIXOPS_POLICY_CAPABILITY_URL_REWRITE (UINT64_C(1) << 1)
#define ANIXOPS_POLICY_CAPABILITY_HEADER_MUTATION (UINT64_C(1) << 2)
#define ANIXOPS_POLICY_CAPABILITY_BODY_MUTATION_BYTES (UINT64_C(1) << 3)
#define ANIXOPS_POLICY_CAPABILITY_SCRIPT_DISPATCH_METADATA (UINT64_C(1) << 4)
#define ANIXOPS_POLICY_CAPABILITY_RULE_DIAGNOSTICS (UINT64_C(1) << 5)
#define ANIXOPS_POLICY_CAPABILITY_TASK_DESCRIPTOR_METADATA (UINT64_C(1) << 6)
#define ANIXOPS_POLICY_CAPABILITY_ALL_V1 ( \
	ANIXOPS_POLICY_CAPABILITY_MITM_DECISION | \
	ANIXOPS_POLICY_CAPABILITY_URL_REWRITE | \
	ANIXOPS_POLICY_CAPABILITY_HEADER_MUTATION | \
	ANIXOPS_POLICY_CAPABILITY_BODY_MUTATION_BYTES | \
	ANIXOPS_POLICY_CAPABILITY_SCRIPT_DISPATCH_METADATA | \
	ANIXOPS_POLICY_CAPABILITY_RULE_DIAGNOSTICS | \
	ANIXOPS_POLICY_CAPABILITY_TASK_DESCRIPTOR_METADATA)
```

Declare and implement exactly:

```c
ANIXOPS_API unsigned int anixops_policy_capability_query_abi_version(void);
ANIXOPS_API uint64_t anixops_policy_capability_flags(void);

ANIXOPS_API unsigned int anixops_policy_capability_query_abi_version(void)
{
	return ANIXOPS_POLICY_CAPABILITY_QUERY_ABI_VERSION;
}

ANIXOPS_API uint64_t anixops_policy_capability_flags(void)
{
	return ANIXOPS_POLICY_CAPABILITY_ALL_V1;
}
```

Append both public symbols to `ci/abi_exports.txt` in lexicographic order. Do not add flags for any host-owned capability.

- [ ] **Step 3: Expose parity helpers in Go and Rust**

Add the same fixed masks to the bindings, exposing a compact value type and subset helper. The helper must use `required == 0 || (available & required) == required`, so a caller can safely test a required subset without assuming unknown bits are acceptable. Bind both C functions through cgo/Rust FFI and keep their return values primitive (`unsigned int`/`uint64_t` in C, `u32`/`u64` in Rust).

- [ ] **Step 4: Write the compatibility and integration contract**

Create `docs/compatibility/policy-capability-query.md` with:

- the seven advertised policy-only flags and their matching public APIs;
- the closed-world rule for unknown bits and mismatched query ABI versions;
- an explicit non-capability list for TLS termination, root CA/trust stores, certificate issuance, capture, socket IO, HTTP parser/framing, HTTP/2, QUIC transport, JavaScript execution, storage, permissions, and consent;
- host activation rule: verify the signed managed profile, check the query ABI/version and required flags, enforce its managed hostname allowlist, verify consent/trust state, and otherwise reject or bypass according to the host's fail-closed policy;
- C, Go, and Rust test identifiers from Step 1.

Add a `policy core capability query` row to both compatibility matrices with `supported` status, this new source contract, `abi/policy_capability_query_is_stable` as positive evidence, the unknown-bit assertion as negative evidence, and host-owned data-plane behavior listed as unimplemented. Update `docs/networkcore_integration.md` to require capability-query validation before a host consumes policy results. Add a concise unreleased changelog entry.

- [ ] **Step 5: Perform non-build pre-commit checks and commit the feature**

Run only permitted read-only checks:

```bash
git diff --check
git status --short
git diff -- mitm_anixops/include/mitm_anixops.h mitm_anixops/src/mitm_anixops.c mitm_anixops/ci/abi_exports.txt
```

Review that the export allowlist is lexicographically ordered, no production or data-plane claim appears in documentation, and no unrelated file changed. Commit all Task 1 files together:

```bash
git add mitm_anixops
git commit -m "feat: expose policy core capability query"
```

- [ ] **Step 6: Push and use GitHub Actions as the sole acceptance gate**

Configure the GitHub remote only if it is absent, push `feat/plan-b-mitm-contract` to `AnixOps/mitm_anixops`, and monitor the branch build workflow until all jobs conclude. Do not start another feature until the full GitHub Actions run is successful. If CI fails, diagnose the exact failing job, make one corrective commit for the discovered defect, push it, and wait for a fully green run.
