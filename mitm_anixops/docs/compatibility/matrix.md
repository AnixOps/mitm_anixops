# v1.0.0 Compatibility Matrix

This matrix describes the v1.0.0 target state and the current evidence. It must
not overstate planned or partial behavior.

| Capability | Ecosystem | Status | Source Contract | Parser Case | Positive Test | Negative Test | Compatibility Note | Unimplemented Items |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Loon plugin common fields | Loon | partial | `loon-plugin-common-fields.md` | `Loon.CommonFields.plugin`, `BiliBili.Enhanced.plugin`, `Representative.Loon.plugin` | `config/loon_common_fields_fixture_is_supported`, CI runner corpus, and C parser tests | `config/loon_common_fields_strict_fixture_rejects_malformed_rule` | High-frequency subset only | full Loon grammar, broader corpus, cron/task |
| Quantumult X rewrite/task/mitm common config | Quantumult X | partial | `quantumultx-common-config.md` | `QuantumultX.CommonConfig.snippet`, `QuantumultX.CommonConfig.Malformed.snippet`, `Representative.QuantumultX.snippet` | `config/quantumultx_common_config_fixture_is_supported`, C rewrite/script tests, and runner corpus | `config/quantumultx_common_config_strict_fixture_rejects_malformed_rule` | Rewrite/MITM subset is covered; task/cron remains planned | task/cron, full grammar, broader corpus |
| Surge module common config | Surge | partial | `surge-module-common-config.md` | `Surge.CommonConfig.sgmodule`, `Surge.CommonConfig.Malformed.sgmodule`, `Representative.Surge.sgmodule` | `config/surge_common_config_fixture_is_supported`, C script tests, and runner corpus | `config/surge_common_config_strict_fixture_rejects_malformed_rule` | Module metadata, arguments, script, rewrite, and MITM subset is covered | full module grammar, requirement behavior, body/JQ corpus, scheduler/runtime |
| Stash migration notes | Stash | planned | pending | none | none | none | Treat as migration mapping until fixtures exist | source contract, parser fixtures, tests |
| Shadowrocket migration notes | Shadowrocket | planned | pending | none | none | none | Treat as migration mapping until fixtures exist | source contract, parser fixtures, tests |
| hostname / MITM domain matching | portable, Loon, QX, Surge | partial | `source-contracts.md#mitm-hostname-policy` | MITM host lines in representative fixtures | `tests/test_mitm.c`, strategy demo | empty host, untrusted cert state, no host match | Adapter supplies certificate state | platform certificate lifecycle |
| request rewrite | portable, Loon, QX | partial | `request-rewrite-common.md` | `RequestRewrite.Common.conf`, `RequestRewrite.Common.Malformed.conf`, representative rewrite fixtures | `config/request_rewrite_common_fixture_is_supported`, C rewrite tests, and runner replay | `config/request_rewrite_common_strict_fixture_rejects_malformed_rule` | URL redirect/reject subset; direct/proxy routing is not claimed | broader ecosystem grammar, remote rule refresh, direct/proxy policy intent |
| response rewrite | portable, Loon, QX | partial | `response-rewrite-common.md` | `ResponseRewrite.Common.conf`, `ResponseRewrite.Common.Malformed.conf`, representative rewrite fixtures | `config/response_rewrite_common_fixture_is_supported`, C rewrite tests and script contract E2E | `config/response_rewrite_common_fixture_rejects_invalid_body_regex`, response/request phase mismatch | Response-phase mock/echo/body-regex policy core; streaming/framing is not claimed | streaming, compression/framing, charset matrix, production adapter behavior |
| header mutation | portable, Loon, QX | partial | `header-mutation-common.md` | `HeaderMutation.Common.conf`, `HeaderMutation.Common.Malformed.conf`, header rewrite fixtures in C tests | `config/header_mutation_common_fixture_is_supported`, named-header and header-list tests | `config/header_mutation_common_fixture_rejects_invalid_regex`, invalid header regex and capacity behavior | Bounded policy-core header list; platform header map is not claimed | unbounded platform map behavior, hop-by-hop filtering, broader cookie/header corpus |
| body mutation | portable, Loon, Surge, QX | partial | `body-mutation-common.md` | `BodyMutation.Common.conf`, `BodyMutation.Common.Malformed.conf`, body rewrite/JQ fixtures in C tests | `config/body_mutation_common_fixture_is_supported`, regex, JSON path, chain, optional JQ tests | `config/body_mutation_common_fixture_rejects_invalid_body_regex`, invalid JSON, truncation, unavailable backend | Already-buffered body policy core; streaming/compression is not claimed | streaming, charset matrix, brotli/zstd, broader JSON/JQ corpus |
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
