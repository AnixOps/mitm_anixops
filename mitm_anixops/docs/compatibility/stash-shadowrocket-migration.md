# Stash And Shadowrocket Migration Notes

Capability: migration notes for Stash and Shadowrocket app-profile
compatibility.

Ecosystem: `stash`, `shadowrocket`.

Status: Stash `planned`; Shadowrocket common config `partial`; Shadowrocket app
profile `planned`.

```text
stash-compatibility-mode=migration-notes-only
shadowrocket-common-config-mode=partial-parser-support
shadowrocket-app-profile-mode=migration-guard-only
stash-parser-fixtures=migration-guard-only
shadowrocket-app-profile-fixtures=migration-guard-only
shadowrocket-common-config-fixtures=positive-and-negative-parser-fixtures
stash-support-claim=forbidden-until-dedicated-contract-fixtures-and-tests
shadowrocket-expanded-support-claim=forbidden-outside-common-config-contract
```

## Purpose

This note defines the current v1.0.0 boundary for Stash and Shadowrocket:

- Stash remains migration notes only.
- Shadowrocket has a dedicated common-config source contract for a narrow
  `[URL Rewrite]`, `[Script]`, and `[MITM]` subset.
- Shadowrocket app-level profile syntax remains a migration guard unless a
  future source contract, fixture pair, positive test, negative test,
  compatibility matrix row, and GitHub Actions evidence explicitly cover it.

The current migration guard fixtures are non-support evidence:

- `tests/fixtures/Stash.MigrationGuard.yaml`;
- `tests/fixtures/Shadowrocket.MigrationGuard.conf`.

The dedicated Shadowrocket common-config fixtures are support evidence only for
the subset described in
[`shadowrocket-common-config.md`](shadowrocket-common-config.md):

- `tests/fixtures/Shadowrocket.CommonConfig.conf`;
- `tests/fixtures/Shadowrocket.CommonConfig.Malformed.conf`.

## Current Claim

Allowed statements:

- Stash is a migration target.
- Shadowrocket app-level profile syntax remains a migration target outside the
  common-config contract.
- Shadowrocket common config has partial parser support for documented URL
  rewrite, script metadata, and MITM host/options syntax.
- Existing Loon, Quantumult X, Surge, Shadowrocket common-config, and portable
  AnixOps fixtures can inform how overlapping rules should be translated.
- Operators may manually map overlapping rules into a currently covered fixture
  profile for evaluation.

Forbidden statements:

- Stash parser support is implemented.
- Full Shadowrocket parser support is implemented.
- Shadowrocket `[General]`, `[Rule]`, `[Proxy]`, DNS, routing, profile UI, or
  proxy-node behavior is implemented.
- App-level routing, DNS, proxy-node, certificate, UI, or script runtime
  behavior is supported by the C policy core.

## CI Evidence

Current CI evidence:

- `config/stash_migration_guard_fixture_stays_parser_unsupported`;
- `config/shadowrocket_migration_guard_fixture_stays_parser_unsupported`;
- `config/shadowrocket_common_config_fixture_is_supported`;
- `config/shadowrocket_common_config_fixture_rejects_invalid_regex`;
- GitHub Actions `governance` requires the migration notes, common-config
  contract, fixtures, matrix rows, manual-intervention markers, and test
  registrations;
- GitHub Actions `linux-test` runs `sh scripts/check.sh`.

Expected Stash guard behavior:

- config load succeeds in the portable profile;
- no rewrite rules, script rules, MITM host patterns, or argument defaults are
  registered;
- parser diagnostics record ignored app-level profile lines;
- no route, proxy-node, DNS, certificate, UI, or runtime behavior is claimed.

Expected Shadowrocket app-profile guard behavior:

- config load succeeds in the portable profile;
- `[General]`, `[Rule]`, and `[Proxy]` lines remain ignored;
- no rewrite rules, script rules, MITM host patterns, or argument defaults are
  registered from that guard fixture;
- no app-level profile behavior is claimed.

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

## Entry Criteria For Expanded Parser Support

Before Stash can move beyond `planned`, or before Shadowrocket app-profile
support can move beyond the common-config contract, a future change must add:

1. A dedicated source contract for the target ecosystem surface.
2. At least one redistributable positive fixture for supported syntax.
3. At least one negative fixture for malformed or unsupported syntax.
4. C parser tests that load or reject those fixtures.
5. Compatibility matrix updates that name the fixture and test IDs.
6. GitHub Actions governance checks proving the contract, fixture, tests, and
   matrix row stay linked.

The current Stash and Shadowrocket app-profile migration guard fixtures do not
satisfy those entry criteria. Keep those surfaces as migration notes only until
dedicated parser evidence exists.
