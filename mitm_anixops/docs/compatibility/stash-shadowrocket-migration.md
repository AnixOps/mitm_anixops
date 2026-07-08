# Stash And Shadowrocket Migration Notes

Capability: migration notes for Stash and Shadowrocket compatibility.

Ecosystem: `stash`, `shadowrocket`.

Status: `planned`.

```text
stash-compatibility-mode=migration-notes-only
shadowrocket-compatibility-mode=migration-notes-only
stash-shadowrocket-parser-fixtures=migration-guard-only
stash-shadowrocket-support-claim=forbidden-until-dedicated-contract-fixtures-and-tests
```

## Purpose

This note defines the current v1.0.0 boundary for Stash and Shadowrocket:
neither is a first-class parser target yet. Any overlapping syntax must be
treated as migration guidance until this repository adds redistributable parser
fixtures, positive tests, negative tests, compatibility matrix evidence, and
GitHub Actions coverage.

The current fixtures are non-support migration guards only:

- `tests/fixtures/Stash.MigrationGuard.yaml`;
- `tests/fixtures/Shadowrocket.MigrationGuard.conf`.

They prove representative app-level profile syntax stays ignored by the current
policy-core parser. They do not prove Stash or Shadowrocket compatibility.

## Current Claim

Allowed statements:

- Stash and Shadowrocket are migration targets.
- Existing Loon, Quantumult X, Surge, and portable AnixOps fixtures can inform
  how overlapping rules should be translated.
- Operators may manually map overlapping rules into a currently covered fixture
  profile for evaluation.

Forbidden statements:

- Stash parser support is implemented.
- Shadowrocket parser support is implemented.
- Stash or Shadowrocket behavior is compatible without dedicated fixtures.
- App-level routing, DNS, proxy-node, certificate, UI, or script runtime behavior
  is supported by the C policy core.

## CI Evidence

Current CI evidence:

- `config/stash_migration_guard_fixture_stays_parser_unsupported`;
- `config/shadowrocket_migration_guard_fixture_stays_parser_unsupported`;
- GitHub Actions `governance` requires the migration notes, guard fixtures,
  matrix rows, manual-intervention marker, and test registrations;
- GitHub Actions `linux-test` runs `sh scripts/check.sh`.

Expected parser behavior:

- config load succeeds in the portable profile;
- no rewrite rules, script rules, MITM host patterns, or argument defaults are
  registered;
- parser diagnostics record ignored app-level profile lines;
- no route, proxy-node, DNS, certificate, UI, or runtime behavior is claimed.

## Migration Mapping

Overlapping syntax should be mapped only into already covered policy-core
concepts:

- MITM host allow/deny lists;
- request URL redirect or reject;
- response rewrite where it maps to the documented response rewrite subset;
- request and response header add, replace, replace-regex, or delete;
- request and response body regex replacement;
- HTTP request/response script dispatch metadata;
- argument defaults that can be represented through the current `[Argument]`
  contract.

Any behavior outside those concepts remains out of scope until a source contract
and fixtures exist.

## Non-Goals

These notes do not cover:

- app UI or profile import behavior;
- proxy node formats or routing policy;
- rule-set subscription refresh;
- DNS, TUN, VPN, or packet-capture behavior;
- certificate installation, trust, revoke, or rollback;
- JavaScript runtime choice or Web API compatibility;
- task/cron scheduler behavior;
- platform-specific storage behavior.

## Entry Criteria For Parser Support

Before either ecosystem can move beyond `planned`, a future change must add:

1. A dedicated source contract for the target ecosystem.
2. At least one redistributable positive fixture for supported syntax.
3. At least one negative fixture for malformed or unsupported syntax.
4. C parser tests that load or reject those fixtures.
5. Compatibility matrix updates that name the fixture and test IDs.
6. GitHub Actions governance checks proving the contract, fixture, tests, and
   matrix row stay linked.

The current migration guard fixtures do not satisfy those entry criteria.
Until dedicated parser evidence exists, keep both rows as migration notes only.
