# Compatibility Matrix

`mitm_anixops` is a policy-chain library. It parses supported rule shapes and returns structured C ABI decisions.
Network transport, TLS, body framing, decompression, and JavaScript execution stay in the embedding adapter.

This matrix documents the currently tested compatibility surface. Anything not listed here should be treated as
unsupported unless a test or E2E fixture is added.

## Config Sections

| Style | Section Shape | Status | Evidence |
| --- | --- | --- | --- |
| AnixOps/Loon plugin | `[Argument]`, `[Script]`, `[MITM]`, `[Rewrite]` | Supported subset | `test_config.c`, `test_script.c`, BiliBili and representative Loon fixtures |
| Loon rewrite aliases | `[URL Rewrite]`, `[Remote Rewrite]` | Supported subset | `config_accepts_section_aliases_and_crlf` |
| Loon header rewrite aliases | `[Header Rewrite]`, `[Remote Header Rewrite]` | Supported subset | `config_accepts_header_rewrite_section_aliases` |
| Quantumult X / snippet | `#[rewrite_local]`, `#[mitm]` | Supported subset | `anixops_snippet_rewrite_script_lines_are_supported`, representative Quantumult X fixture |
| Quantumult X | `[rewrite_local]`, `[mitm]` | Supported subset | `quantumultx_rewrite_local_section_is_supported` |
| Surge module | `[Script]` plus `name = type=http-response, pattern=...` | Supported subset | `surge_style_script_rule_template_is_supported`, representative Surge fixture |
| Surge module arguments | `#!arguments = Name:value` | Supported subset | `sgmodule_inline_arguments_are_supported` |

Unknown sections are ignored so clients can load larger platform configs and let this library consume only the policy
features it understands.

## MITM

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| `hostname = host1, host2` | Supported | `anixops_engine_add_mitm_hostname`, `anixops_mitm_evaluate` |
| `%APPEND%` prefix | Supported | `append_marker_is_ignored_in_mitm_hostname` |
| Allow host patterns | Supported | exact, wildcard, `*.` suffix tests |
| Deny host patterns | Supported | `-host` and `!host` tests |
| Certificate trust gate | Supported as adapter input | `anixops_engine_set_cert_state` |
| QUIC rejection decision | Supported as adapter signal | `ANIXOPS_MITM_REJECT_QUIC` |
| Dynamic certificates / trust store install | Out of scope | adapter responsibility |

## Rewrite

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| URL redirect `302`, `307` | Supported | `anixops_rewrite_evaluate_url` |
| Reject variants | Supported | `reject`, `reject-200`, numeric `reject-NNN`, `reject-img`, `reject-video`, `reject-dict`, `reject-array` |
| Quantumult X `url`-prefixed redirect/reject/body/header rewrites | Supported subset | `quantumultx_url_prefixed_rewrites_are_supported` |
| Unsupported Quantumult X `url` actions | Ignored | `unsupported_quantumultx_url_actions_are_ignored` |
| Unsupported recognized ecosystem rewrite actions | Ignored for 0.x compatibility | `unsupported_recognized_rewrite_actions_are_ignored` |
| Capture expansion | Supported subset | `$1` and `\1` tests |
| Leading `(?i)` regex prefix | Supported subset | URL, body, header, and script regex test |
| PCRE shorthand classes `\d`, `\w`, `\s` | Supported subset | URL, body, header, and script regex test |
| PCRE absolute anchors `\A`, `\z`, `\Z` | Supported subset | URL, body, header, and script regex test |
| PCRE non-capturing groups `(?:...)` | Supported as regular capturing groups | URL, body, header, and script regex test |
| PCRE named capture groups `(?<name>...)`, `(?'name'...)` | Supported as regular capturing groups | URL, body, header, and script regex test |
| PCRE quoted literals `\Q...\E` | Supported subset | URL, body, header, and script regex test |
| Request body mock | Supported for buffered plain text | `anixops_rewrite_apply_body` |
| Response body mock | Supported for buffered plain text | `anixops_rewrite_apply_body` |
| Request/response body regex replace | Supported for buffered plain text | POSIX ERE, global replacement tests |
| Request/response body JSON path replace | Supported subset for buffered JSON | `$.field`, `$['field']`, common escaped and `\uXXXX` bracket keys, nested object path, and array index tests |
| Request header add/replace/regex replace | Supported as structured operation | `anixops_rewrite_evaluate_header` |
| Response header add/replace/delete/regex replace | Supported as structured operation | `anixops_rewrite_evaluate_header` |
| Full JQ-style JSON rewrites | Not implemented | See `docs/TODO.md` |
| Compression/chunk handling | Out of scope | adapter responsibility |

## Script Dispatch

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| `http-request` / `http-response` plugin lines | Supported subset | `anixops_script_evaluate_url` |
| `requires-body` / `requires_body` | Supported | response/request script tests |
| `script-path` / `script_path` | Supported | plugin and Surge tests |
| `tag` | Supported | plugin and Surge tests |
| `argument=[{Name},...]` | Supported | BiliBili fixture tests |
| Surge template arguments `{Name}`, `{{{Name}}}` | Supported subset | `surge_style_script_rule_template_is_supported` |
| Malformed Surge attr-list script rules | Ignored, except invalid regex reports an error | `malformed_and_non_http_script_rules_are_ignored_or_rejected` |
| Quantumult X `url script-request-body` / `script-request-header` / `script-response-body` | Supported subset | snippet and `[rewrite_local]` tests |
| JavaScript runtime | Out of scope | `docs/script_runtime_contract.md` |

## Diagnostics And ABI

| Feature | Status | Public API / Evidence |
| --- | --- | --- |
| Stable opaque engine handle | Supported | `anixops_engine_t` opaque typedef |
| Caller-owned result structs | Supported | no public internal pointers |
| Status text | Supported | `anixops_status_message` |
| Last error status/line/message copy | Supported | `anixops_engine_copy_last_error` |
| ABI export allowlist | Supported | `ci/abi_exports.txt` checked by `scripts/check.sh` |
| Thread-safe mutation | Not provided | external synchronization required |

## Current End-To-End Evidence

- `make demo-check`: pure C strategy-chain demo, no sockets/TLS/JS.
- `make test`: public C ABI unit tests, including representative Loon, Surge, and Quantumult X fixture parsing.
- `make e2e`: local shim plus mihomo, proving library decisions through a proxy path.
- `make bili-e2e`: BiliUniverse Enhanced plugin/script dispatch and script execution fixture.
- `make script-contract-e2e`: request/response script metadata and adapter writeback contract.
- `make bilibili-homepage-demo-e2e`: built-in Windows Bilibili homepage demo behavior through the shim path.
