# Loon Rule Reject Source Contract

Capability: Loon `[Rule]` URL-regex, exact-domain, domain-keyword,
domain-suffix, and final reject policy intent.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes Loon `[Rule]` parser evidence for high-frequency URL-regex,
exact-domain, domain-keyword, domain-suffix, and final reject policy intent. It
maps only documented reject rules to the existing request-phase policy-core
rewrite reject decision.

It does not implement Loon routing, proxy selection, DNS, rule provider,
packet-level behavior, or profile UI behavior.

## Input Forms

The parser accepts this narrow form inside `[Rule]`:

```text
DOMAIN-SUFFIX,ads.example.test,REJECT-200
DOMAIN-SUFFIX,media.example.test,REJECT-IMG
DOMAIN,exact.example.test,REJECT
DOMAIN,media.example.test,REJECT-IMG
DOMAIN-KEYWORD,tracking,REJECT-200
DOMAIN-KEYWORD,media-keyword,REJECT-IMG
FINAL,REJECT-200
URL-REGEX,^https:\/\/ads.example.test\/banner,REJECT
URL-REGEX,^https:\/\/gone.example.test\/path,REJECT-410
```

Supported fields:

- `URL-REGEX` request URL regex rule type;
- `DOMAIN` exact-host rule type;
- `DOMAIN-KEYWORD` host substring rule type;
- `DOMAIN-SUFFIX` rule type;
- `FINAL` fallback rule type when paired with a reject action;
- a valid exact hostname, hostname suffix, or host keyword;
- `REJECT`, `REJECT-200`, `REJECT-NNN`, `REJECT-IMG`, `REJECT-VIDEO`,
  `REJECT-DICT`, and `REJECT-ARRAY` actions already supported by the common
  rewrite parser.

Unsupported or unclaimed Loon `[Rule]` forms remain route or adapter owned:

- `DIRECT`, `PROXY`, proxy group names, route policy names, and `no-resolve`;
- `IP-CIDR`, `GEOIP`, and DNS/network routing matchers;
- Loon rule-provider refresh, subscription, UI, or packet-routing behavior.

## Parser Output

For supported `URL-REGEX`, `DOMAIN`, `DOMAIN-KEYWORD`, `DOMAIN-SUFFIX`, and
`FINAL` reject rules, the parser must:

- register request-phase rewrite reject rules through the existing policy-core
  rewrite store;
- emit accepted diagnostics with section `Rule` and action `rule`;
- expose reject action and status-code fields through
  `anixops_rewrite_evaluate_url`;
- keep unsupported route-selection rules ignored without registering rewrite,
  script, MITM, task, argument, DNS, proxy-node, or route behavior.

Malformed supported `URL-REGEX` reject rules are rejected when the URL regex
cannot compile. Malformed supported `DOMAIN`, `DOMAIN-KEYWORD`, and
`DOMAIN-SUFFIX` reject rules are rejected when the hostname or keyword is
invalid. Malformed supported `FINAL` reject rules are rejected when the reject
action is missing.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.RuleDomainReject.plugin
tests/fixtures/Loon.RuleDomainExactReject.plugin
tests/fixtures/Loon.RuleDomainKeywordReject.plugin
tests/fixtures/Loon.RuleFinalReject.plugin
tests/fixtures/Loon.RuleUrlRegexReject.plugin
```

Expected behavior:

- config load succeeds;
- two `DOMAIN-SUFFIX` reject rules are registered;
- one `DOMAIN-SUFFIX` direct route remains ignored;
- exact suffix and subdomain request-phase URL evaluation returns the expected
  reject actions;
- two `DOMAIN` exact-host reject rules are registered in the exact-domain
  fixture;
- one `DOMAIN` direct route remains ignored;
- exact-host request-phase URL evaluation returns the expected reject actions;
- subdomains do not match exact-host rules;
- two `DOMAIN-KEYWORD` host-substring reject rules are registered in the
  keyword fixture;
- one `DOMAIN-KEYWORD` direct route remains ignored;
- matching hostnames return the expected reject actions;
- path-only keyword occurrences do not match host keyword rules;
- one `FINAL` reject rule is registered in the final fixture;
- one `FINAL` direct route remains ignored;
- HTTP and HTTPS request-phase URL evaluation returns the expected reject
  action through the fallback rule;
- two `URL-REGEX` reject rules are registered in the URL-regex fixture;
- one `DOMAIN-SUFFIX` proxy route remains ignored in the URL-regex fixture;
- matching request-phase URLs return the expected reject actions and status
  codes;
- non-matching request-phase URLs do not trigger the URL-regex rule;
- unrelated hostnames do not match;
- response-phase URL evaluation does not trigger the request-phase rule.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.RuleDomainReject.Malformed.plugin
tests/fixtures/Loon.RuleDomainExactReject.Malformed.plugin
tests/fixtures/Loon.RuleDomainKeywordReject.Malformed.plugin
tests/fixtures/Loon.RuleFinalReject.Malformed.plugin
tests/fixtures/Loon.RuleUrlRegexReject.Malformed.plugin
```

Expected behavior:

- config load fails with `ANIXOPS_ERR_PARSE`;
- no rewrite rules are registered;
- a rejected diagnostic is recorded with section `Rule` and action `rule`;
- last error reports the parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers parser and policy-core URL decision output only.

It does not implement:

- upstream direct/proxy route selection;
- proxy groups or proxy-node parsing;
- DNS strategy or `no-resolve`;
- rule provider download or refresh;
- socket creation, network IO, VPN, TUN, or packet capture;
- profile import UI;
- certificate lifecycle behavior;
- JavaScript runtime execution.

Adapters remain responsible for any future route-selection contract and
platform capability confirmation before claiming production route behavior.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_rule_domain_reject_fixture_maps_domain_suffix_rejects`;
- `tests/test_config.c` registers
  `config/loon_rule_domain_reject_malformed_fixture_rejects_invalid_domain`;
- `tests/test_config.c` registers
  `config/loon_rule_domain_exact_reject_fixture_maps_exact_domain_rejects`;
- `tests/test_config.c` registers
  `config/loon_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain`;
- `tests/test_config.c` registers
  `config/loon_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects`;
- `tests/test_config.c` registers
  `config/loon_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword`;
- `tests/test_config.c` registers
  `config/loon_rule_final_reject_fixture_maps_final_reject`;
- `tests/test_config.c` registers
  `config/loon_rule_final_reject_malformed_fixture_rejects_missing_action`;
- `tests/test_config.c` registers
  `config/loon_rule_url_regex_reject_fixture_maps_url_regex_rejects`;
- `tests/test_config.c` registers
  `config/loon_rule_url_regex_reject_malformed_fixture_rejects_invalid_regex`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon rule reject policy intent
```

The row remains `partial` because only `URL-REGEX`, `DOMAIN`,
`DOMAIN-KEYWORD`, `DOMAIN-SUFFIX`, and `FINAL` reject policy intent is covered.
Direct/proxy route selection, proxy groups, DNS, rule providers, app-level
profile UI, and platform networking behavior remain unimplemented.
