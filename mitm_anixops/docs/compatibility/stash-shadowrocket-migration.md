# Stash And Shadowrocket Migration Notes

Capability: migration notes for Stash and Shadowrocket app-profile
compatibility.

Ecosystem: `stash`, `shadowrocket`.

Status: Stash `http.mitm` `partial`; Stash URL rewrite request `partial`;
remaining Stash app-profile syntax `planned`; Shadowrocket common config
`partial`; Shadowrocket rule reject `partial`; remaining Shadowrocket app
profile syntax `planned`.

```text
stash-http-mitm-mode=partial-parser-support
stash-url-rewrite-mode=partial-parser-support
stash-app-profile-mode=url-rewrite-plus-migration-guard
shadowrocket-common-config-mode=partial-parser-support
shadowrocket-rule-reject-mode=partial-parser-support
shadowrocket-app-profile-mode=rule-reject-plus-migration-guard
stash-http-mitm-fixtures=positive-and-negative-parser-fixtures
stash-http-force-http-engine-fixtures=positive-and-negative-parser-fixtures
stash-url-rewrite-fixtures=positive-and-negative-parser-fixtures
stash-app-profile-fixtures=url-rewrite-plus-migration-guard
shadowrocket-rule-reject-fixtures=positive-and-negative-parser-fixtures
shadowrocket-app-profile-fixtures=rule-reject-plus-migration-guard
shadowrocket-common-config-fixtures=positive-and-negative-parser-fixtures
stash-expanded-support-claim=forbidden-outside-http-mitm-and-url-rewrite-contracts
shadowrocket-expanded-support-claim=forbidden-outside-common-config-and-rule-reject-contracts
```

## Purpose

This note defines the current v1.0.0 boundary for Stash and Shadowrocket:

- Stash has a dedicated HTTP MITM source contract for a narrow `http.mitm`
  host-policy subset.
- Stash has a dedicated URL rewrite source contract for a narrow
  `http.url-rewrite` reject and 302/307 redirect subset.
- Shadowrocket has a dedicated common-config source contract for a narrow
  `[URL Rewrite]`, `[Script]`, and `[MITM]` subset.
- Shadowrocket has a dedicated rule-reject source contract for a narrow
  `[Rule]` `URL-REGEX`, `DOMAIN`, `DOMAIN-KEYWORD`, and `DOMAIN-SUFFIX`
  reject subset.
- Stash app-level routing, proxy, DNS, UI, script, cron, transparent rewrite,
  and expanded redirect rewrite
  syntax remains a migration guard unless a future source contract, fixture
  pair, positive test, negative test, compatibility matrix row, and GitHub
  Actions evidence explicitly cover it.
- Shadowrocket app-level profile syntax outside common config and `[Rule]`
  URL-regex/domain/domain-keyword/domain-suffix reject remains a migration guard
  unless a future source contract, fixture pair, positive test, negative test,
  compatibility matrix row, and GitHub Actions evidence explicitly cover it.

The current migration guard fixtures are non-support evidence:

- `tests/fixtures/Stash.MigrationGuard.yaml`;
- `tests/fixtures/Shadowrocket.MigrationGuard.conf`.

The dedicated Shadowrocket common-config fixtures are support evidence only for
the subset described in
[`shadowrocket-common-config.md`](shadowrocket-common-config.md):

- `tests/fixtures/Shadowrocket.CommonConfig.conf`;
- `tests/fixtures/Shadowrocket.CommonConfig.Malformed.conf`.

The dedicated Shadowrocket rule-reject fixtures are support evidence only for
the subset described in
[`shadowrocket-rule-reject.md`](shadowrocket-rule-reject.md):

- `tests/fixtures/Shadowrocket.RuleReject.conf`;
- `tests/fixtures/Shadowrocket.RuleReject.Malformed.conf`;
- `tests/fixtures/Shadowrocket.RuleDomainExactReject.conf`;
- `tests/fixtures/Shadowrocket.RuleDomainExactReject.Malformed.conf`;
- `tests/fixtures/Shadowrocket.RuleDomainKeywordReject.conf`;
- `tests/fixtures/Shadowrocket.RuleDomainKeywordReject.Malformed.conf`;
- `tests/fixtures/Shadowrocket.RuleDomainReject.conf`;
- `tests/fixtures/Shadowrocket.RuleDomainReject.Malformed.conf`.

