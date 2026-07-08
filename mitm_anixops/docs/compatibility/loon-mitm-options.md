# Loon MITM Options Source Contract

Capability: Loon `[MITM]` host and adapter-visible option parsing.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the Loon-specific parser evidence for common `[MITM]`
host/options lines. It exposes conservative policy-core and adapter-visible
signals only; it does not claim certificate installation, trust mutation, TLS
interception, or HTTP/2 transport support.

## Input Forms

The parser accepts these lines inside `[MITM]`:

```text
[MITM]
enable = true
hostname = %APPEND% api.example.test, *.wild.example.test, -blocked.example.test
skip-server-cert-verify = cert.example.test
h2 = false
disable-quic = false
```

Supported fields:

- `enable` and `enabled`;
- `hostname` with exact hosts, wildcard hosts, deny-prefixed hosts, comma
  separation, and optional `%APPEND%` / `%INSERT%` prefixes;
- `skip-server-cert-verify` and `skip_server_cert_verify` as adapter-readable
  boolean or host-list intent;
- `h2`, `h2-enable`, and `h2_enable` as adapter-readable HTTP/2 policy
  metadata;
- `disable-quic`, `disable_quic`, `disable-mitm-quic`, and
  `disable_mitm_quic` as the policy-core QUIC fallback decision signal.

## Parser Output

For valid option lines, the parser must:

- register MITM host patterns from `hostname`;
- expose `skip-server-cert-verify` through
  `anixops_engine_skip_server_cert_verify`;
- expose HTTP/2 policy metadata through `anixops_engine_h2_mitm_enabled`;
- apply `enable` to the MITM decision gate;
- apply `disable-quic` to the existing QUIC fallback decision signal;
- emit accepted diagnostics with section `MITM`.

Malformed host lists are rejected when hostname validation fails. No later
option lines are consumed after a parse failure.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.MitmOptions.plugin
```

Expected behavior:

- config load succeeds;
- three MITM host patterns are registered;
- `skip-server-cert-verify` is exposed as enabled;
- `h2 = false` is exposed to adapters;
- `enable = true` allows trusted matching hosts to intercept;
- `disable-quic = false` keeps QUIC evaluation as an intercept decision for a
  trusted matching host instead of forcing the fallback reject signal;
- deny-prefixed hosts bypass interception.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.MitmOptions.Malformed.plugin
```

Expected behavior:

- `ANIXOPS_COMPAT_LOON_STRICT` rejects the malformed hostname line;
- no MITM host patterns are registered from the malformed host list;
- later options are not consumed after the parse failure;
- a rejected diagnostic is recorded with section `MITM` and action `hostname`.

## Unsupported Case

Parser case:

```text
tests/fixtures/Loon.MitmCertificateUnsupported.plugin
```

Expected behavior:

- config load succeeds in the portable profile;
- `ca-p12`, `ca-passphrase`, and `ca-cert` lines record ignored diagnostics;
- certificate material lines do not set `skip-server-cert-verify`, trust state,
  or any rewrite, script, task, route, DNS, proxy, UI, or certificate lifecycle
  behavior;
- a matched hostname still bypasses interception until the adapter explicitly
  supplies a trusted certificate state.

## Runtime And Security Boundary

This contract covers parser and policy-core output only.

It does not implement:

- root CA generation or installation;
- platform trust-store mutation;
- dynamic leaf certificate signing;
- TLS interception or HTTP/2 transport;
- certificate verification bypass behavior in a host adapter;
- QUIC network fallback behavior outside the policy-core decision signal.

Certificate lifecycle requirements remain in
[Certificate Lifecycle](../architecture/certificate-lifecycle.md), and pending
platform work remains recorded in
[manual-intervention.md](../manual-intervention.md).

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_mitm_options_fixture_exposes_adapter_flags`;
- `tests/test_config.c` registers
  `config/loon_mitm_options_malformed_fixture_rejects_invalid_host`;
- `tests/test_config.c` registers
  `config/loon_mitm_certificate_unsupported_fixture_keeps_material_ignored`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon MITM options
```

The row remains `partial` because parser and policy-core signals plus
certificate-material non-support guards are covered, but platform certificate
lifecycle, trust mutation, TLS interception, and adapter network behavior
remain unimplemented.
