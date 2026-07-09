# Certificate Lifecycle Architecture Contract

Status: accepted for the v1.0.0 policy-core track.

Decision date: 2026-07-08.

## Decision

`mitm_anixops` owns the MITM policy decision, not production certificate
lifecycle management.

The current certificate lifecycle decision is:

```text
certificate-lifecycle-policy-core-ca-trust=adapter-supplied
certificate-lifecycle-automatic-root-trust=forbidden
certificate-lifecycle-non-target-host-decryption=forbidden
certificate-lifecycle-signing-materials=manual-intervention-only
certificate-lifecycle-production-store-mutation=adapter-owned
```

The C policy core may intercept only when all policy gates pass:

- MITM is enabled;
- the adapter reports `ANIXOPS_CERT_TRUSTED`;
- the runtime hostname is syntactically valid;
- the hostname matches an allow pattern;
- no deny pattern matches;
- QUIC is rejected or disabled according to the configured policy.

## Owned By The Policy Core

- Parse MITM hostname rules and host-list options.
- Normalize and match hostnames conservatively.
- Consume adapter-supplied certificate trust state through
  `anixops_engine_set_cert_state`.
- Return `ANIXOPS_MITM_BYPASS`, `ANIXOPS_MITM_INTERCEPT`, or
  `ANIXOPS_MITM_REJECT_QUIC` with a reason.
- Bypass malformed, empty, denied, non-target, or untrusted hosts.
- Expose `skip-server-cert-verify` as adapter-readable policy metadata.

## Owned By Platform Adapters

- Root CA generation.
- Root CA private-key storage, rotation, export, backup, and destruction.
- Dynamic leaf certificate generation and signing.
- System, browser, mobile, enterprise, or app-scoped certificate store
  mutation.
- User consent, trust UI, revocation, removal, and rollback.
- Protected runtime environments for secrets and signing materials.
- Platform entitlement, store review, MDM, or enterprise policy compliance.

## Lifecycle States

Future production adapters must document these states before claiming
certificate lifecycle support:

- no CA material exists;
- CA generated but not installed;
- CA installed but untrusted;
- CA trusted by the target platform store;
- leaf certificate generated for a target hostname;
- CA revoked or removed;
- trust rollback completed.

The policy core currently models only the adapter-supplied trust gate:

- `ANIXOPS_CERT_UNKNOWN`;
- `ANIXOPS_CERT_NOT_INSTALLED`;
- `ANIXOPS_CERT_INSTALLED_UNTRUSTED`;
- `ANIXOPS_CERT_TRUSTED`;
- `ANIXOPS_CERT_REVOKED`.

Only `ANIXOPS_CERT_TRUSTED` can allow interception, and only for a configured
target hostname.

## Security Rules

- No code path may automatically trust or install a root CA.
- No code path may decrypt a non-target hostname.
- No code path may bypass user consent or platform permission requirements.
- No signing key, private key, certificate secret, provisioning profile, or
  platform credential may be committed to this repository.
- No sensitive request or response header/body content may be logged by default.
- `skip-server-cert-verify` is parser output for adapters; it is not authority
  to skip root trust, platform consent, or hostname targeting.

## Positive Evidence

Current CI evidence for the policy-core trust gate:

- `mitm/certificate_state_matrix_blocks_untrusted_states`;
- `mitm/wildcard_matches_subdomains_without_base_domain`;
- `mitm/hostname_policy_normalizes_trailing_dot_port_and_deny_wildcard_boundary`;
- `config/loon_mitm_options_fixture_exposes_adapter_flags`;
- `config/config_accepts_quantumultx_mitm_host_options`;
- `config/config_exposes_skip_server_cert_verify`.

## Negative Evidence

Current CI evidence for conservative bypass:

- `mitm/no_host_match_bypasses`;
- `mitm/deny_pattern_wins_over_allow_pattern`;
- `mitm/malformed_runtime_hosts_do_not_intercept_even_with_wildcard`;
- `config/mitm_hostname_malformed_fixture_rejects_invalid_host`;
- `scripts/security-claim-check.sh` prevents accidental documentation claims
  of automatic root trust or non-target hostname decryption.

## Manual Intervention

Production certificate lifecycle support is blocked by pending markers in
[manual-intervention.md](../manual-intervention.md):

- `ca-root-generation-policy-status=pending`;
- `platform-certificate-store-status=pending`;
- `protected-runtime-environment-status=pending`;
- `mitm-signing-materials-status=pending`;
- `certificate-trust-ux-status=pending`.

These markers must remain pending until a human or platform owner documents the
external policy, credential handling, platform store behavior, and rollback
flow. They must not be used to skip CI or to claim production certificate
lifecycle support.

## CI Evidence

GitHub Actions governance must prove:

- this contract exists;
- the manual-intervention markers exist;
- `scripts/security-claim-check.sh` passes;
- P4 TODO items for the certificate lifecycle contract, manual markers, and
  safety claim checks are marked complete.

`linux-test` also runs `sh scripts/check.sh`, which invokes the same security
claim check before build/test/package validation.

## Unimplemented Items

- production root CA generation;
- platform certificate store integration;
- protected storage for CA private keys;
- dynamic leaf certificate generation;
- trust install, revoke, remove, and rollback workflows;
- OS/browser/mobile/enterprise policy mapping;
- signing and notarization for platform-specific artifacts.
