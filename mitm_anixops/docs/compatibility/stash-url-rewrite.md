# Stash URL Rewrite Source Contract

Capability: Stash `http.url-rewrite` request URL reject and redirect policy
intent.

Ecosystem: `stash`.

Status: `partial`.

## Purpose

This contract adds a narrow Stash app-profile parser subset that maps directly
onto the existing request-phase rewrite/reject policy core. It covers
request-phase reject decisions and the conservative 302/307 redirect subset. It
is intentionally smaller than Stash's full HTTP rewrite grammar and does not
introduce a general YAML parser.

The source form follows Stash HTTP rewrite configuration examples where
`url-rewrite` entries live below a top-level `http:` mapping:

```text
https://stash.wiki/en/http-engine/rewrite
```

## Input Forms

The parser accepts this narrow YAML shape:

```yaml
http:
  url-rewrite:
    - ^https:\/\/example\.test\/ads - reject
    - ^https:\/\/example\.test\/gone - reject-410
    - ^http:\/\/example\.test\/(.*) https://example.test/$1 302
```

Supported forms:

- top-level `http:`;
- nested `url-rewrite:`;
- list scalar entries below `url-rewrite`;
- `pattern - reject`;
- `pattern - reject-200`;
- `pattern - reject-NNN` for 100-599 status codes;
- existing reject body variants already understood by the policy core, such as
  `reject-img`, `reject-video`, `reject-dict`, and `reject-array`;
- `pattern replacement 302`;
- `pattern replacement 307`.

Unsupported forms remain ignored or outside this contract:

- transparent URL rewrites such as `pattern replacement transparent`;
- redirect status codes outside the contracted 302/307 subset;
- route-selection, direct, proxy, proxy-group, DNS, rule-provider, and TUN
  behavior;
- Stash scripts, cron, shortcuts, UI fields, and proxy node definitions;
- anchors, aliases, folded scalars, inline arrays, maps, or arbitrary YAML
  expressions.

## Parser Output

For valid supported `http.url-rewrite` reject and redirect entries, the parser
must:

- register request-phase rewrite reject or redirect rules through the existing
  policy-core rewrite store;
- emit accepted diagnostics with section `Rewrite` and action `url-rewrite`;
- preserve existing reject status-code and redirect replacement behavior from
  the portable rewrite parser;
- ignore unsupported transparent, route-like, or out-of-subset entries without
  registering rewrite, script, task, MITM, proxy, DNS, or certificate behavior.

Malformed supported reject or redirect entries are rejected when the parser
recognizes the contracted shape but the regex is invalid.

## Positive Case

Parser case:

```text
tests/fixtures/Stash.UrlRewrite.yaml
tests/fixtures/Stash.UrlRewriteRedirect.yaml
```

Expected behavior:

- config load succeeds;
- two request-phase rewrite reject rules are registered from
  `Stash.UrlRewrite.yaml`;
- two request-phase redirect rules are registered from
  `Stash.UrlRewriteRedirect.yaml`;
- transparent `url-rewrite` entries remain ignored;
- no script rules, task descriptors, MITM host patterns, arguments, route
  policies, proxy nodes, DNS settings, or certificate lifecycle behavior are
  registered.

## Negative Case

Parser case:

```text
tests/fixtures/Stash.UrlRewrite.Malformed.yaml
tests/fixtures/Stash.UrlRewriteRedirect.Malformed.yaml
```

Expected behavior:

- config load fails with `ANIXOPS_ERR_REGEX`;
- no rewrite rules are registered;
- a rejected diagnostic is recorded with section `Rewrite` and action
  `url-rewrite`;
- last error reports invalid rewrite URL regex context at the malformed line.

## Runtime And Security Boundary

Stash `http.url-rewrite` parser support must not:

- implement route selection, direct/proxy behavior, proxy groups, DNS, TUN, VPN,
  packet-capture, or platform network-extension behavior;
- install, trust, generate, rotate, revoke, or persist certificates;
- decrypt traffic for non-target hostnames;
- parse or execute Stash scripts;
- log sensitive headers or body content.

The C policy core only exposes a bounded reject decision. Adapter-visible
routing, transport, UI, and certificate behavior remain outside this contract.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/stash_url_rewrite_fixture_maps_reject_subset`;
- `tests/test_config.c` registers
  `config/stash_url_rewrite_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` registers
  `config/stash_url_rewrite_redirect_fixture_maps_redirect_subset`;
- `tests/test_config.c` registers
  `config/stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex`;
- `tests/test_config.c` keeps
  `config/stash_migration_guard_fixture_stays_parser_unsupported`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Stash URL rewrite request policy intent
```

The row remains `partial` because only the `http.url-rewrite` reject and
302/307 redirect subsets are covered. Full Stash YAML profiles, transparent
rewrites, scripts, cron, routing, proxy nodes, DNS, UI, transport behavior, and
certificate lifecycle behavior remain unimplemented.
