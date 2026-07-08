# Compatibility Matrix

This is the Alpha implementation matrix. The v1.0.0 compatibility source of truth starts in
`docs/compatibility/README.md` and `docs/compatibility/matrix.md`; keep both views aligned while the v1.0.0 track is
being built.

`mitm_anixops` is a policy-chain library. It parses supported rule shapes and returns structured C ABI decisions.
Network transport, TLS, body framing, decompression, and JavaScript execution stay in the embedding adapter.

This matrix documents the currently tested compatibility surface. Anything not listed here should be treated as
unsupported unless a test or E2E fixture is added.

## Config Sections

| Style | Section Shape | Status | Evidence |
| --- | --- | --- | --- |
| AnixOps/Loon plugin | `[Argument]`, `[Script]`, `[MITM]`, `[Rewrite]` | Supported subset | `test_config.c`, `test_script.c`, BiliBili and representative Loon fixtures |
| Loon plugin metadata | `[Plugin]` | Tolerated metadata section | `Loon.PluginMetadata.plugin`, `Loon.PluginMetadata.Unsupported.plugin`, `config/loon_plugin_metadata_fixture_records_ignored_lines`, `config/loon_plugin_metadata_unsupported_keys_are_not_claimed` |
| Hashbang metadata | `#!name`, `#!desc`, `#!arguments-desc`, `#!requirement` | Tolerated with ignored diagnostics | representative corpus fixtures |
| Loon argument section | `[Argument]` `Name = kind,default` | Supported parser/default subset | `Loon.ArgumentSection.plugin`, `Loon.ArgumentSection.Malformed.plugin`, `config/loon_argument_section_fixture_resolves_script_defaults`, `config/loon_argument_section_malformed_fixture_rejects_missing_equals` |
| Loon inline arguments | `#!arguments = Name:value` | Supported subset | `Loon.InlineArguments.plugin`, `Loon.InlineArguments.Malformed.plugin`, `config/loon_inline_arguments_fixture_resolves_script_defaults`, `config/loon_inline_arguments_malformed_fixture_rejects_missing_separator` |
| Loon script metadata | `[Script]` `http-request`/`http-response` fields | Supported parser/dispatch-metadata subset | `Loon.ScriptMetadata.plugin`, `Loon.ScriptMetadata.Malformed.plugin`, `config/loon_script_metadata_fixture_exposes_dispatch_fields`, `config/loon_script_metadata_malformed_fixture_rejects_missing_path` |
| Loon task metadata | `[Script]` `cron`/`type=interval` fields | Supported parser/task-descriptor subset | `Loon.TaskMetadata.plugin`, `Loon.TaskMetadata.Malformed.plugin`, `config/loon_task_metadata_fixture_emits_task_descriptors`, `config/loon_task_metadata_malformed_fixture_rejects_invalid_cron` |
| Loon MITM options | `[MITM]` `enable`, `hostname`, `skip-server-cert-verify`, `h2`, `disable-quic` | Supported parser/adapter-signal subset | `Loon.MitmOptions.plugin`, `Loon.MitmOptions.Malformed.plugin`, `config/loon_mitm_options_fixture_exposes_adapter_flags`, `config/loon_mitm_options_malformed_fixture_rejects_invalid_host`; certificate lifecycle and TLS behavior remain adapter-owned |
| Loon rule reject | `[Rule]` `DOMAIN-SUFFIX,<host>,REJECT` | Supported parser/reject-intent subset | `Loon.RuleDomainReject.plugin`, `Loon.RuleDomainReject.Malformed.plugin`, `config/loon_rule_domain_reject_fixture_maps_domain_suffix_rejects`, `config/loon_rule_domain_reject_malformed_fixture_rejects_invalid_domain`; direct/proxy route selection remains adapter-owned |
| Loon rewrite aliases | `[URL Rewrite]`, `[Remote Rewrite]` | Supported subset | `config_accepts_section_aliases_and_crlf` |
| Loon body rewrite aliases | `[Body Rewrite]`, `[Remote Body Rewrite]` | Supported subset | `config_accepts_body_rewrite_section_aliases` |
| Loon header rewrite aliases | `[Header Rewrite]`, `[Remote Header Rewrite]` | Supported subset | `config_accepts_header_rewrite_section_aliases` |
| Quantumult X / snippet | `#[rewrite_local]`, `#[rewrite_remote]`, `#[mitm]` | Supported subset | `anixops_snippet_rewrite_script_lines_are_supported`, representative Quantumult X fixture |
| Quantumult X MITM options | `force-http-engine-hosts`, `skip-server-cert-verify`, `h2`, `disable-quic` | Supported parser/adapter-signal subset | `QuantumultX.MitmOptions.snippet`, `QuantumultX.MitmOptions.Malformed.snippet`, `config/quantumultx_mitm_options_fixture_exposes_adapter_flags`, `config/quantumultx_mitm_options_malformed_fixture_rejects_invalid_host` |
| Quantumult X task metadata | `#[task_local]` cron lines | Supported parser/task-descriptor subset | `QuantumultX.TaskMetadata.snippet`, `QuantumultX.TaskMetadata.Malformed.snippet`, `config/quantumultx_task_metadata_fixture_emits_task_descriptors`, `config/quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron` |
| Quantumult X | `[rewrite_local]`, `[rewrite_remote]`, `[mitm]` | Supported subset | `quantumultx_rewrite_local_section_is_supported`, `config_accepts_quantumultx_rewrite_remote_section_aliases` |
| Surge module | `[Script]` plus `name = type=http-response, pattern=...` | Supported subset | `surge_style_script_rule_template_is_supported`, representative Surge fixture |
| Surge module arguments | `#!arguments = Name:value` | Supported subset | `sgmodule_inline_arguments_are_supported` |
| Surge requirement metadata | `#!requirement` and adjacent module metadata | Tolerated metadata diagnostics | `Surge.RequirementMetadata.sgmodule`, `Surge.RequirementMetadata.Unsupported.sgmodule`, `config/surge_requirement_metadata_fixture_records_tolerated_keys`, `config/surge_requirement_metadata_unsupported_keys_are_not_claimed` |
| Surge task metadata | `[Script]` `type=cron`/`type=interval` fields | Supported parser/task-descriptor subset | `Surge.TaskMetadata.sgmodule`, `Surge.TaskMetadata.Malformed.sgmodule`, `config/surge_task_metadata_fixture_emits_task_descriptors`, `config/surge_task_metadata_malformed_fixture_rejects_invalid_cron` |
| Stash HTTP MITM hosts/options | YAML `http.mitm` host list and `http.force-http-engine` | Supported parser/MITM-host and adapter-signal subset | `Stash.HttpMitm.yaml`, `Stash.HttpMitm.Malformed.yaml`, `Stash.HttpMitm.PortSpecificUnsupported.yaml`, `Stash.HttpForceHttpEngine.yaml`, `Stash.HttpForceHttpEngine.Malformed.yaml`, `config/stash_http_mitm_fixture_exposes_host_patterns`, `config/stash_http_mitm_malformed_fixture_rejects_invalid_host`, `config/stash_http_mitm_port_specific_fixture_stays_unsupported`, `config/stash_http_force_http_engine_fixture_exposes_quic_signal`, `config/stash_http_force_http_engine_malformed_fixture_rejects_invalid_bool`; `host:*` entries normalize to host-only policy patterns; `force-http-engine` maps only to the existing QUIC fallback decision signal; Stash app-level routing/proxy syntax remains migration-guarded |
| Stash URL rewrite request policy | YAML `http.url-rewrite` `pattern - reject` and `pattern replacement 302/307` | Supported parser/reject and redirect-intent subset | `Stash.UrlRewrite.yaml`, `Stash.UrlRewrite.Malformed.yaml`, `Stash.UrlRewriteRedirect.yaml`, `Stash.UrlRewriteRedirect.Malformed.yaml`, `config/stash_url_rewrite_fixture_maps_reject_subset`, `config/stash_url_rewrite_malformed_fixture_rejects_invalid_regex`, `config/stash_url_rewrite_redirect_fixture_maps_redirect_subset`, `config/stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex`; transparent and direct/proxy route selection remain migration-guarded |
| Shadowrocket common config | `[URL Rewrite]`, `[Script]`, `[MITM]` | Supported subset | `Shadowrocket.CommonConfig.conf`, `Shadowrocket.CommonConfig.Malformed.conf`, `config/shadowrocket_common_config_fixture_is_supported`, `config/shadowrocket_common_config_fixture_rejects_invalid_regex` |
| Shadowrocket rule reject | `[Rule]` `URL-REGEX,<pattern>,REJECT`, `DOMAIN,<host>,REJECT`, `DOMAIN-KEYWORD,<keyword>,REJECT`, `DOMAIN-SUFFIX,<host>,REJECT`, `FINAL,REJECT` | Supported parser/reject-intent subset | `Shadowrocket.RuleReject.conf`, `Shadowrocket.RuleReject.Malformed.conf`, `Shadowrocket.RuleDomainExactReject.conf`, `Shadowrocket.RuleDomainExactReject.Malformed.conf`, `Shadowrocket.RuleDomainKeywordReject.conf`, `Shadowrocket.RuleDomainKeywordReject.Malformed.conf`, `Shadowrocket.RuleDomainReject.conf`, `Shadowrocket.RuleDomainReject.Malformed.conf`, `Shadowrocket.RuleFinalReject.conf`, `Shadowrocket.RuleFinalReject.Malformed.conf`, `config/shadowrocket_rule_reject_fixture_maps_url_regex_rejects`, `config/shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects`, `config/shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects`, `config/shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects`, `config/shadowrocket_rule_final_reject_fixture_maps_final_reject`, `config/shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex`, `config/shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain`, `config/shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword`, `config/shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain`, `config/shadowrocket_rule_final_reject_malformed_fixture_rejects_missing_action`; direct/proxy route selection remains migration-guarded |

