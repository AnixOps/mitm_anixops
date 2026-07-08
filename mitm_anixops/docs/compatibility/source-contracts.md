# Compatibility Source Contracts

Source contracts define expected behavior before implementation. A feature is
not ready for implementation until its source contract states inputs, outputs,
errors, and CI evidence.

## Contract Template

Each new capability contract must include:

```text
Capability:
Ecosystem: loon | quantumultx | surge | stash | shadowrocket | portable
Input form:
Parser output:
Positive case:
Negative case:
Runtime/matching behavior:
Diagnostics:
Security boundary:
CI evidence:
Compatibility matrix row:
Unimplemented items:
```

## Current Baseline Contracts

### Loon Plugin Common Subset

Detailed contract: [Loon Plugin Common Fields Source Contract](loon-plugin-common-fields.md).

Capability: parse and evaluate the currently implemented Loon/AnixOps subset.

Input form:

- `[MITM] hostname = ...`
- `[Rewrite]`, `[URL Rewrite]`, `[Header Rewrite]`, `[Body Rewrite]`
- `[Script]`
- `[Argument]`
- `[Plugin]` metadata tolerance

Parser output:

- MITM host config;
- URL/header/body rewrite rules;
- script dispatch metadata;
- argument defaults and overrides;
- accepted, ignored, and rejected diagnostics.

Current CI evidence:

- C parser and rewrite tests under `tests/`;
- runner corpus entry `Representative.Loon.plugin`;
- BiliUniverse Loon fixture in `tests/fixtures/corpus/manifest.json`;
- GitHub Actions `linux-test` job running `sh scripts/check.sh`.

Unimplemented items:

- complete Loon grammar;
- broader real plugin corpus;
- cron/task trigger contract;
- platform JS runtime beyond Alpha Node runner.

### Loon Hashbang Metadata

Detailed contract: [Loon Hashbang Metadata Source Contract](loon-hashbang-metadata.md).

Capability: tolerate common Loon `#!` metadata without claiming runtime or
platform UI behavior.

Input form:

- `#!name`, `#!desc`, `#!description`;
- `#!author`, `#!category`, `#!icon`, `#!homepage`;
- `#!date`, `#!system`, `#!requirement`, `#!arguments-desc`.

Parser output:

- ignored diagnostics with section `Plugin` and action equal to the metadata
  key;
- no MITM, rewrite, script, body, header, routing, certificate, or UI behavior
  from metadata.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.HashbangMetadata.plugin`;
- negative fixture `tests/fixtures/Loon.HashbangMetadata.Unsupported.plugin`;
- `config/loon_hashbang_metadata_fixture_records_tolerated_keys`;
- `config/loon_hashbang_metadata_unsupported_keys_are_not_claimed`.

Unimplemented items:

- platform UI behavior for icons, homepages, external links, or install
  prompts;
- broader Loon metadata corpus;
- first-class validation of unsupported metadata keys.

### Loon Plugin Metadata

Detailed contract: [Loon Plugin Metadata Source Contract](loon-plugin-metadata.md).

Capability: tolerate Loon `[Plugin]` metadata without claiming runtime,
policy, or platform UI behavior.

Input form:

- `[Plugin]` section;
- key/value metadata lines such as `name`, `desc`, `author`, `icon`, and
  `homepage`;
- unsupported rule-shaped lines remain metadata-only while inside `[Plugin]`.

Parser output:

- ignored diagnostics with section `Plugin`, action `line`, and message
  `plugin metadata ignored`;
- no MITM, rewrite, script, argument, task, routing, certificate, or UI
  behavior from metadata.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.PluginMetadata.plugin`;
- negative fixture `tests/fixtures/Loon.PluginMetadata.Unsupported.plugin`;
- `config/loon_plugin_metadata_fixture_records_ignored_lines`;
- `config/loon_plugin_metadata_unsupported_keys_are_not_claimed`.

Unimplemented items:

- platform UI behavior for names, icons, homepages, or install prompts;
- validation or typed extraction of individual plugin metadata keys;
- broader Loon plugin metadata corpus.

### Loon Inline Arguments

Detailed contract: [Loon Inline Arguments Source Contract](loon-inline-arguments.md).

Capability: parse `#!arguments` defaults and resolve them through script
argument templates without claiming platform UI behavior.