The dedicated Stash HTTP MITM fixtures are support evidence only for the subset
described in [`stash-http-mitm.md`](stash-http-mitm.md):

- `tests/fixtures/Stash.HttpMitm.yaml`;
- `tests/fixtures/Stash.HttpMitm.Malformed.yaml`.

The dedicated Stash URL rewrite fixtures are support evidence only for the
subset described in [`stash-url-rewrite.md`](stash-url-rewrite.md):

- `tests/fixtures/Stash.UrlRewrite.yaml`;
- `tests/fixtures/Stash.UrlRewrite.Malformed.yaml`;
- `tests/fixtures/Stash.UrlRewriteRedirect.yaml`;
- `tests/fixtures/Stash.UrlRewriteRedirect.Malformed.yaml`.

## Current Claim

Allowed statements:

- Stash is a migration target.
- Stash `http.mitm` has partial parser support for documented MITM host lists,
  including host-only normalization for `host:*` port-wildcard entries.
- Stash `http.force-http-engine` has parser support as an adapter-visible QUIC
  fallback decision signal.
- Stash `http.url-rewrite` has partial parser support for documented reject and
  302/307 redirect policy intent.
- Shadowrocket app-level profile syntax remains a migration target outside the
  common-config and rule-reject contracts.
- Shadowrocket common config has partial parser support for documented URL
  rewrite, script metadata, and MITM host/options syntax.
- Shadowrocket `[Rule]` `URL-REGEX`, `DOMAIN`, `DOMAIN-KEYWORD`, and
  `DOMAIN-SUFFIX` reject syntax has partial parser support as policy-core
  reject intent.
- Existing Loon, Quantumult X, Surge, Shadowrocket common-config, and portable
  AnixOps fixtures can inform how overlapping rules should be translated.
- Operators may manually map overlapping rules into a currently covered fixture
  profile for evaluation.

Forbidden statements:

- Full Stash parser support is implemented.
- Stash `rules`, `proxies`, DNS, routing, UI, cron, script runtime, or
  transport-level HTTP engine behavior is implemented.
- Stash transparent rewrite, route selection, or redirect status codes beyond
  302/307 are implemented.
- Full Shadowrocket parser support is implemented.
- Shadowrocket `[General]`, `[Proxy]`, DNS, routing, profile UI, proxy-node
  behavior, or `[Rule]` direct/proxy route selection is implemented.
- App-level routing, DNS, proxy-node, certificate, UI, or script runtime
  behavior is supported by the C policy core.

## CI Evidence

Current CI evidence:

- `config/stash_migration_guard_fixture_stays_parser_unsupported`;
- `config/stash_http_mitm_fixture_exposes_host_patterns`;
- `config/stash_http_mitm_malformed_fixture_rejects_invalid_host`;
- `config/stash_http_mitm_port_specific_fixture_stays_unsupported`;
- `config/stash_http_force_http_engine_fixture_exposes_quic_signal`;
- `config/stash_http_force_http_engine_malformed_fixture_rejects_invalid_bool`;
- `config/stash_url_rewrite_fixture_maps_reject_subset`;
- `config/stash_url_rewrite_malformed_fixture_rejects_invalid_regex`;
- `config/stash_url_rewrite_redirect_fixture_maps_redirect_subset`;
- `config/stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex`;
- `config/shadowrocket_migration_guard_fixture_stays_parser_unsupported`;
- `config/shadowrocket_rule_reject_fixture_maps_url_regex_rejects`;
- `config/shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex`;
- `config/shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects`;
- `config/shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain`;
- `config/shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects`;
- `config/shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword`;
- `config/shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects`;
- `config/shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain`;
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

Expected Stash HTTP MITM behavior:

