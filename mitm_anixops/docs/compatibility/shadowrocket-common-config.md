# Shadowrocket Common Config Source Contract

Capability: Shadowrocket common config.

Ecosystem: `shadowrocket`.

Status: `partial`.

## Purpose

This contract fixes a narrow P1 parser milestone for Shadowrocket-style common
configuration syntax that overlaps with the existing policy-core grammar. It
does not claim full Shadowrocket profile compatibility.

## Input Forms

The current common-config subset accepts:

- `[URL Rewrite]` URL redirect, reject, response mock body, request/response
  header mutation, and response body regex mutation rules;
- `[Script]` attr-list rules with `type`, `pattern`, `requires-body`,
  `timeout`, `timeout-ms`, `max-size`, `max_size`, `script-path`, and
  `argument`;
- `[MITM] hostname = ...` with exact, deny, and `%APPEND%` host entries;
- `[MITM] skip-server-cert-verify = ...` as an adapter-visible boolean;
- `[MITM] h2 = ...` as an adapter-visible HTTP/2 MITM flag;
- `[MITM] ca-p12`, `ca-passphrase`, and `ca-cert` as unsupported
  certificate-material diagnostics only.

## Parser Output

The parser must produce:

- accepted diagnostics for supported rewrite, script, and MITM lines;
- ignored diagnostics for unsupported certificate-material MITM lines;
- rejected diagnostics for malformed supported-section regex rules;
- URL rewrite rules, header mutation rules, body mutation rules, script trigger
  metadata, MITM host patterns, and MITM options observable through the public
  ABI.

## Positive Case

Parser case:

```text
tests/fixtures/Shadowrocket.CommonConfig.conf
tests/fixtures/Shadowrocket.RequestRewrite.conf
tests/fixtures/Shadowrocket.ResponseRewrite.conf
tests/fixtures/Shadowrocket.HeaderMutation.conf
tests/fixtures/Shadowrocket.BodyMutation.conf
```

Expected behavior:

- config load succeeds;
- two rewrite rules are registered;
- the dedicated request rewrite fixture registers two URL redirect/reject
  rules;
- the dedicated response rewrite fixture registers one response mock body rule;
- the dedicated header mutation fixture registers eight request/response
  header mutation rules;
- the dedicated body mutation fixture registers one response body regex
  mutation rule;
- two script rules are registered;
- three MITM host patterns are registered;
- `skip-server-cert-verify` and `h2` options are exposed to adapters;
- redirect, reject, response mock body, request header mutation, response header
  mutation, response body mutation, request script, response script, and MITM
  allow/deny behavior are observable through public ABI calls.

## Negative Case

Parser cases:

```text
tests/fixtures/Shadowrocket.CommonConfig.Malformed.conf
tests/fixtures/Shadowrocket.RequestRewrite.Malformed.conf
tests/fixtures/Shadowrocket.ResponseRewrite.Malformed.conf
tests/fixtures/Shadowrocket.HeaderMutation.Malformed.conf
tests/fixtures/Shadowrocket.BodyMutation.Malformed.conf
tests/fixtures/Shadowrocket.MitmCertificateUnsupported.conf
```

Expected behavior:

- malformed URL rewrite config load fails with `ANIXOPS_ERR_REGEX`;
- malformed dedicated request rewrite config load fails with
  `ANIXOPS_ERR_REGEX`;
- malformed dedicated response rewrite config load fails with
  `ANIXOPS_ERR_PARSE` under a strict compatibility profile;
- malformed dedicated header mutation config load fails with
  `ANIXOPS_ERR_REGEX`;
- malformed dedicated body mutation config load fails with `ANIXOPS_ERR_REGEX`;
- malformed URL rewrite config records a rejected rule diagnostic with section
  `Rewrite` and action `rewrite`;
- malformed URL rewrite config last error reports regex failure at the
  malformed line;
- certificate-material config load succeeds;
- `ca-p12`, `ca-passphrase`, and `ca-cert` record ignored diagnostics, do not
  enable skip-server-cert-verify, do not establish trust, and keep a matching
  host as a conservative MITM bypass until the adapter supplies trusted
  certificate state.

## App Profile Boundary

This contract does not implement general Shadowrocket app-level profile
behavior. `[Rule]` URL-regex reject policy intent is covered separately by
[`shadowrocket-rule-reject.md`](shadowrocket-rule-reject.md). The existing
migration guard fixture remains valid for unsupported sections and route
selection syntax such as `[General]`, `[Proxy]`, and `[Rule]` direct/proxy
routes.

Out-of-scope behavior includes:

- proxy node formats;
- rule routing and policy groups;
- DNS, VPN, packet-capture, or TUN behavior;
- profile import UI;
- certificate installation or trust lifecycle;
- certificate material loading or passphrase use;
- JavaScript runtime execution;
- task/cron scheduler behavior.

## Runtime And Security Boundary

This contract covers parser and policy-core output only.

It does not implement:

- TLS interception;
- certificate generation or trust installation;
- certificate material loading;
- HTTP parser or body streaming;
- JavaScript execution;
- remote script download or cache refresh.

Those remain runner, runtime, scheduler, or adapter responsibilities.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/shadowrocket_common_config_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/shadowrocket_common_config_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/shadowrocket_request_rewrite_fixture_maps_redirect_and_reject`;
- `tests/test_config.c` registers
  `config/shadowrocket_request_rewrite_malformed_fixture_rejects_invalid_url_regex`;
- `tests/test_config.c` registers
  `config/shadowrocket_response_rewrite_fixture_maps_mock_response_body`;
- `tests/test_config.c` registers
  `config/shadowrocket_response_rewrite_malformed_fixture_rejects_missing_body`;
- `tests/test_config.c` registers
  `config/shadowrocket_header_mutation_fixture_maps_header_rewrites`;
- `tests/test_config.c` registers
  `config/shadowrocket_header_mutation_malformed_fixture_rejects_invalid_header_regex`;
- `tests/test_config.c` registers
  `config/shadowrocket_body_mutation_fixture_maps_response_body_regex`;
- `tests/test_config.c` registers
  `config/shadowrocket_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/shadowrocket_mitm_certificate_unsupported_fixture_keeps_material_ignored`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Shadowrocket common config
```

The row remains `partial` because app-level profile grammar, routing, DNS,
proxy nodes, certificate lifecycle, certificate material loading, and runtime
behavior are not covered.