Input form:

- `#!arguments = Name:value,Other:"quoted value"`;
- comma-separated `name:value` fields;
- malformed non-empty fields without `:` are rejected.

Parser output:

- accepted diagnostics with section `Argument` and action `arguments`;
- argument defaults visible through script argument template resolution;
- no MITM, rewrite, certificate, routing, or platform UI behavior from inline
  arguments.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.InlineArguments.plugin`;
- negative fixture `tests/fixtures/Loon.InlineArguments.Malformed.plugin`;
- `config/loon_inline_arguments_fixture_resolves_script_defaults`;
- `config/loon_inline_arguments_malformed_fixture_rejects_missing_separator`.

Unimplemented items:

- host-platform UI controls for selecting argument values;
- broader Loon argument metadata forms;
- persistence or remote argument sources.

### Quantumult X Common Subset

Detailed contract:
[Quantumult X Common Config Source Contract](quantumultx-common-config.md).

Capability: parse and evaluate the currently implemented Quantumult X subset.

Input form:

- `#[rewrite_local]`
- `#[rewrite_remote]`
- `#[mitm]`
- `url`-prefixed rewrite/script/header/body forms;
- `force-http-engine-hosts`;
- host-list `skip-server-cert-verify`.

Current CI evidence:

- positive fixture `tests/fixtures/QuantumultX.CommonConfig.snippet`;
- negative fixture `tests/fixtures/QuantumultX.CommonConfig.Malformed.snippet`;
- `config/quantumultx_common_config_fixture_is_supported`;
- `config/quantumultx_common_config_strict_fixture_rejects_malformed_rule`;
- C parser/script/rewrite tests;
- runner corpus entry `Representative.QuantumultX.snippet`.

Unimplemented items:

- task/cron behavior;
- full rewrite/task grammar;
- broader corpus and migration notes.

### Quantumult X MITM Options

Detailed contract:
[Quantumult X MITM Options Source Contract](quantumultx-mitm-options.md).

Capability: parse Quantumult X `[mitm]` and `#[mitm]` host/options lines as
policy-core and adapter-visible signals.

Input form:

- `enable`;
- `hostname` and `force-http-engine-hosts`;
- `skip-server-cert-verify` and `skip_server_cert_verify`;
- `h2`, `h2-enable`, and `h2_enable`;
- `disable-quic`, `disable_quic`, `disable-mitm-quic`, and
  `disable_mitm_quic`.

Current CI evidence:

- positive fixture `tests/fixtures/QuantumultX.MitmOptions.snippet`;
- negative fixture `tests/fixtures/QuantumultX.MitmOptions.Malformed.snippet`;
- `config/quantumultx_mitm_options_fixture_exposes_adapter_flags`;
- `config/quantumultx_mitm_options_malformed_fixture_rejects_invalid_host`.

Unimplemented items:

- root CA installation or trust-store mutation;
- TLS interception and HTTP/2 transport behavior;
- adapter certificate verification bypass behavior;
- QUIC network fallback outside the policy-core decision signal.

### Surge Module Common Subset

Detailed contract: [Surge Module Common Config Source Contract](surge-module-common-config.md).

Capability: parse and evaluate the currently implemented Surge module subset.

Input form:

- `#!` metadata diagnostics;
- `#!arguments`;
- `%APPEND%` and `%INSERT%`;
- `[Script]` attr-list fields including type, pattern, script-path,
  requires-body, timeout, max-size, tag, and argument;
- selected body/JQ rewrite forms.

Current CI evidence:

- positive fixture `tests/fixtures/Surge.CommonConfig.sgmodule`;
- negative fixture `tests/fixtures/Surge.CommonConfig.Malformed.sgmodule`;
- `config/surge_common_config_fixture_is_supported`;
- `config/surge_common_config_strict_fixture_rejects_malformed_rule`;
- C parser/script tests;
- runner corpus entry `Representative.Surge.sgmodule`;
- optional `JQ=1` GitHub Actions coverage when libjq headers are installed.

Unimplemented items:

- full Surge module grammar;
- broader body rewrite and JQ corpus;
- requirement metadata behavior.

### Shadowrocket Common Config Subset

