# Shadowrocket Rule Reject Source Contract

Capability: Shadowrocket `[Rule]` reject policy intent.

Ecosystem: `shadowrocket`.

Status: `partial`.

## Purpose

This contract defines the first app-profile parser-supported Shadowrocket
`[Rule]` subset for the v1.0.0 compatibility matrix. It covers URL regex,
exact-domain, domain-keyword, and domain-suffix rules whose policy maps to the
existing policy-core reject decision.

It does not implement Shadowrocket routing, proxy selection, DNS, or profile UI
behavior.

## Input Forms

The parser accepts this narrow form inside `[Rule]`:

```text
URL-REGEX,^https:\/\/rule\.shadowrocket\.test\/ads,REJECT
URL-REGEX,^https:\/\/rule\.shadowrocket\.test\/gone,REJECT-410
DOMAIN,exact.domain-rule.shadowrocket.test,REJECT
DOMAIN-KEYWORD,tracking,REJECT-200
DOMAIN-SUFFIX,domain-rule.shadowrocket.test,REJECT-200
```

Supported fields:

- `URL-REGEX` rule type;
- `DOMAIN` exact host rule type;
- `DOMAIN-KEYWORD` host substring rule type;
- `DOMAIN-SUFFIX` host suffix rule type;
- a policy-core URL regex pattern, exact hostname, hostname keyword, or
  hostname suffix;
- `REJECT`, `REJECT-200`, `REJECT-NNN`, `REJECT-IMG`, `REJECT-VIDEO`,
  `REJECT-DICT`, and `REJECT-ARRAY` actions already supported by the common
  rewrite parser.

Unsupported Shadowrocket `[Rule]` forms remain ignored in the portable profile:

- `IP-CIDR`, `GEOIP`, `FINAL`, and other routing matchers;
- `DIRECT`, `PROXY`, proxy group names, route policy names, and `no-resolve`;
- DNS, proxy-node, VPN, packet-capture, UI, or subscription behavior.

## Parser Output

For supported `URL-REGEX`, `DOMAIN`, `DOMAIN-KEYWORD`, and `DOMAIN-SUFFIX`
reject rules, the parser must:

- register request-phase rewrite reject rules through the existing policy-core
  rewrite store;
- emit accepted diagnostics with section `Rule` and action `rule`;
- expose reject action and status-code fields through
  `anixops_rewrite_evaluate_url`;
- keep unsupported route-selection rules ignored without registering rewrite,
  script, MITM, task, argument, DNS, proxy-node, or route behavior.

Malformed supported `URL-REGEX` reject rules are rejected when the URL regex
cannot compile. Malformed supported `DOMAIN`, `DOMAIN-KEYWORD`, or
`DOMAIN-SUFFIX` reject rules are rejected when the hostname or keyword is
invalid.

## Positive Case

Parser case:

```text
tests/fixtures/Shadowrocket.RuleReject.conf
tests/fixtures/Shadowrocket.RuleDomainExactReject.conf
tests/fixtures/Shadowrocket.RuleDomainKeywordReject.conf
tests/fixtures/Shadowrocket.RuleDomainReject.conf
```

Expected behavior:

- config load succeeds;
- two `URL-REGEX` reject rules are registered;
- two `DOMAIN` exact-host reject rules are registered from the dedicated exact
  domain fixture;
- two `DOMAIN-KEYWORD` reject rules are registered from the dedicated keyword
  fixture;
- two `DOMAIN-SUFFIX` reject rules are registered from the dedicated domain
  fixture;
- one `DOMAIN-SUFFIX` proxy route remains ignored;
- request-phase URL evaluation returns the expected reject actions;
- response-phase URL evaluation does not trigger the request-phase rule.

## Negative Case

Parser case:

```text
tests/fixtures/Shadowrocket.RuleReject.Malformed.conf
tests/fixtures/Shadowrocket.RuleDomainExactReject.Malformed.conf
tests/fixtures/Shadowrocket.RuleDomainKeywordReject.Malformed.conf
tests/fixtures/Shadowrocket.RuleDomainReject.Malformed.conf
```

Expected behavior:

- config load fails with `ANIXOPS_ERR_REGEX` for malformed URL regex input or
  `ANIXOPS_ERR_PARSE` for malformed hostname input;
- no rewrite rules are registered;
- a rejected diagnostic is recorded with section `Rule` and action `rule`;
- last error reports the parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers parser and policy-core URL decision output only.

It does not implement:

- upstream direct/proxy route selection;
- proxy groups or proxy-node parsing;
- DNS strategy or `no-resolve`;
- socket creation, network IO, VPN, TUN, or packet capture;
- profile import UI;
- certificate lifecycle behavior;
- JavaScript runtime execution.

Adapters remain responsible for any future route-selection contract and platform
capability confirmation before claiming production route behavior.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/shadowrocket_rule_reject_fixture_maps_url_regex_rejects`;
- `tests/test_config.c` registers
  `config/shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects`;
- `tests/test_config.c` registers
  `config/shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain`;
- `tests/test_config.c` registers
  `config/shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects`;
- `tests/test_config.c` registers
  `config/shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain`;
- `tests/test_config.c` registers
  `config/shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects`;
- `tests/test_config.c` registers
  `config/shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword`;
- `tests/test_config.c` keeps
  `config/shadowrocket_migration_guard_fixture_stays_parser_unsupported`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Shadowrocket rule reject policy intent
```

The row remains `partial` because only URL-regex, exact-domain, domain-keyword,
and domain-suffix reject policy intent is covered. Direct/proxy route selection,
proxy groups, DNS, app-level profile UI, and platform networking behavior remain
unimplemented.
