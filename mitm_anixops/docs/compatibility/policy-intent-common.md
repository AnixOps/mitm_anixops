# Policy Intent Common Source Contract

Capability: reject/direct/proxy policy intent.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for routing-like policy intent
tokens that commonly appear near URL rewrite rules. It deliberately separates
the currently implemented policy-core `reject` subset from adapter-owned
`direct` and `proxy` route selection.

The policy core may return structured reject decisions. It must not claim,
select, or enforce direct/proxy upstream routing until a host adapter contract
and CI evidence exist.

## Input Forms

The current common subset accepts these reject forms in `[Rewrite]`,
`[URL Rewrite]`, `[Remote Rewrite]`, and compatible rewrite aliases:

- `pattern reject`;
- `pattern reject-current-session`;
- `pattern reject-200`;
- `pattern reject-NNN` for numeric HTTP status codes from `100` through `599`;
- `pattern reject-img` and `pattern reject-tinygif`;
- `pattern reject-video`;
- `pattern reject-dict`;
- `pattern reject-array`.

The current common subset does not accept:

- `pattern direct`;
- `pattern proxy`;
- proxy group names;
- route priority, fallback, or upstream selection metadata.

## Parser Output

The parser must produce:

- accepted diagnostics for supported reject lines;
- request-phase rewrite rules observable through `anixops_rewrite_evaluate_url`;
- reject action/status-code fields through the public ABI;
- ignored diagnostics for unsupported direct/proxy route-intent lines in the
  portable profile;
- no rewrite rules for unsupported direct/proxy route-intent lines.

## Positive Case

Parser case:

```text
tests/fixtures/PolicyIntent.Common.conf
```

Expected behavior:

- config load succeeds;
- four rewrite rules are registered;
- `reject`, `reject-410`, `reject-200`, and `reject-img` expose stable action
  and status-code fields;
- no script or MITM patterns are registered by the policy-intent fixture;
- response phase does not trigger request-phase reject rules.

## Negative Case

Parser case:

```text
tests/fixtures/PolicyIntent.Unsupported.conf
```

Expected behavior:

- config load succeeds in the portable profile;
- `direct` and `proxy` route-intent lines are ignored with diagnostics;
- no rewrite rules are registered;
- URL evaluation for those routes returns `ANIXOPS_REWRITE_NONE`.

Strict compatibility profiles may reject unsupported rule-shaped lines earlier,
but they still must not claim direct/proxy routing support.

## Runtime And Security Boundary

This contract covers policy-core parser and URL decision fields only.

It does not implement:

- upstream direct/proxy route selection;
- proxy group resolution;
- DNS strategy, failover, or load balancing;
- socket creation, network I/O, or packet forwarding;
- platform VPN or network-extension policy;
- hidden traffic interception or route bypass.

Adapters remain responsible for any future route-selection contract and must
record platform capability confirmation in `docs/manual-intervention.md` before
claiming production route behavior.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/policy_intent_common_fixture_covers_reject_subset`;
- `tests/test_config.c` registers
  `config/policy_intent_unsupported_routes_are_ignored`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
reject/direct/proxy policy intent
```

The row remains `partial` until direct/proxy route selection has a source
contract, adapter implementation, positive and negative tests, and GitHub
Actions evidence.
