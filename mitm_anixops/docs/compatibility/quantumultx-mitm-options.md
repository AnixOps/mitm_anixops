# Quantumult X MITM Options Source Contract

Capability: Quantumult X MITM host and adapter-visible option parsing.

Ecosystem: `quantumultx`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for Quantumult X `[mitm]` and
`#[mitm]` host/options lines. It exposes policy-core signals that a host
adapter may consume, without claiming certificate installation, trust mutation,
or TLS interception support.

## Input Forms

The parser accepts:

- `[mitm]` and `#[mitm]` section aliases;
- `enable = true|false`;
- `hostname = ...` with optional `%APPEND%` or `%INSERT%`;
- `force-http-engine-hosts = ...` with optional `%APPEND%` or `%INSERT%`;
- `skip-server-cert-verify = true|false` or a non-empty host list;
- `skip_server_cert_verify = true|false`;
- `h2`, `h2-enable`, and `h2_enable`;
- `disable-quic`, `disable_quic`, `disable-mitm-quic`, and
  `disable_mitm_quic`.

Host lists accept exact hosts, wildcard hosts, deny prefixes, and comma
separation under the same hostname validation used by the MITM host policy.

## Parser Output

The parser must produce:

- accepted diagnostics for supported MITM option lines;
- MITM host patterns from `hostname` and `force-http-engine-hosts`;
- an adapter-visible `skip-server-cert-verify` flag;
- adapter-visible HTTP/2 and QUIC fallback flags;
- rejected diagnostics and parse failure for malformed host patterns.

## Positive Case

Parser case:

```text
tests/fixtures/QuantumultX.MitmOptions.snippet
```

Expected behavior:

- config load succeeds;
- one `hostname` and two `force-http-engine-hosts` host patterns are
  registered;
- `skip-server-cert-verify` is exposed as enabled;
- `h2 = false` is exposed to adapters;
- `disable-quic = false` allows QUIC evaluation to continue as an intercept
  decision for a trusted matching host;
- exact and wildcard force-host entries match through `anixops_mitm_evaluate`.

## Negative Case

Parser case:

```text
tests/fixtures/QuantumultX.MitmOptions.Malformed.snippet
```

Expected behavior:

- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects the malformed
  `force-http-engine-hosts` host list;
- no MITM host patterns are registered from the malformed host list;
- later options are not consumed after the parse failure;
- a rejected diagnostic is recorded with section `MITM` and action
  `force-http-engine-hosts`.

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
  `config/quantumultx_mitm_options_fixture_exposes_adapter_flags`;
- `tests/test_config.c` registers
  `config/quantumultx_mitm_options_malformed_fixture_rejects_invalid_host`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Quantumult X MITM options
```

The row remains `partial` because parser and policy-core signals are covered,
but platform certificate lifecycle, trust mutation, TLS interception, and
adapter network behavior remain unimplemented.