Detailed contract:
[Shadowrocket Common Config Source Contract](shadowrocket-common-config.md).

Capability: parse and evaluate the currently implemented Shadowrocket common
config subset.

Input form:

- `[URL Rewrite]` URL redirect and reject rules;
- `[Script]` attr-list fields including type, pattern, script-path,
  requires-body, timeout, max-size, tag, and argument;
- `[MITM] hostname = ...`;
- `[MITM] skip-server-cert-verify = ...`;
- `[MITM] h2 = ...`.

Current CI evidence:

- positive fixture `tests/fixtures/Shadowrocket.CommonConfig.conf`;
- negative fixture `tests/fixtures/Shadowrocket.CommonConfig.Malformed.conf`;
- `config/shadowrocket_common_config_fixture_is_supported`;
- `config/shadowrocket_common_config_fixture_rejects_invalid_regex`;
- app-profile guard fixture `tests/fixtures/Shadowrocket.MigrationGuard.conf`
  remains unsupported syntax evidence.

Unimplemented items:

- full Shadowrocket app-level profile grammar;
- proxy nodes, routing policy, DNS, VPN, packet-capture, or UI behavior;
- certificate lifecycle;
- runtime and scheduler behavior.

### Stash Migration And Shadowrocket App-Profile Boundary

Detailed notes:
[Stash And Shadowrocket Migration Notes](stash-shadowrocket-migration.md).

Capability: record migration guidance for Stash and for Shadowrocket syntax
outside the dedicated common-config source contract.

Input form:

- `tests/fixtures/Stash.MigrationGuard.yaml` as a non-support guard;
- `tests/fixtures/Shadowrocket.MigrationGuard.conf` as a non-support guard;
- no first-class Stash parser fixture is accepted as supported syntax yet;
- Shadowrocket app-level profile sections remain unsupported unless they are
  explicitly covered by `shadowrocket-common-config.md`.

Current CI evidence:

- Stash compatibility matrix row remains `planned`;
- Shadowrocket common-config matrix row is `partial`;
- `config/stash_migration_guard_fixture_stays_parser_unsupported`;
- `config/shadowrocket_migration_guard_fixture_stays_parser_unsupported`;
- governance checks require migration and app-profile boundary markers plus
  guard fixture/test links.

Unimplemented items:

- dedicated Stash source contract, parser fixtures, positive tests, and negative
  tests;
- expanded Shadowrocket app-level profile source contract, parser fixtures,
  positive tests, and negative tests.

### MITM Hostname Policy

Capability: evaluate whether a host is eligible for MITM and whether QUIC should
be rejected for fallback.

Input form:

- exact hostnames;
- wildcard/suffix hostnames;
- deny host entries;
- host with port;
- IPv6 literal;
- malformed host parser fixture `tests/fixtures/MITM.Hostname.Malformed.conf`;
- certificate state supplied by adapter.

Current CI evidence:

- `tests/test_mitm.c`;
- `config/mitm_hostname_malformed_fixture_rejects_invalid_host`;
- `mitm/malformed_runtime_hosts_do_not_intercept_even_with_wildcard`;
- strategy-chain demo;
- runner and proxy E2E paths.

Unimplemented items:

- platform certificate installation;
- trust store mutation;
- platform-specific revocation and expiry probes.

### Certificate Lifecycle And Trust Gate

Detailed contract:
[Certificate Lifecycle Architecture Contract](../architecture/certificate-lifecycle.md).

Capability: define the policy-core trust gate and adapter-owned certificate
lifecycle boundary.

Input form:

- MITM hostname parser output;
- adapter-supplied `anixops_cert_state_t`;
- runtime hostname supplied by the adapter;
- QUIC flag supplied by the adapter.

Current CI evidence:

- `mitm/certificate_state_matrix_blocks_untrusted_states`;
- `mitm/no_host_match_bypasses`;
- `mitm/malformed_runtime_hosts_do_not_intercept_even_with_wildcard`;
- `scripts/security-claim-check.sh`.

Unimplemented items:

- production root CA generation;
- platform certificate store integration;
- dynamic leaf certificate generation and signing;
- trust install, revoke, remove, and rollback workflows;
- protected storage for CA private keys and signing materials.

### Request Rewrite Common Subset

Detailed contract: [Request Rewrite Common Source Contract](request-rewrite-common.md).