Unknown sections are ignored so clients can load larger platform configs and let this library consume only the policy
features it understands.

## MITM

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| `hostname = host1, host2` | Supported | `anixops_engine_add_mitm_hostname`, `anixops_mitm_evaluate` |
| Quantumult X `force-http-engine-hosts = host1, host2` | Supported as MITM host-list alias | representative Quantumult X corpus fixture and `config_accepts_quantumultx_mitm_host_options` |
| `%APPEND%` / `%INSERT%` prefixes | Supported | `module_patch_markers_are_ignored_in_mitm_hostname` |
| Allow host patterns | Supported | exact, wildcard, `*.` suffix tests |
| Deny host patterns | Supported | `-host` and `!host` tests |
| Certificate trust gate | Supported as adapter input | `anixops_engine_set_cert_state` |
| Certificate lifecycle | Adapter-owned partial contract | `docs/architecture/certificate-lifecycle.md` records no automatic root trust, no non-target hostname decryption, and pending manual-intervention markers for CA stores, signing materials, protected environments, and trust UX |
| Server certificate verification bypass flag | Supported as adapter-readable config | `skip-server-cert-verify` boolean and host-list forms, `anixops_engine_skip_server_cert_verify`, `config_exposes_skip_server_cert_verify`, representative Quantumult X corpus fixture |
| QUIC rejection decision and config flag | Supported as adapter signal | `ANIXOPS_MITM_REJECT_QUIC`, `disable-quic`, `disable_quic`, `disable-mitm-quic`, and `disable_mitm_quic` tests |
| Stash `http.force-http-engine` | Supported as adapter signal | `Stash.HttpForceHttpEngine.yaml` and `config/stash_http_force_http_engine_fixture_exposes_quic_signal`; transport-level HTTP engine behavior remains adapter-owned |
| HTTP/2 MITM config flag | Supported | `h2`, `h2-enable`, and `h2_enable` tests |
| Dynamic certificates / trust store install | Out of scope | adapter responsibility |

