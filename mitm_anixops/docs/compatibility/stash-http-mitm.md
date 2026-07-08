# Stash HTTP MITM Source Contract

Capability: Stash `http.mitm` host policy metadata and `force-http-engine`
option metadata.

Ecosystem: `stash`.

Status: `partial`.

## Purpose

This contract establishes the first Stash parser-supported subset for the
v1.0.0 compatibility matrix. It covers only host policy metadata that maps
directly onto the policy-core MITM hostname matcher and the
`force-http-engine` boolean option as an adapter-visible QUIC fallback signal.

The source form is based on the Stash configuration example, which models HTTP
settings under a top-level `http:` YAML mapping and includes an `http.mitm`
host list:

```text
https://stash.wiki/en/configuration/example-config
```

This is not a general YAML parser contract.

## Input Forms

The parser accepts this narrow YAML shape:

```yaml
http:
  force-http-engine: true
  mitm:
    - stash.example.test
    - "*.stash.example.test"
    - -blocked.stash.example.test
    - weather-data.port-stash.test:*
```

Supported forms:

- top-level `http:`;
- `http.force-http-engine: true|false`;
- nested `mitm:`;
- list scalar host patterns below `mitm`;
- exact host patterns;
- wildcard host patterns already accepted by the policy core;
- deny host patterns using the existing `-host` marker after the YAML list
  marker;
- Stash `host:*` port-wildcard suffixes, normalized to host-only policy-core
  patterns because the current ABI has no port dimension.

Unsupported forms remain ignored or outside this contract:

- port-specific matching such as `host:443`;
- `url-rewrite` beyond the dedicated request URL rewrite subset contract,
  `script`, `cron`, `rules`, `proxies`, DNS, and routing keys;
- anchors, aliases, folded scalars, inline arrays, maps, or arbitrary YAML
  expressions.

## Parser Output

For valid `http.mitm` list entries, the parser must:

- register MITM hostname patterns through the existing policy-core host store;
- emit accepted diagnostics with section `MITM` and action `mitm`;
- emit accepted diagnostics with section `MITM` and action
  `force-http-engine` for the boolean option;
- map `force-http-engine: true` to the existing QUIC fallback decision signal
  (`ANIXOPS_MITM_REJECT_QUIC`) for matched, trusted MITM hosts;
- preserve existing exact, wildcard, deny-host, and normalized `host:*` MITM
  evaluation behavior;
- avoid registering rewrite rules, script rules, task descriptors, arguments,
  route policies, proxy nodes, DNS settings, or certificate lifecycle behavior.

Malformed host entries are rejected when the parser recognizes an `http.mitm`
list entry but the host pattern is invalid.

## Positive Case

Parser case:

```text
tests/fixtures/Stash.HttpMitm.yaml
tests/fixtures/Stash.HttpForceHttpEngine.yaml
```

Expected behavior:

- config load succeeds;
- four MITM host patterns are registered;
- `host:*` input is normalized to the host-only matched pattern;
- `force-http-engine: true` is accepted as adapter-visible metadata and causes
  QUIC evaluation for a matched trusted host to return the existing reject-QUIC
  policy-core decision;
- no rewrite, script, task, or argument entries are registered;
- exact and wildcard hosts can be intercepted only after the adapter supplies
  enabled MITM and trusted certificate state;
- deny hosts bypass with the existing deny-host reason.

## Negative Case

Parser case:

```text
tests/fixtures/Stash.HttpMitm.Malformed.yaml
tests/fixtures/Stash.HttpMitm.PortSpecificUnsupported.yaml
tests/fixtures/Stash.HttpForceHttpEngine.Malformed.yaml
```

Expected behavior:

- config load fails with `ANIXOPS_ERR_PARSE`;
- no MITM host patterns are registered;
- a rejected diagnostic is recorded with section `MITM` and action `mitm`;
- last error reports invalid MITM hostname context at the malformed line.
- port-specific `host:443` input loads as unsupported syntax, does not register
  any MITM host pattern, and records an ignored diagnostic.
- malformed `force-http-engine` values fail with `ANIXOPS_ERR_PARSE`, do not
  register later MITM hosts, and record a rejected diagnostic with action
  `force-http-engine`.

## Runtime And Security Boundary

Stash `http.mitm` parser support must not:

- install, trust, generate, rotate, revoke, or persist certificates;
- decrypt traffic for non-target hostnames;
- enable MITM without adapter-provided enabled and trusted certificate state;
- parse or execute Stash scripts;
- implement a Stash HTTP engine or transport fallback outside the existing
  policy-core QUIC decision signal;
- change DNS, routing, proxy, TUN, VPN, or packet-capture behavior;
- log sensitive headers or body content.

Certificate lifecycle and platform trust remain adapter/manual-intervention
responsibilities.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/stash_http_mitm_fixture_exposes_host_patterns`;
- `tests/test_config.c` registers
  `config/stash_http_mitm_malformed_fixture_rejects_invalid_host`;
- `tests/test_config.c` registers
  `config/stash_http_mitm_port_specific_fixture_stays_unsupported`;
- `tests/test_config.c` registers
  `config/stash_http_force_http_engine_fixture_exposes_quic_signal`;
- `tests/test_config.c` registers
  `config/stash_http_force_http_engine_malformed_fixture_rejects_invalid_bool`;
- `tests/test_config.c` keeps
  `config/stash_migration_guard_fixture_stays_parser_unsupported`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Stash HTTP MITM hosts
```

The row remains `partial` because only the `http.mitm` host-policy subset and
`force-http-engine` option metadata are covered. Full Stash YAML profiles,
port-specific matching, `rules`, `proxies`, DNS, routing, transport-level HTTP
engine behavior, transparent or expanded rewrite behavior, scripts, cron, UI,
and certificate lifecycle behavior remain unimplemented. The separate
[`stash-url-rewrite.md`](stash-url-rewrite.md) contract covers only
`http.url-rewrite` request URL policy intent.
