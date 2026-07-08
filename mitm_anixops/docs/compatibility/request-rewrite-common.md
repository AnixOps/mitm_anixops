# Request Rewrite Common Source Contract

Capability: request URL rewrite.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for the common request URL
rewrite subset. It covers parser and policy-core behavior only; it does not
claim adapter routing, upstream proxy selection, or platform HTTP pipeline
behavior.

## Input Forms

The current common subset accepts:

- `[Rewrite]`, `[URL Rewrite]`, and `[Remote Rewrite]` section aliases;
- direct redirect lines such as `pattern replacement 302`;
- URL-prefixed redirect lines such as `pattern url 303 replacement`;
- reject lines such as `pattern reject-200`;
- regex capture expansion through `$1`, `${1}`, and compatible replacement
  forms already covered by the rewrite tests.

## Parser Output

The parser must produce:

- accepted diagnostics for supported rewrite lines;
- ignored diagnostics for incomplete rules under the portable profile;
- rejected diagnostics for malformed supported-section rules under strict
  compatibility profiles;
- request-phase URL rewrite rules observable through the public ABI.

## Positive Case

Parser case:

```text
tests/fixtures/RequestRewrite.Common.conf
tests/fixtures/Loon.RequestRewrite.plugin
```

Expected behavior:

- config load succeeds;
- three rewrite rules are registered;
- direct redirect, URL-prefixed redirect, and reject behavior are observable
  through `anixops_rewrite_evaluate_url`;
- Loon `[URL Rewrite]` redirect and reject behavior is observable through
  `anixops_rewrite_evaluate_url`;
- redirect capture expansion is applied to the output value;
- response phase does not trigger request rewrite rules.

## Negative Case

Parser case:

```text
tests/fixtures/RequestRewrite.Common.Malformed.conf
tests/fixtures/Loon.RequestRewrite.Malformed.plugin
```

Expected behavior:

- a strict compatibility profile rejects the malformed rewrite line;
- an invalid Loon `[URL Rewrite]` URL regex rejects config load;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers request URL rewrite selection in the policy core.

It does not implement:

- HTTP parsing, request serialization, or network I/O;
- upstream proxy, direct, or route selection;
- response rewrite behavior;
- header or body mutation behavior;
- remote rule download;
- JavaScript execution;
- TLS interception or certificate lifecycle.

Those remain adapter, runner, or separate policy contracts.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/request_rewrite_common_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/request_rewrite_common_strict_fixture_rejects_malformed_rule`;
- `tests/test_config.c` registers
  `config/loon_request_rewrite_fixture_maps_redirect_and_reject`;
- `tests/test_config.c` registers
  `config/loon_request_rewrite_malformed_fixture_rejects_invalid_url_regex`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
request rewrite
```

The row remains `partial` until broader ecosystem grammar, remote rule refresh,
and direct/proxy policy intent mapping exist.