## Rewrite

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| URL redirect `301`, `302`, `303`, `307`, `308` | Supported | `anixops_rewrite_evaluate_url` |
| Reject variants | Supported | `reject`, `reject-200`, numeric `reject-NNN`, `reject-img`, `reject-tinygif`, `reject-video`, `reject-dict`, `reject-array` |
| Direct/proxy route intent | Unsupported route selection; tolerated as ignored parser lines in portable profile | `docs/compatibility/policy-intent-common.md`, `PolicyIntent.Unsupported.conf`, and `config/policy_intent_unsupported_routes_are_ignored` |
| Quantumult X `url`-prefixed redirect/reject/body/header/echo-response rewrites | Supported subset | `quantumultx_url_prefixed_rewrites_are_supported`, `quantumultx_echo_response_is_supported` |
| Unsupported recognized ecosystem rewrite actions | Ignored for 0.x compatibility | `unsupported_recognized_rewrite_actions_are_ignored` |
| Capture expansion | Supported subset | `$1`, `${1}`, `\1`, `${name}`, and `$<name>` tests |
| Leading `(?i)`, `(?m)`, `(?s)`, and combined regex prefixes | Supported subset | URL, body, header, and script regex tests |
| PCRE shorthand classes `\d`, `\D`, `\w`, `\W`, `\s`, `\S` | Supported subset | URL, body, header, and script regex tests |
| PCRE horizontal whitespace classes `\h`, `\H` | Supported subset | URL, body, header, and script regex test |
| PCRE vertical whitespace classes `\v`, `\V` | Supported ASCII subset | URL, body, header, and script regex test |
| PCRE control escapes `\t`, `\n`, `\r`, `\f`, `\a`, `\e` | Supported subset | URL, body, header, and script regex test |
| PCRE hex byte escapes `\xHH` | Supported subset | URL, body, header, and script regex test |
| PCRE Unicode escapes `\uHHHH` | Supported subset | URL, body, header, and script regex test |
| PCRE lazy quantifier suffixes `*?`, `+?`, `??`, `{m,n}?` | Supported as greedy equivalents | URL, body, header, and script regex test |
| PCRE absolute anchors `\A`, `\z`, `\Z` | Supported subset | URL, body, header, and script regex test |
| PCRE non-capturing groups `(?:...)` | Supported without replacement capture numbering side effects | URL, body, header, and script regex test |
| PCRE named capture groups `(?<name>...)`, `(?'name'...)` | Supported for matching and replacement | URL, body, header, and script regex test; named replacement golden test |
| PCRE quoted literals `\Q...\E` | Supported subset | URL, body, header, and script regex test |
| Empty regex replacement matches | Supported subset | Body/header global replacement tests cover `^`, `$`, and lazy `.*?` normalization without repeated anchor replacement |
| Request body mock | Supported for buffered plain text | `anixops_rewrite_apply_body` and chain-capable `anixops_rewrite_apply_body_chain` |
| Response body mock | Supported for buffered plain text | `anixops_rewrite_apply_body` and chain-capable `anixops_rewrite_apply_body_chain` |
| Request/response body regex replace | Supported for buffered plain text | POSIX ERE, capture replacement, global replacement, empty-match tests, and chain API tests proving previous output feeds the next matching body rule |
| Request/response body JSON path replace | Supported subset for buffered JSON | `$.field`, `$['field']`, empty, common escaped, and `\uXXXX` bracket keys, nested object path, and positive/negative array index tests |
| Request/response body JQ rewrite actions | Supported with optional backend | `request-body-jq`, `http-request-jq`, `response-body-jq`, and `http-response-jq` parse and match. Default builds fail open with `jq backend unavailable`; `JQ=1` builds execute through libjq with configurable max-input bytes |
| Alpha proxy body rewrite path | Supported subset | `make script-contract-e2e` verifies buffered request/response body rewrite before script dispatch through the HTTP/1.1 MITM shim, which now calls the chain body rewrite API |
| Request header add/replace/delete/regex replace | Supported as structured operation | `anixops_rewrite_evaluate_header` |
| Response header add/replace/delete/regex replace | Supported as structured operation | `anixops_rewrite_evaluate_header` |
| Case-insensitive named header lookup | Supported foundation | `anixops_rewrite_evaluate_named_header` filters rewrite rules by header name using case-insensitive matching; Go/Rust wrappers expose the same helper |
| Multi-value header-list application | Supported Alpha bounded list | `anixops_rewrite_apply_headers` applies add/replace/delete and regex replace across a bounded header list with case-insensitive names; tests cover request multi-value semantics and response `Set-Cookie` fields kept as independent header entries |
| Alpha proxy header rewrite path | Supported subset | `make script-contract-e2e` verifies request/response header rewrite before script dispatch through the HTTP/1.1 MITM shim |
| Full JQ-style JSON rewrites | Supported subset with gaps | libjq first-output-wins, empty-output, invalid JSON, compile-error, input-limit fail-open, and output-buffer fail-open policies are tested; timeout/memory limits and broad corpus coverage remain gaps |
| Compression/chunk handling | Out of scope | adapter responsibility |