Capability: parse and evaluate request URL rewrite rules.

Input form:

- `[Rewrite]`, `[URL Rewrite]`, and `[Remote Rewrite]`;
- direct redirect rules;
- URL-prefixed redirect rules;
- reject rules.

Current CI evidence:

- positive fixture `tests/fixtures/RequestRewrite.Common.conf`;
- negative fixture `tests/fixtures/RequestRewrite.Common.Malformed.conf`;
- `config/request_rewrite_common_fixture_is_supported`;
- `config/request_rewrite_common_strict_fixture_rejects_malformed_rule`;
- C rewrite tests under `tests/test_rewrite.c`;
- runner replay entries for representative rewrite behavior.

Unimplemented items:

- remote rule refresh;
- direct/proxy route selection;
- full ecosystem rewrite grammar;
- adapter HTTP request serialization.

### Policy Intent Common Subset

Detailed contract:
[Policy Intent Common Source Contract](policy-intent-common.md).

Capability: parse reject policy intent while keeping direct/proxy route
selection unsupported.

Input form:

- `[Rewrite]`, `[URL Rewrite]`, and `[Remote Rewrite]`;
- reject, reject-current-session, numeric reject, and reject body-shape tokens;
- unsupported direct/proxy route-intent lines.

Current CI evidence:

- positive fixture `tests/fixtures/PolicyIntent.Common.conf`;
- negative fixture `tests/fixtures/PolicyIntent.Unsupported.conf`;
- `config/policy_intent_common_fixture_covers_reject_subset`;
- `config/policy_intent_unsupported_routes_are_ignored`;
- C rewrite reject tests under `tests/test_rewrite.c`.

Unimplemented items:

- direct/proxy route selection;
- proxy group resolution;
- adapter route priority, fallback, DNS, socket, and platform network-extension
  behavior.

### Header Mutation Common Subset

Detailed contract: [Header Mutation Common Source Contract](header-mutation-common.md).

Capability: parse and evaluate request/response header mutation rules.

Input form:

- `[Header Rewrite]`, `[Remote Header Rewrite]`, `[Rewrite]`, and
  `[URL Rewrite]`;
- request header add, replace, delete, and regex replace rules;
- response header add, replace, delete, and regex replace rules.

Current CI evidence:

- positive fixture `tests/fixtures/HeaderMutation.Common.conf`;
- negative fixture `tests/fixtures/HeaderMutation.Common.Malformed.conf`;
- `config/header_mutation_common_fixture_is_supported`;
- `config/header_mutation_common_fixture_rejects_invalid_regex`;
- named-header and header-list tests under `tests/test_rewrite.c`.

Unimplemented items:

- unbounded platform header map behavior;
- hop-by-hop header filtering;
- broader cookie/header corpus;
- adapter HTTP request/response serialization.

### Response Rewrite Common Subset

Detailed contract: [Response Rewrite Common Source Contract](response-rewrite-common.md).

Capability: parse and evaluate response-phase rewrite rules.

Input form:

- `[Rewrite]`, `[URL Rewrite]`, `[Remote Rewrite]`, and `[rewrite_local]`;
- `mock-response-body`;
- `echo-response`;
- `response-body-replace-regex`.

Current CI evidence:

- positive fixture `tests/fixtures/ResponseRewrite.Common.conf`;
- negative fixture `tests/fixtures/ResponseRewrite.Common.Malformed.conf`;
- `config/response_rewrite_common_fixture_is_supported`;
- `config/response_rewrite_common_fixture_rejects_invalid_body_regex`;
- response phase rewrite tests under `tests/test_rewrite.c`;
- script contract E2E coverage for Alpha response ordering.

Unimplemented items:

- streaming response rewrite;
- compression/framing behavior;
- charset matrix;
- production adapter HTTP response serialization.

### Body Mutation Common Subset

Detailed contract: [Body Mutation Common Source Contract](body-mutation-common.md).

Capability: parse and evaluate request/response body mutation rules.

Input form:

- `[Body Rewrite]`, `[Remote Body Rewrite]`, `[Rewrite]`, and
  `[URL Rewrite]`;
- request and response body regex replacement rules;
- request and response body JSON replacement rules.

Current CI evidence:

