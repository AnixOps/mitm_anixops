# Policy Capability Query Source Contract

Capability: versioned policy-core capability query.

Ecosystem: portable NetworkCore-style host boundary.

Status: `supported`.

## Purpose

The policy core exposes a deterministic, process-global V1 query so a host can
reject an incompatible policy core before it consumes policy results. The query
does not inspect configuration, traffic, certificates, network state, or host
state.

## Public ABI

```c
ANIXOPS_API unsigned int anixops_policy_capability_query_abi_version(void);
ANIXOPS_API uint64_t anixops_policy_capability_flags(void);
```

`anixops_policy_capability_query_abi_version()` returns
`ANIXOPS_POLICY_CAPABILITY_QUERY_ABI_VERSION`, currently `1`.
`anixops_policy_capability_flags()` returns exactly
`ANIXOPS_POLICY_CAPABILITY_ALL_V1`.

The Go binding exposes `PolicyCapabilityABIVersion()`,
`PolicyCapabilities()`, and `PolicyCapability.Supports(PolicyCapability)`.
The Rust binding exposes `policy_capability_query_abi_version()`,
`policy_capabilities()`, and `PolicyCapabilitySet::supports(u64)`.

## V1 Policy Capabilities

| Flag | Policy-Core Public API |
| --- | --- |
| `ANIXOPS_POLICY_CAPABILITY_MITM_DECISION` | `anixops_mitm_evaluate` |
| `ANIXOPS_POLICY_CAPABILITY_URL_REWRITE` | `anixops_rewrite_evaluate_url`, `anixops_rewrite_build_plan` |
| `ANIXOPS_POLICY_CAPABILITY_HEADER_MUTATION` | `anixops_rewrite_evaluate_header`, `anixops_rewrite_evaluate_named_header`, `anixops_rewrite_apply_headers` |
| `ANIXOPS_POLICY_CAPABILITY_BODY_MUTATION_BYTES` | `anixops_rewrite_apply_body_bytes`, `anixops_rewrite_apply_body_chain_bytes` |
| `ANIXOPS_POLICY_CAPABILITY_SCRIPT_DISPATCH_METADATA` | `anixops_script_evaluate_url`, `anixops_rewrite_build_plan` |
| `ANIXOPS_POLICY_CAPABILITY_RULE_DIAGNOSTICS` | `anixops_engine_rule_diagnostic_count`, `anixops_engine_copy_rule_diagnostic` |
| `ANIXOPS_POLICY_CAPABILITY_TASK_DESCRIPTOR_METADATA` | `anixops_engine_task_descriptor_count`, `anixops_engine_copy_task_descriptor` |

The script and task flags describe dispatch or descriptor metadata only. They
do not describe runtime execution or scheduling.

## Closed-World Compatibility Rule

Hosts must first require query ABI version `1`, then verify that the returned
mask contains no bits outside `ANIXOPS_POLICY_CAPABILITY_ALL_V1`, and finally
verify that their required subset is present. Unknown bits are unsupported;
an unknown bit or a mismatched query ABI version requires the host to reject
the policy result or bypass it according to its fail-closed policy.

The Go and Rust subset helpers implement
`required == 0 || (available & required) == required`. They are only a subset
check. They do not make an unknown query ABI version or an unknown returned bit
compatible.

## Explicit Non-Capabilities

This query does not advertise or implement TLS termination, root CA or trust
stores, certificate issuance, traffic capture, socket IO, HTTP parser or
framing behavior, HTTP/2, QUIC transport, JavaScript execution, storage,
platform permissions, or user consent. Those responsibilities remain with the
host adapter.

## Host Activation Rule

Before consuming any policy result, the host must:

1. Verify the signed managed profile.
2. Check the query ABI version, reject unknown returned bits, and require its
   needed V1 policy flags.
3. Enforce the managed hostname allowlist.
4. Verify host-owned consent and trust state.
5. Consume the policy result only after all checks pass; otherwise reject or
   bypass it according to the host's fail-closed policy.

The capability query does not activate TLS interception, traffic capture, or
any other host-owned data-plane behavior.

## CI Evidence

Required CI evidence:

- C: `abi/policy_capability_query_is_stable`, including the bit-63
  unknown-bit assertion.
- Go: `TestGoBindingEvaluatesPolicy`.
- Rust: `rust_binding_evaluates_policy`.

GitHub Actions is the acceptance source of truth for this contract.