- config load succeeds for `tests/fixtures/Stash.HttpMitm.yaml`;
- exact, wildcard, deny, and `host:*` entries register as MITM host patterns;
- `host:*` entries expose host-only matched patterns because port-specific MITM
  decisions are adapter-owned;
- `host:443` entries remain unsupported and do not register MITM host patterns;
- `force-http-engine: true` is parsed only as the existing QUIC fallback
  policy-core decision signal for matched trusted hosts;
- no rewrite rules, script rules, task descriptors, route policies, proxy
  nodes, DNS settings, or argument defaults are registered;
- malformed `http.mitm` host entries reject with parse diagnostics.

Expected Stash URL rewrite behavior:

- config load succeeds for `tests/fixtures/Stash.UrlRewrite.yaml` and
  `tests/fixtures/Stash.UrlRewriteRedirect.yaml`;
- `http.url-rewrite` entries shaped as `pattern - reject*` register
  request-phase rewrite reject decisions;
- `http.url-rewrite` entries shaped as `pattern replacement 302/307` register
  request-phase redirect decisions;
- unsupported transparent-shaped `url-rewrite` entries remain ignored;
- malformed supported reject or redirect entries fail with regex diagnostics;
- no script rules, task descriptors, MITM host patterns, route policies, proxy
  nodes, DNS settings, or argument defaults are registered from URL rewrite
  input.

Expected Shadowrocket app-profile guard behavior:

- config load succeeds in the portable profile;
- `[General]`, `[Proxy]`, and unsupported `[Rule]` route-selection lines remain
  ignored;
- no rewrite rules, script rules, MITM host patterns, or argument defaults are
  registered from unsupported guard lines;
- no direct/proxy routing, DNS, proxy-node, UI, or platform network behavior is
  claimed.

Expected Shadowrocket rule-reject behavior:

- config load succeeds for `tests/fixtures/Shadowrocket.RuleReject.conf`;
- config load succeeds for
  `tests/fixtures/Shadowrocket.RuleDomainExactReject.conf`;
- config load succeeds for
  `tests/fixtures/Shadowrocket.RuleDomainKeywordReject.conf`;
- config load succeeds for `tests/fixtures/Shadowrocket.RuleDomainReject.conf`;
- `[Rule]` `URL-REGEX`, `DOMAIN`, `DOMAIN-KEYWORD`, and `DOMAIN-SUFFIX` reject
  entries register request-phase rewrite reject decisions;
- unsupported `[Rule]` direct/proxy route-selection entries remain ignored;
- malformed supported `URL-REGEX` reject entries fail with regex diagnostics;
- malformed supported `DOMAIN` reject entries fail with parse diagnostics;
- malformed supported `DOMAIN-KEYWORD` reject entries fail with parse
  diagnostics;
- malformed supported `DOMAIN-SUFFIX` reject entries fail with parse
  diagnostics.

## Migration Mapping

Overlapping syntax should be mapped only into already covered policy-core
concepts:

- MITM host allow/deny lists;
- request URL redirect or reject;
- Stash `http.url-rewrite` reject and 302/307 redirect intent;
- Shadowrocket `[Rule]` `URL-REGEX`, `DOMAIN`, `DOMAIN-KEYWORD`, and
  `DOMAIN-SUFFIX` reject intent;
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

Before Stash app-profile support can move beyond the `http.mitm` and
`http.url-rewrite` contracts, or before Shadowrocket app-profile support can
move beyond the common-config and rule-reject contracts, a future change must
add:

1. A dedicated source contract for the target ecosystem surface.
2. At least one redistributable positive fixture for supported syntax.
3. At least one negative fixture for malformed or unsupported syntax.
4. C parser tests that load or reject those fixtures.
5. Compatibility matrix updates that name the fixture and test IDs.
6. GitHub Actions governance checks proving the contract, fixture, tests, and
   matrix row stay linked.

The current Stash app-profile guard and remaining Shadowrocket app-profile guard
fixtures do not satisfy those entry criteria for route selection, proxy nodes,
DNS, UI, transparent rewrite, redirect status codes beyond 302/307, or platform
networking. Keep those surfaces as migration notes only until dedicated parser
evidence exists.