- positive fixture `tests/fixtures/BodyMutation.Common.conf`;
- negative fixture `tests/fixtures/BodyMutation.Common.Malformed.conf`;
- `config/body_mutation_common_fixture_is_supported`;
- `config/body_mutation_common_fixture_rejects_invalid_body_regex`;
- regex, JSON path, body-chain, and optional JQ tests under
  `tests/test_rewrite.c`.

Unimplemented items:

- streaming body mutation;
- compression/framing behavior;
- charset matrix;
- broader JSON/JQ corpus and production runtime limits;
- production adapter HTTP body serialization.

### Decision Trace Schema

Detailed contract: [Decision Trace Schema Source Contract](decision-trace-schema.md).

Capability: expose structured policy-core trace fields for host, URL, header,
body, script trigger, and policy intent decisions.

Input form:

- `tests/fixtures/DecisionTrace.Schema.conf`;
- MITM hostname policy;
- URL redirect and reject rewrite rules;
- request header mutation;
- request body mutation;
- request script trigger metadata.

Current CI evidence:

- positive fixture `tests/fixtures/DecisionTrace.Schema.conf`;
- negative fixture `tests/fixtures/DecisionTrace.Schema.UnsupportedPolicy.conf`;
- golden runner fixtures `tests/fixtures/BindingParity.RequestTrace.json` and
  `tests/fixtures/BindingParity.ResponseTrace.json`;
- MITM golden runner fixtures `tests/fixtures/BindingParity.MitmTrace.json`
  and `tests/fixtures/BindingParity.MitmQuicTrace.json`;
- negative MITM golden runner fixtures
  `tests/fixtures/LoonCommonFields.MitmCertUntrustedTrace.json`,
  `tests/fixtures/LoonCommonFields.MitmDenyTrace.json`, and
  `tests/fixtures/LoonCommonFields.MitmNoHostTrace.json`;
- disabled and malformed MITM golden runner fixtures
  `tests/fixtures/LoonCommonFields.MitmDisabledTrace.json`,
  `tests/fixtures/LoonCommonFields.MitmEmptyHostTrace.json`, and
  `tests/fixtures/LoonCommonFields.MitmMalformedHostTrace.json`;
- `config/decision_trace_schema_fixture_covers_policy_fields`;
- `config/decision_trace_schema_fixture_ignores_unsupported_policy_intent`;
- architecture contract `docs/architecture/adapter-redaction-policy.md`;
- `scripts/security-claim-check.sh` payload logging claim guard;
- runner `trace` smoke checks under `runner-check`.

Unimplemented items:

- production adapter redaction implementation;
- direct/proxy route-selection contract;
- serialized trace object for every ABI decision.

### Plan API Parity

Detailed contract: [Plan API Parity Source Contract](plan-api-parity.md).

Capability: prove `anixops_rewrite_build_plan` matches the legacy evaluation
APIs for the same input.

Input form:

- `tests/fixtures/PlanApiParity.Golden.conf`;
- `tests/fixtures/PlanApiParity.PhaseMismatch.conf`;
- request and response URL/header/body/script rules;
- request and response current-header regex mutation rules.

Current CI evidence:

- positive fixture `tests/fixtures/PlanApiParity.Golden.conf`;
- negative fixture `tests/fixtures/PlanApiParity.PhaseMismatch.conf`;
- `config/plan_api_parity_fixture_matches_legacy_evaluation`;
- `config/plan_api_parity_fixture_keeps_phase_mismatches_empty`;
- Go wrapper `BuildPlanWithCurrentHeader`;
- Rust wrapper `build_plan_with_current_header`;
- `rewrite/rewrite_plan_matches_individual_evaluation_order`.

Unimplemented items:

- adapter ordering coverage beyond the shared binding golden JSON traces.

### Binding Parity

Detailed contract: [Binding Parity Source Contract](binding-parity.md).

Capability: prove the C runner, Go wrapper, and Rust wrapper expose equivalent
policy outputs from a shared fixture.

Input form:

