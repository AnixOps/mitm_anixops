# TODO And Non-Goals

This file tracks remaining work for `mitm_anixops` as an embeddable MITM/rewrite policy-chain library. It is not a
proxy client roadmap.

For the broader LOON MITM plugin compatibility, optional runtime, no-UI runner, and platform adapter roadmap, see
`docs/loon_mitm_full_compat_plan.md`. A shorter Chinese presentation draft is available at
`docs/loon_mitm_full_compat_brief_zh.md`.

## Before A Stable Release

No additional release-blocking library tasks are tracked here. Keep the release checklist and CI gates green before
publishing.

## Compatibility Gaps

- Compatibility profiles and per-rule diagnostics now have an initial C ABI foundation, but LOON/SURGE/QX strict-mode
  behavioral differences still need to be implemented rule-by-rule.
- Regex backend selection now has an initial C ABI foundation with a POSIX Lite default and optional PCRE2
  compile/match/replace support behind `PCRE2=1`; NSRegularExpression still needs a real Darwin backend.
- Full JQ-style JSON body rewrite hardening beyond the optional libjq backend behind `JQ=1`. Default builds still
  fail-open with `jq backend unavailable`; remaining work includes resource limits, production cache/reuse policy,
  broad plugin-corpus coverage, and edge behavior for predicates, slices, recursive selectors, and computed filters.
- Full NSRegularExpression/PCRE compatibility beyond POSIX ERE plus the tested leading `(?i)`/`(?m)`/`(?s)` prefixes,
  shorthand class subset, horizontal and vertical whitespace subset, control, hex, and Unicode escapes, lazy quantifier
  normalization, absolute anchors, named capture groups, and quoted literal matching subset.
- Full Quantumult X rewrite grammar beyond the tested `url`-prefixed request/response script, `echo-response`, common
  reject/body/header rewrite actions, and redirect actions.
- Full Surge rule grammar beyond tested `type=`, `pattern=`, `script-path=`, `requires-body=`, `tag=`, and `argument=`.
- Exact AnixOps/Loon behavior for every malformed line. Current behavior ignores incomplete non-action lines and reports
  regex compilation failures.
- Exact rewrite/script ordering for all edge cases in larger platform clients. The Alpha C ABI now has
  `anixops_rewrite_build_plan` for URL/body/header/script aggregation, but larger client corpus ordering still needs
  broader fixtures.
- Production `$persistentStore` beyond the Alpha Node runner's JSON file backend: platform namespace policy, locking,
  transaction semantics, quotas, and migration behavior.
- Production script runtime scheduling beyond the Alpha proxy shim's timeout fail-open path: cancellation, memory limits,
  script cache policy, and per-rule timeout/max-size attributes.
- Versioned Rust crate/Go module publication beyond the current Alpha wrapper sources and package metadata.

## Explicit Non-Goals For This Stage

- Production proxy forwarding implementation. The Alpha proxy shim is an integration/demo artifact, not the stable
  adapter boundary.
- Production TLS socket handling.
- Dynamic leaf certificate generation.
- System certificate installation or platform trust UI.
- HTTP/2 frame parsing.
- Full compression, chunked transfer decoding, or body streaming beyond the Alpha proxy shim's gzip/deflate
  body/script mutation path.
- Embedded QuickJS/JavaScriptCore runtime. The Alpha package includes a Node-based contract runner for tests/demos, but
  the C library still only returns script dispatch metadata.
- GUI client features.
- iOS/macOS Network Extension implementation.
- Platform-specific packet capture or TUN routing.

## Adapter Responsibilities

Adapters such as NetworkCore or another proxy client must own:

- Network IO and upstream proxy routing.
- TLS session handling and certificate generation/trust decisions.
- HTTP/1.1 and HTTP/2 message parsing and serialization.
- Body buffering, decoding, recompression, transfer framing, and size limits.
- Header map storage, case-insensitive lookup, and multi-value semantics.
- Script asset download/cache policy and JavaScript execution.
- Concurrency control around each `anixops_engine_t`, or an engine snapshot strategy.