## Script Dispatch

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| `http-request` / `http-response` plugin lines | Supported subset | `anixops_script_evaluate_url` |
| `requires-body` / `requires_body` | Supported | response/request script tests |
| `script-path` / `script_path` | Supported | plugin and Surge tests |
| `tag` | Supported | plugin and Surge tests |
| `argument=[{Name},...]` | Supported | BiliBili fixture tests |
| `enable`, `enabled` | Supported dispatch gating | `disabled_script_rules_do_not_dispatch` |
| `timeout`, `timeout-ms`, `max-size`, `max_size` | Supported as scheduling metadata | `script_scheduling_attributes_are_exposed`, representative Surge fixture; runner/proxy shim use timeout override and max-size fail-open |
| Surge template arguments `{Name}`, `{{{Name}}}` | Supported subset | `surge_style_script_rule_template_is_supported` |
| Malformed Surge attr-list script rules | Ignored, except invalid regex reports an error | `malformed_and_non_http_script_rules_are_ignored_or_rejected` |
| Quantumult X `url script-request-body` / `script-request-header` / `script-response-body` / `script-response-header` | Supported subset | snippet and `[rewrite_local]` tests |
| Cron/task descriptor parser | Supported metadata subset | `CronTaskTrigger.HttpScriptGuard.conf`, `CronTaskTrigger.Malformed.conf`, `Loon.TaskMetadata.plugin`, `Loon.TaskMetadata.Malformed.plugin`, `QuantumultX.TaskMetadata.snippet`, `QuantumultX.TaskMetadata.Malformed.snippet`, `Surge.TaskMetadata.sgmodule`, `Surge.TaskMetadata.Malformed.sgmodule`, `anixops_engine_task_descriptor_count`, and `anixops_engine_copy_task_descriptor`; scheduler/runtime remains adapter-owned |
| JavaScript runtime | Adapter contract supported | C library returns dispatch metadata; Alpha runner can execute mapped scripts or offline script bundles through the Node contract runner during `replay --script-runner`; `docs/architecture/script-runtime-dependency.md` records no embedded JS engine for the v1.0.0 policy core |
| `$persistentStore` | Alpha runner backend | Node contract runner supports `--store <file>` with read/write/remove; runner replay and proxy script-contract E2E verify state shared across invocations |
| Script asset bundle/digest | Alpha runner backend | `replay --script-bundle` resolves local offline assets, verifies sha256 pins, and reports digest mismatch/cache miss without network IO |
| Script timeout/error policy | Alpha runner/proxy shim subset | Rule-level `timeout` metadata overrides the global runner timeout; max-size overflow fails open; `make runner-check` covers a throwing replay script, and `make script-contract-e2e` verifies timed-out and throwing response scripts fail open after static rewrites instead of returning 502 |
| Double `$done` | Alpha runner backend | `make runner-check` covers no-network replay where the first `$done` body wins and a later `$done` call is ignored |
| Cron/task trigger runtime | Planned | Source contract is `docs/compatibility/cron-task-trigger.md`; parser descriptors are exposed through the C ABI, while scheduler, task runtime bindings, permissions, and concurrency policy remain future work |
| Response compression for scripts | Alpha proxy shim subset | gzip/deflate response bodies are decoded before the script runner and returned as identity after mutation; brotli/zstd/streaming remain future work |