- `tests/fixtures/BindingParity.Common.conf`;
- `tests/fixtures/BindingParity.RequestTrace.json`;
- `tests/fixtures/BindingParity.ResponseTrace.json`;
- `tests/fixtures/BindingParity.MitmTrace.json`;
- `tests/fixtures/BindingParity.MitmQuicTrace.json`;
- request/response URL, header, body, and script policy rules;
- TCP allow and QUIC fallback MITM decisions;
- request/response named-header current-value rewrite rules;
- argument substitution shared across all binding surfaces.

Current CI evidence:

- C runner `scan` and `trace` checks in `make runner-check`;
- C runner request/response/MITM golden JSON trace comparisons;
- Go wrapper `TestGoBindingLoadsSharedParityFixture`, including named-header
  current-value rewrite checks;
- Rust wrapper `rust_binding_loads_shared_parity_fixture`, including
  named-header current-value rewrite checks.

Unimplemented items:

- release-package binding surface parity.

### Script Trigger Metadata

Capability: select request/response script rules and expose script metadata.

Input form:

- Loon/AnixOps style script rules;
- Surge attr-list rules;
- Quantumult X url-prefixed script forms.

Current CI evidence:

- `tests/test_script.c`;
- runner replay with Node contract runner;
- script contract E2E;
- script exception and timeout fail-open coverage.

Unimplemented items:

- production embedded JS runtime;
- cron/task scheduler/runtime execution;
- remote script cache refresh policy;
- memory limits.

### Script Runtime Common Subset

Detailed contract: [Script Runtime Common Source Contract](script-runtime-common.md).

Capability: execute matched request/response scripts through an adapter-owned
runtime with portable globals and fail-open behavior.

Input form:

- request and response script metadata from Loon/AnixOps, Surge, and
  Quantumult X parser subsets;
- local no-network assets supplied by runner `--script-map` or
  `--script-bundle`;
- rule-level `argument`, `timeout`, `max-size`, `requires-body`, and `tag`
  metadata.

Current CI evidence:

- runner replay with the Alpha Node contract runner;
- `$request`, `$response`, `$argument`, `$persistentStore`, and `$done.body`
  replay evidence;
- double `$done` first-wins fixture
  `tests/fixtures/runner_double_done_script.js`;
- timeout and exception fail-open coverage in `script-contract-e2e`;
- script bundle sha256 match, digest mismatch, and cache miss diagnostics.

Unimplemented items:

- production embedded QuickJS, JavaScriptCore, or other JavaScript runtime. The
  current v1 policy-core dependency decision is no embedded JavaScript engine;
- production script download/cache refresh/signature policy;
- full platform Web API compatibility;
- streaming and binary body runtime behavior;
- production memory, CPU, storage, and log-redaction policy.

### Cron And Task Trigger

Detailed contract: [Cron And Task Trigger Source Contract](cron-task-trigger.md).

Capability: parse cron and interval task trigger metadata without claiming
scheduler/runtime support.

Input form:

- direct `[Script]` cron task declarations;
- `[Script]` attr-list rules with `type=cron`, `type=interval`, or
  `type=task` plus a concrete cron expression or interval;
- `script-path`, `tag`, `argument`, `timeout`, `max-size`, and `enabled`
  metadata;
- positive parser fixture `tests/fixtures/CronTaskTrigger.HttpScriptGuard.conf`;
- unsupported parser fixture `tests/fixtures/CronTaskTrigger.Unsupported.conf`;
- malformed parser fixture `tests/fixtures/CronTaskTrigger.Malformed.conf`.

Current CI evidence:

- `config/cron_task_trigger_common_fixture_emits_task_descriptors` proves HTTP
  script triggers stay separate from cron and interval task descriptors;
- `config/cron_task_trigger_unsupported_fixture_does_not_register_descriptors`
  proves unsupported scheduler-like lines do not register as HTTP scripts or
  task descriptors;
- `config/cron_task_trigger_malformed_fixture_rejects_invalid_cron` proves
  malformed cron descriptors are rejected;
- `script/malformed_and_non_http_script_rules_are_ignored_or_rejected` proves a
  bare cron rule does not register as an HTTP script rule when using the HTTP
  script parser directly;
- public ABI functions `anixops_engine_task_descriptor_count` and
  `anixops_engine_copy_task_descriptor`;
- GitHub Actions governance requires the contract and matrix row.

Unimplemented items:

- scheduler/runtime replay or E2E evidence;
- task JavaScript bindings and concurrency policy.
