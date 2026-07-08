# v1.0.0 Compatibility Matrix

This matrix describes the v1.0.0 target state and the current evidence. It must
not overstate planned or partial behavior.

| Capability | Ecosystem | Status | Source Contract | Parser Case | Positive Test | Negative Test | Compatibility Note | Unimplemented Items |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Loon plugin common fields | Loon | partial | `source-contracts.md#loon-plugin-common-subset` | `Representative.Loon.plugin`, `BiliBili.Enhanced.plugin` | CI runner corpus and C parser tests | malformed config diagnostics and strict profile tests | High-frequency subset only | full Loon grammar, broader corpus, cron/task |
| Quantumult X rewrite/task/mitm common config | Quantumult X | partial | `source-contracts.md#quantumult-x-common-subset` | `Representative.QuantumultX.snippet` | C rewrite/script tests and runner corpus | unsupported recognized rewrite actions and strict diagnostics | Rewrite/mitm subset exists; task is not supported | task/cron, full grammar, broader corpus |
| Surge module common config | Surge | partial | `source-contracts.md#surge-module-common-subset` | `Representative.Surge.sgmodule` | C script tests and runner corpus | malformed/non-HTTP script rules and diagnostics | Module metadata/script subset exists | full body/JQ corpus, requirement metadata |
| Stash migration notes | Stash | planned | pending | none | none | none | Treat as migration mapping until fixtures exist | source contract, parser fixtures, tests |
| Shadowrocket migration notes | Shadowrocket | planned | pending | none | none | none | Treat as migration mapping until fixtures exist | source contract, parser fixtures, tests |
| hostname / MITM domain matching | portable, Loon, QX, Surge | partial | `source-contracts.md#mitm-hostname-policy` | MITM host lines in representative fixtures | `tests/test_mitm.c`, strategy demo | empty host, untrusted cert state, no host match | Adapter supplies certificate state | platform certificate lifecycle |
| request rewrite | portable, Loon, QX | partial | current Alpha matrix, pending v1 contract | representative rewrite fixtures | C rewrite tests and runner replay | phase mismatch and unsupported action tests | URL redirect/reject subset | broader ecosystem grammar |
| response rewrite | portable, Loon, QX | partial | current Alpha matrix, pending v1 contract | representative rewrite fixtures | C rewrite tests and script contract E2E | response/request phase mismatch | Header/body/static rewrite order is covered in Alpha shim | streaming and production framing |
| header mutation | portable, Loon, QX | partial | current Alpha matrix, pending v1 contract | header rewrite fixtures in C tests | named-header and header-list tests | invalid header regex and capacity behavior | Bounded adapter-owned header list | unbounded platform map behavior |
| body mutation | portable, Loon, Surge, QX | partial | current Alpha matrix, pending v1 contract | body rewrite/JQ fixtures in C tests | regex, JSON path, chain, optional JQ tests | invalid JSON, truncation, unavailable backend | Already-buffered body policy core | streaming, charset matrix, brotli/zstd |
| script trigger | Loon, Surge, QX | partial | `source-contracts.md#script-trigger-metadata` | representative script fixtures | C script tests, runner replay, script E2E | disabled scripts, malformed script rules, timeout/throw fail-open | Dispatch metadata and Alpha Node runner | production JS runtime, remote cache refresh |
| cron/task trigger | Loon, Quantumult X, Surge | planned | pending | none | none | none | Not implemented; must not be claimed supported | source contract, parser, scheduler/runtime |
| reject/direct/proxy policy intent | portable migration target | planned | pending | reject subset exists for URL rewrite | reject URL rewrite tests | unsupported policy intent tests pending | Reject is partially mapped; direct/proxy intent is not routing | source contract and adapter mapping |

## Rules For Updating This Matrix

Do not change `planned` to `partial` or `supported` unless the same change adds
or references CI-covered parser cases, positive tests, negative tests, and a
compatibility note.

Do not mark adapter-owned behavior as supported by the C policy core. Certificate
trust, TLS, HTTP/2, compression/framing, production JS runtime, and platform
permission flows require adapter evidence.