## Diagnostics And ABI

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| Stable opaque engine handle | Supported | `anixops_engine_t` opaque typedef |
| Caller-owned result structs | Supported | no public internal pointers |
| Status text | Supported | `anixops_status_message` |
| Last error status/line/message copy | Supported | `anixops_engine_copy_last_error` |
| Compatibility profile selector | Supported foundation | `anixops_engine_set_compat_profile`, `anixops_engine_compat_profile`; portable mode ignores unsupported rule-shaped lines in supported sections, while strict profiles reject them with parse diagnostics. Rule-by-rule platform parity remains incremental |
| Per-rule parse diagnostics | Supported foundation | `anixops_engine_rule_diagnostic_count`, `anixops_engine_copy_rule_diagnostic`, accepted/ignored/rejected config tests, strict-profile rejected-rule tests |
| Corpus manifest scan | Supported foundation | `tests/fixtures/corpus/manifest.json`, `anixops-mitm-runner scan --corpus`, runner-check verifies representative Loon/Surge/Quantumult X and BiliBili fixture counts, sha256 digests, and accepted/ignored/rejected diagnostic counts |
| Request/response plan builder | Supported foundation | `anixops_rewrite_build_plan` aggregates phase rewrite, matching header rewrites, script dispatch, body-rewrite output, and `requires_body`; unit test compares it against individual evaluation APIs |
| Regex backend selector | Supported foundation | `anixops_regex_backend_available`, `anixops_engine_set_regex_backend`, POSIX Lite default, `requires pcre2 backend` diagnostics for PCRE2-only constructs in URL/body/header/script regexes |
| Optional PCRE2 backend | Supported optional backend | `make pcre2-test`, `PCRE2=1`, lookahead/lookbehind, backreference, Unicode property, word-boundary, atomic group, possessive quantifier, and named-backreference fixtures |
| Optional libjq backend | Supported optional backend | `make jq-test`, `JQ=1`, body rewrite, max-input and output-buffer fail-open, and general fail-open policy fixtures |
| ABI export allowlist | Supported | `ci/abi_exports.txt` checked by `scripts/check.sh` |
| pkg-config metadata | Supported Alpha packaging | `make pkg-config-check`, relocatable `lib/pkgconfig/mitm_anixops.pc` in `alpha-dist` |
| CMake package config | Supported Alpha packaging | `make cmake-package-check`, relocatable `lib/cmake/mitm_anixops`; configure/build smoke runs when `cmake` is installed |
| Go cgo binding | Supported Alpha wrapper | `make go-binding-check`, package `bindings/go/anixops`, pkg-config backed cgo link, includes plan, body-chain, named-header, and header-list helper coverage |
| Rust FFI binding | Supported Alpha wrapper | `make rust-binding-check`, crate `bindings/rust/mitm-anixops`, pkg-config backed build script, includes plan, body-chain, named-header, and header-list helper coverage |
| Thread-safe mutation | Not provided | external synchronization required |

