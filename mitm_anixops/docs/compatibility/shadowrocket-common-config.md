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

- `[URL Rewrite]` URL redirect and reject rules;
- `[Script]` attr-list rules with `type`, `pattern`, `requires-body`,
  `timeout`, `timeout-ms`, `max-size`, `max_size`, `script-path`, and
  `argument`;
- `[MITM] hostname = ...` with exact, deny, and `%APPEND%` host entries;
- `[MITM] skip-server-cert-verify = ...` as an adapter-visible boolean;
- `[MITM] h2 = ...` as an adapter-visible HTTP/2 MITM flag.

## Parser Output

The parser must produce:

- accepted diagnostics for supported rewrite, script, and MITM lines;
- rejected diagnostics for malformed supported-section regex rules;
- URL rewrite rules, script trigger metadata, MITM host patterns, and MITM
  options observable through the public ABI.

## Positive Case

Parser case:

```text
tests/fixtures/Shadowrocket.CommonConfig.conf
```

Expected behavior:

- config load succeeds;
- two rewrite rules are registered;
- two script rules are registered;
- three MITM host patterns are registered;
- `skip-server-cert-verify` and `h2` options are exposed to adapters;
- redirect, reject, request script, response script, and MITM allow/deny
  behavior are observable through public ABI calls.

## Negative Case

Parser case:

```text
tests/fixtures/Shadowrocket.CommonConfig.Malformed.conf
```

Expected behavior:

- config load fails with `ANIXOPS_ERR_REGEX`;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports regex failure at the malformed line.

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
- JavaScript runtime execution;
- task/cron scheduler behavior.

## Runtime And Security Boundary

This contract covers parser and policy-core output only.

It does not implement:

- TLS interception;
- certificate generation or trust installation;
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
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Shadowrocket common config
```

The row remains `partial` because app-level profile grammar, routing, DNS,
proxy nodes, certificate lifecycle, and runtime behavior are not covered.
