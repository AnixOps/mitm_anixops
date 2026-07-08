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
- `config/decision_trace_schema_fixture_covers_policy_fields`;
- `config/decision_trace_schema_fixture_ignores_unsupported_policy_intent`;
- runner `trace` smoke checks under `runner-check`.

Unimplemented items:

- golden JSON trace fixtures;
- full runner MITM trace coverage;
- adapter redaction policy;
- direct/proxy route-selection contract;
- serialized trace object for every ABI decision.

### Plan API Parity

Detailed contract: [Plan API Parity Source Contract](plan-api-parity.md).

Capability: prove `anixops_rewrite_build_plan` matches the legacy evaluation
APIs for the same input.

Input form:

- `tests/fixtures/PlanApiParity.Golden.conf`;
- `tests/fixtures/PlanApiParity.PhaseMismatch.conf`;
- request and response URL/header/body/script rules.

Current CI evidence:

- positive fixture `tests/fixtures/PlanApiParity.Golden.conf`;
- negative fixture `tests/fixtures/PlanApiParity.PhaseMismatch.conf`;
- `config/plan_api_parity_fixture_matches_legacy_evaluation`;
- `config/plan_api_parity_fixture_keeps_phase_mismatches_empty`;
- `rewrite/rewrite_plan_matches_individual_evaluation_order`.

Unimplemented items:

- runner golden JSON trace fixtures;
- binding parity fixtures for Go and Rust wrappers;
- named-header current-value parity matrix;
- adapter ordering coverage.

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
- cron/task triggers;
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

Capability: define the planned scheduler/task boundary without claiming runtime
support.

Input form:

- future Quantumult X task and cron forms;
- future Surge scheduled script forms;
- future Loon or AnixOps-style scheduled script/task declarations from the
  supported corpus.

Current CI evidence:

- `script/malformed_and_non_http_script_rules_are_ignored_or_rejected` proves a
  bare cron rule does not register as an HTTP script rule;
- GitHub Actions governance requires the planned contract and matrix row.

Unimplemented items:

- parser fixtures and positive/negative parser tests;
- public task descriptor API;
- scheduler/runtime replay or E2E evidence;
- task JavaScript bindings and concurrency policy.