## Current End-To-End Evidence

- `make demo-check`: pure C strategy-chain demo, no sockets/TLS/JS.
- `make runner-check`: no-UI runner scan/trace/replay smoke test over the representative Loon fixture, corpus manifest
  scan over Loon/Surge/Quantumult X/BiliBili fixtures with sha256 and diagnostic-status checks, optional Node
  script-runtime replay, `$done.body` writeback, throwing-script fail-open, and file-backed `$persistentStore`.
- `make proxy-shim-check`: Alpha HTTP/1.1 CONNECT/TLS proxy shim build smoke test.
- `make script-contract-e2e`: request/response header/body-chain rewrite and script mutation through the proxy path,
  including shared `$persistentStore`, script timeout/exception fail-open, gzip/deflate response decode, and identity
  writeback.
- `make pkg-config-check`: staged Alpha-style install layout compiled and executed through `pkg-config`.
- `make cmake-package-check`: staged Alpha-style install layout checked as a CMake config package, including
  configure/build/run smoke when `cmake` is available.
- `make go-binding-check`: Go cgo wrapper tests over config load, rewrite, body rewrite, body-chain rewrite, script
  dispatch, and plan helper.
- `make rust-binding-check`: Rust FFI wrapper tests over config load, rewrite, body rewrite, body-chain rewrite, script
  dispatch, and plan helper.
- `make test`: public C ABI unit tests, including the plan builder, representative Loon, Surge, Quantumult X,
  Shadowrocket common-config, and cron/task descriptor fixture parsing.
- `make e2e`: local shim plus mihomo, proving library decisions through a proxy path.
- `make bili-e2e`: BiliUniverse Enhanced plugin/script dispatch and script execution fixture.
- `make script-contract-e2e`: request/response script metadata, persistentStore, timeout/exception fail-open, and
  adapter writeback contract. `make runner-check` also covers double `$done` first-wins replay behavior.
- `make bilibili-homepage-demo-e2e`: built-in Windows Bilibili homepage demo behavior through the shim path.
