# Loon Rule Routing Guard Source Contract

Capability: Loon `[Rule]` route-selection and DNS non-support guard.

Ecosystem: `loon`.

Status: `planned`.

## Purpose

This contract records the current v1.0.0 boundary for Loon route-like
`[Rule]` entries. The policy core may parse Loon reject policy intent through
the existing rewrite decision ABI, but it must not claim direct/proxy upstream
selection, IP routing, GEOIP behavior, or DNS/no-resolve handling until an
adapter route-selection contract exists.

## Input Forms

The guard fixture covers representative unsupported route and DNS shapes:

- `DOMAIN-SUFFIX,<host>,DIRECT`;
- `DOMAIN,<host>,PROXY`;
- `IP-CIDR,<cidr>,DIRECT,no-resolve`;
- `GEOIP,<country>,PROXY`;
- `FINAL,DIRECT`.

## Guard Case

Parser case:

```text
tests/fixtures/Loon.RuleRouteUnsupported.plugin
```

Expected behavior:

- config load succeeds;
- every route-like line is recorded as an ignored diagnostic in section `Rule`;
- no rewrite, script, task, argument, or MITM state is registered;
- URL evaluation for guarded hosts returns `ANIXOPS_REWRITE_NONE`.

## Runtime And Security Boundary

This guard does not implement:

- direct/proxy route selection;
- proxy group resolution;
- IP-CIDR or GEOIP matching;
- DNS strategy, no-resolve semantics, failover, or load balancing;
- rule-provider refresh;
- platform VPN, network-extension, or socket routing behavior.

`docs/manual-intervention.md` keeps
`route-selection-adapter-status=pending` as the required gate before claiming
route-selection support.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_rule_route_unsupported_fixture_keeps_routes_ignored`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon rule route-selection guard
```

The row remains `planned` until route selection has an adapter-owned source
contract, implementation, positive and negative tests, and GitHub Actions
evidence.
