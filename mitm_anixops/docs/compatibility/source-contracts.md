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

- `[MITM] hostname = ...` plus adapter-visible options covered by
  [Loon MITM Options Source Contract](loon-mitm-options.md);
- `[Rewrite]`, `[URL Rewrite]`, `[Header Rewrite]`, `[Body Rewrite]`
- `[Script]`
- `[Argument]`
- `[Rule]` domain-suffix reject policy intent covered by
  [Loon Rule Reject Source Contract](loon-rule-reject.md)
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

### Loon Argument Section

Detailed contract:
[Loon Argument Section Source Contract](loon-argument-section.md).

Capability: parse Loon `[Argument]` section defaults and resolve them through
script argument templates without claiming platform UI behavior.

Input form:

- `[Argument]` lines such as `Name = select,default` or
  `Token = input,"quoted value"`;
- script argument templates using `{Name}`, `{{{Name}}}`, or list-style
  `[{Name}]` placeholders.

Parser output:

- accepted diagnostics with section `Argument` and action `argument`;
- argument defaults visible through script argument template resolution;
- runtime overrides through `anixops_engine_set_argument_value` take precedence;
- no MITM, rewrite, certificate, routing, task, or platform UI behavior from
  argument defaults.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.ArgumentSection.plugin`;
- negative fixture `tests/fixtures/Loon.ArgumentSection.Malformed.plugin`;
- `config/loon_argument_section_fixture_resolves_script_defaults`;
- `config/loon_argument_section_malformed_fixture_rejects_missing_equals`.

Unimplemented items:

- host-platform UI controls for selecting argument values;
- kind-specific validation beyond default extraction;
- persistence or remote argument sources.

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
- broader Loon inline argument metadata forms;
- persistence or remote argument sources.

### Loon Script Metadata

Detailed contract:
[Loon Script Metadata Source Contract](loon-script-metadata.md).

Capability: parse Loon `[Script]` request/response dispatch metadata without
claiming JavaScript runtime execution.

Input form:

- `[Script]` `http-request` and `http-response` rules;
- URL regex pattern and `script-path`;
- `requires-body`, `timeout`, `timeout-ms`, `max-size`, `tag`, `argument`,
  `enable`, and `enabled` metadata.

Parser output:

- accepted diagnostics with section `Script` and action `script`;
- request/response phase, script path, tag, argument, `requires_body`,
  `timeout_ms`, and `max_size` visible through `anixops_script_evaluate_url`;
- disabled script rules are stored but skipped during dispatch;
- no JavaScript execution, remote fetching, certificate, routing, task, or
  platform UI behavior from script metadata.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.ScriptMetadata.plugin`;
- negative fixture `tests/fixtures/Loon.ScriptMetadata.Malformed.plugin`;
- `config/loon_script_metadata_fixture_exposes_dispatch_fields`;
- `config/loon_script_metadata_malformed_fixture_rejects_missing_path`.

Unimplemented items:

- JavaScript execution;
- remote script download/cache/signature policy;
- runtime permission model;
- broader Loon script option corpus.

### Loon Task Metadata

Detailed contract:
[Loon Task Metadata Source Contract](loon-task-metadata.md).

Capability: parse Loon `[Script]` scheduled task descriptors without claiming
scheduler or runtime execution.

Input form:

- direct `[Script]` `cron "..." script-path=...` rules;
- `[Script]` attr-list rules with `type=cron`, `type=scheduled`,
  `type=scheduled-script`, `type=interval`, or `type=task`;
- `cronexp`, `cron`, `schedule`, `interval`, `script-path`, `tag`, `argument`,
  `timeout`, `timeout-ms`, `max-size`, `enable`, and `enabled` metadata.

Parser output:

- accepted diagnostics with section `Script` and action `task`;
- task kind, schedule or interval seconds, script path, tag, resolved argument,
  timeout, max-size, enabled state, and parser origin visible through
  `anixops_engine_copy_task_descriptor`;
- task descriptors remain separate from HTTP script URL dispatch;
- no scheduler, background execution, JavaScript task runtime, certificate,
  routing, or platform UI behavior from task metadata.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.TaskMetadata.plugin`;
- negative fixture `tests/fixtures/Loon.TaskMetadata.Malformed.plugin`;
- `config/loon_task_metadata_fixture_emits_task_descriptors`;
- `config/loon_task_metadata_malformed_fixture_rejects_invalid_cron`.

Unimplemented items:

- scheduler/runtime execution;
- task JavaScript bindings;
- concurrency and permission policy;
- broader Loon task option corpus.

### Loon MITM Options

Detailed contract:
[Loon MITM Options Source Contract](loon-mitm-options.md).

Capability: parse Loon `[MITM]` host/options lines as policy-core and
adapter-visible signals.

Input form:

- `enable` and `enabled`;
- `hostname` with exact, wildcard, deny-prefixed, comma-separated, and
  `%APPEND%` / `%INSERT%` host-list entries;
- `skip-server-cert-verify` and `skip_server_cert_verify`;
- `h2`, `h2-enable`, and `h2_enable`;
- `disable-quic`, `disable_quic`, `disable-mitm-quic`, and
  `disable_mitm_quic`.
- unsupported certificate-material keys `ca-p12`, `ca-passphrase`, and
  `ca-cert` remain ignored policy-core non-support evidence.

Parser output:

- MITM host patterns visible through `anixops_engine_mitm_pattern_count`;
- adapter-visible `skip-server-cert-verify` and HTTP/2 option signals;
- existing MITM enable and QUIC fallback decision signals;
- accepted diagnostics for valid option lines;
- rejected diagnostics and parse failure for malformed host patterns;
- no certificate lifecycle, trust-store mutation, TLS, HTTP/2 transport,
  routing, or platform UI behavior.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.MitmOptions.plugin`;
- negative fixture `tests/fixtures/Loon.MitmOptions.Malformed.plugin`;
- unsupported fixture
  `tests/fixtures/Loon.MitmCertificateUnsupported.plugin`;
- `config/loon_mitm_options_fixture_exposes_adapter_flags`;
- `config/loon_mitm_options_malformed_fixture_rejects_invalid_host`.
- `config/loon_mitm_certificate_unsupported_fixture_keeps_material_ignored`.

Unimplemented items:

- root CA installation or trust-store mutation;
- TLS interception and HTTP/2 transport behavior;
- adapter certificate verification bypass behavior;
- QUIC network fallback outside the policy-core decision signal.

### Loon Rule Reject Policy Intent

Detailed contract:
[Loon Rule Reject Source Contract](loon-rule-reject.md).

Capability: parse Loon `[Rule]` `URL-REGEX`, `DOMAIN`, `DOMAIN-KEYWORD`,
`DOMAIN-SUFFIX`, and `FINAL` reject policy intent.

Input form:

- `[Rule]` `URL-REGEX,<pattern>,REJECT`;
- `[Rule]` `URL-REGEX,<pattern>,REJECT-NNN` and other common reject variants
  already supported by the policy-core rewrite parser.
- `[Rule]` `DOMAIN-SUFFIX,<host>,REJECT`;
- `[Rule]` `DOMAIN-SUFFIX,<host>,REJECT-NNN` and other common reject variants
  already supported by the policy-core rewrite parser.
- `[Rule]` `DOMAIN,<host>,REJECT`;
- `[Rule]` `DOMAIN,<host>,REJECT-NNN` and other common reject variants already
  supported by the policy-core rewrite parser.
- `[Rule]` `DOMAIN-KEYWORD,<keyword>,REJECT`;
- `[Rule]` `DOMAIN-KEYWORD,<keyword>,REJECT-NNN` and other common reject
  variants already supported by the policy-core rewrite parser.
- `[Rule]` `FINAL,REJECT`;
- `[Rule]` `FINAL,REJECT-NNN` and other common reject variants already
  supported by the policy-core rewrite parser.

Parser output:

- request-phase rewrite reject rules visible through
  `anixops_rewrite_evaluate_url`;
- accepted diagnostics with section `Rule` and action `rule`;
- ignored diagnostics for direct/proxy route-selection lines in the documented
  fixture;
- rejected diagnostics and parse failure for malformed URL regexes, exact hosts,
  host suffixes, host keywords, or final reject action lines;
- no direct/proxy route selection, DNS, rule-provider refresh, certificate
  lifecycle, scheduler, runtime, or platform UI behavior.

Current CI evidence:

- positive fixture `tests/fixtures/Loon.RuleDomainReject.plugin`;
- negative fixture `tests/fixtures/Loon.RuleDomainReject.Malformed.plugin`;
- positive fixture `tests/fixtures/Loon.RuleDomainExactReject.plugin`;
- negative fixture `tests/fixtures/Loon.RuleDomainExactReject.Malformed.plugin`;
- positive fixture `tests/fixtures/Loon.RuleDomainKeywordReject.plugin`;
- negative fixture `tests/fixtures/Loon.RuleDomainKeywordReject.Malformed.plugin`;
- positive fixture `tests/fixtures/Loon.RuleFinalReject.plugin`;
- negative fixture `tests/fixtures/Loon.RuleFinalReject.Malformed.plugin`;
- positive fixture `tests/fixtures/Loon.RuleUrlRegexReject.plugin`;
- negative fixture `tests/fixtures/Loon.RuleUrlRegexReject.Malformed.plugin`;
- `config/loon_rule_domain_reject_fixture_maps_domain_suffix_rejects`;
- `config/loon_rule_domain_reject_malformed_fixture_rejects_invalid_domain`;
- `config/loon_rule_domain_exact_reject_fixture_maps_exact_domain_rejects`;
- `config/loon_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain`;
- `config/loon_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects`;
- `config/loon_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword`;
- `config/loon_rule_final_reject_fixture_maps_final_reject`;
- `config/loon_rule_final_reject_malformed_fixture_rejects_missing_action`;
- `config/loon_rule_url_regex_reject_fixture_maps_url_regex_rejects`;
- `config/loon_rule_url_regex_reject_malformed_fixture_rejects_invalid_regex`.

Unimplemented items:

- direct/proxy route selection;
- `IP-CIDR`, `GEOIP`, and broader Loon rule matchers;
- proxy groups, DNS/no-resolve, and rule providers;
- platform networking behavior.

### Loon Rule Route-Selection Guard

Detailed contract:
[Loon Rule Routing Guard Source Contract](loon-rule-routing-guard.md).

Capability: keep Loon route-selection and DNS-like `[Rule]` entries outside
the policy-core rewrite contract until adapter evidence exists.

Input form:

- `[Rule]` `DOMAIN-SUFFIX,<host>,DIRECT`;
- `[Rule]` `DOMAIN,<host>,PROXY`;
- `[Rule]` `IP-CIDR,<cidr>,DIRECT,no-resolve`;
- `[Rule]` `GEOIP,<country>,PROXY`;
- `[Rule]` `FINAL,DIRECT`.

Parser output:

- ignored diagnostics with section `Rule` and action `rule`;
- no request-phase rewrite reject or redirect rule;
- no script, task, argument, MITM, DNS, proxy-group, rule-provider, or platform
  route-selection state.

Current CI evidence:

- guard fixture `tests/fixtures/Loon.RuleRouteUnsupported.plugin`;
- `config/loon_rule_route_unsupported_fixture_keeps_routes_ignored`.

Unimplemented items:

- direct/proxy route selection;
- proxy groups and upstream selection;
- IP-CIDR, GEOIP, DNS/no-resolve, fallback, and rule-provider semantics;
- platform VPN, network-extension, and socket routing behavior.

### Quantumult X Common Subset

Detailed contract:
[Quantumult X Common Config Source Contract](quantumultx-common-config.md).

Capability: parse and evaluate the currently implemented Quantumult X subset.

Input form:

- `#[rewrite_local]`
- `#[rewrite_remote]`
- `#[mitm]`
- `url`-prefixed rewrite/script/header/body forms;
- Quantumult X `url echo-response content-type body` response-body forms;
- Quantumult X `url response-body-replace-regex pattern replacement`
  response-body forms;
- Quantumult X `url header-del header` request header forms;
- Quantumult X
  `url response-header-replace-regex header pattern replacement` response
  header forms;
- `#[task_local]` cron and event descriptor lines covered by
  [Quantumult X Task Metadata](quantumultx-task-metadata.md);
- `force-http-engine-hosts`;
- host-list `skip-server-cert-verify`.

Current CI evidence:

- positive fixture `tests/fixtures/QuantumultX.CommonConfig.snippet`;
- positive fixture `tests/fixtures/QuantumultX.EchoResponse.snippet`;
- positive fixture `tests/fixtures/QuantumultX.BodyMutation.snippet`;
- positive fixture `tests/fixtures/QuantumultX.HeaderMutation.snippet`;
- positive fixture `tests/fixtures/QuantumultX.HeaderDelete.snippet`;
- negative fixture `tests/fixtures/QuantumultX.CommonConfig.Malformed.snippet`;
- negative fixture `tests/fixtures/QuantumultX.EchoResponse.Malformed.snippet`;
- negative fixture `tests/fixtures/QuantumultX.BodyMutation.Malformed.snippet`;
- negative fixture `tests/fixtures/QuantumultX.HeaderMutation.Malformed.snippet`;
- negative fixture `tests/fixtures/QuantumultX.HeaderDelete.Malformed.snippet`;
- `config/quantumultx_common_config_fixture_is_supported`;
- `config/quantumultx_common_config_strict_fixture_rejects_malformed_rule`;
- `config/quantumultx_echo_response_fixture_maps_response_body`;
- `config/quantumultx_echo_response_malformed_fixture_rejects_missing_body`;
- `config/quantumultx_body_mutation_fixture_maps_response_body_regex`;
- `config/quantumultx_body_mutation_malformed_fixture_rejects_invalid_regex`;
- `config/quantumultx_header_mutation_fixture_maps_response_header_regex`;
- `config/quantumultx_header_mutation_malformed_fixture_rejects_invalid_regex`;
- `config/quantumultx_header_delete_fixture_maps_request_header_delete`;
- `config/quantumultx_header_delete_malformed_fixture_rejects_missing_header_name`;
- C parser/script/rewrite tests;
- runner corpus entry `Representative.QuantumultX.snippet`;
- dedicated task parser evidence in
  [Quantumult X Task Metadata Source Contract](quantumultx-task-metadata.md).

Unimplemented items:

- task scheduler/runtime behavior;
- event dispatch behavior;
- HTTP response serialization, content-type writeback, and body streaming;
- full rewrite/task grammar;
- broader corpus and migration notes.

### Quantumult X Task Metadata

Detailed contract:
[Quantumult X Task Metadata Source Contract](quantumultx-task-metadata.md).

Capability: parse Quantumult X `[task_local]` cron and event task descriptors
without claiming scheduler, event dispatch, or runtime execution.

Input form:

- `#[task_local]` or `[task_local]` sections;
- five-field or six-field cron expressions followed by a script path token;
- `event-network` and `event-interaction` trigger tokens followed by a script
  path token;
- comma-separated `tag`, `argument`, `timeout`, `timeout-ms`, `max-size`,
  `enable`, and `enabled` metadata.

Parser output:

- accepted diagnostics with section `Script` and action `task`;
- task kind, schedule, script path, tag, resolved argument, timeout, max-size,
  enabled state, and parser origin visible through
  `anixops_engine_copy_task_descriptor`;
- task descriptors remain separate from HTTP script URL dispatch;
- no scheduler, background execution, JavaScript task runtime, certificate,
  routing, or platform UI behavior from task metadata.

Current CI evidence:

- positive fixture `tests/fixtures/QuantumultX.TaskMetadata.snippet`;
- negative fixture `tests/fixtures/QuantumultX.TaskMetadata.Malformed.snippet`;
- negative fixture
  `tests/fixtures/QuantumultX.TaskMetadata.EventMalformed.snippet`;
- unsupported-event fixture
  `tests/fixtures/QuantumultX.TaskMetadata.UnsupportedEvent.snippet`;
- `config/quantumultx_task_metadata_fixture_emits_task_descriptors`;
- `config/quantumultx_task_metadata_event_malformed_fixture_rejects_missing_path`;
- `config/quantumultx_task_metadata_unsupported_event_fixture_stays_ignored`;
- `config/quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron`.

Unimplemented items:

- scheduler/runtime execution;
- event dispatch execution;
- task JavaScript bindings;
- concurrency and permission policy;
- broader Quantumult X task corpus.

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
  `disable_mitm_quic`;
- unsupported certificate-material and validation-bypass keys `passphrase`,
  `p12`, and `skip_validating_cert`.

Current CI evidence:

- positive fixture `tests/fixtures/QuantumultX.MitmOptions.snippet`;
- negative fixture `tests/fixtures/QuantumultX.MitmOptions.Malformed.snippet`;
- unsupported fixture
  `tests/fixtures/QuantumultX.MitmCertificateUnsupported.snippet`;
- `config/quantumultx_mitm_options_fixture_exposes_adapter_flags`;
- `config/quantumultx_mitm_options_malformed_fixture_rejects_invalid_host`;
- `config/quantumultx_mitm_certificate_unsupported_fixture_keeps_options_ignored`.

Unimplemented items:

- root CA installation or trust-store mutation;
- p12 material loading or passphrase handling;
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
- `[Script]` cron and interval task metadata covered by
  [Surge Task Metadata](surge-task-metadata.md);
- `[MITM] hostname`;
- unsupported certificate-material keys `ca-p12` and `ca-passphrase`;
- selected body/JQ rewrite forms.

Current CI evidence:

- positive fixture `tests/fixtures/Surge.CommonConfig.sgmodule`;
- negative fixture `tests/fixtures/Surge.CommonConfig.Malformed.sgmodule`;
- unsupported fixture `tests/fixtures/Surge.MitmCertificateUnsupported.sgmodule`;
- `config/surge_common_config_fixture_is_supported`;
- `config/surge_common_config_strict_fixture_rejects_malformed_rule`;
- `config/surge_mitm_certificate_unsupported_fixture_keeps_material_ignored`;
- C parser/script tests;
- runner corpus entry `Representative.Surge.sgmodule`;
- optional `JQ=1` GitHub Actions coverage when libjq headers are installed.

Unimplemented items:

- full Surge module grammar;
- certificate material loading and platform trust-store mutation;
- broader body rewrite and JQ corpus;
- requirement runtime gating behavior.

### Surge Task Metadata

Detailed contract:
[Surge Task Metadata Source Contract](surge-task-metadata.md).

Capability: parse Surge `[Script]` cron, interval, and event task descriptors
without claiming scheduler, event dispatch, or runtime execution.

Input form:

- `[Script]` attr-list rules with `type=cron`, `type=scheduled`,
  `type=scheduled-script`, `type=interval`, or `type=task`;
- `[Script]` attr-list rules with `type=event` and `event-name`;
- `cronexp`, `cron`, `schedule`, `interval`, `script-path`, `script_path`,
  `tag`, `argument`, `timeout`, `timeout-ms`, `max-size`, `max_size`,
  `enable`, and `enabled` metadata;
- inline `#!arguments` defaults used by task `argument` templates.

Parser output:

- accepted diagnostics with section `Script` and action `task`;
- task kind, schedule or interval seconds, script path, tag, resolved argument,
  timeout, max-size, enabled state, and parser origin visible through
  `anixops_engine_copy_task_descriptor`;
- task descriptors remain separate from HTTP request/response script dispatch;
- no scheduler, background execution, JavaScript task runtime, certificate,
  routing, DNS, proxy, or platform UI behavior from task metadata.

Current CI evidence:

- positive fixture `tests/fixtures/Surge.TaskMetadata.sgmodule`;
- negative fixture `tests/fixtures/Surge.TaskMetadata.Malformed.sgmodule`;
- positive fixture `tests/fixtures/Surge.TaskEvent.sgmodule`;
- negative fixture `tests/fixtures/Surge.TaskEvent.Malformed.sgmodule`;
- unsupported-name fixture `tests/fixtures/Surge.TaskEvent.Unsupported.sgmodule`;
- unsupported script-type fixture
  `tests/fixtures/Surge.ScriptUnsupported.sgmodule`;
- `config/surge_task_metadata_fixture_emits_task_descriptors`;
- `config/surge_task_metadata_malformed_fixture_rejects_invalid_cron`;
- `config/surge_task_event_fixture_emits_event_descriptors`;
- `config/surge_task_event_malformed_fixture_rejects_missing_event_name`;
- `config/surge_task_event_unsupported_fixture_rejects_unknown_event_name`;
- `config/surge_script_unsupported_fixture_keeps_non_task_types_ignored`.

Unimplemented items:

- scheduler/runtime execution;
- event dispatch execution;
- DNS, rule, and policy-group script task forms;
- task JavaScript bindings;
- concurrency and permission policy;
- broader Surge task corpus.

### Surge Requirement Metadata

Detailed contract:
[Surge Requirement Metadata Source Contract](surge-requirement-metadata.md).

Capability: tolerate Surge requirement metadata without claiming platform
requirement gating.

Input form:

- `#!requirement`;
- adjacent module metadata keys such as `#!name`, `#!desc`, `#!system`, and
  `#!arguments-desc`;
- unsupported requirement-like hashbang keys as comments;
- bare requirement-like lines outside supported sections as ignored parser
  input.

Current CI evidence:

- positive fixture `tests/fixtures/Surge.RequirementMetadata.sgmodule`;
- negative fixture
  `tests/fixtures/Surge.RequirementMetadata.Unsupported.sgmodule`;
- `config/surge_requirement_metadata_fixture_records_tolerated_keys`;
- `config/surge_requirement_metadata_unsupported_keys_are_not_claimed`.

Unimplemented items:

- requirement runtime gating;
- dependency download;
- platform UI or install behavior;
- broader Surge metadata corpus.

### Stash HTTP MITM Hosts

Detailed contract:
[Stash HTTP MITM Source Contract](stash-http-mitm.md).

Capability: parse Stash `http.mitm` host policy metadata without claiming full
Stash YAML profile support.

Input form:

- top-level YAML `http:` mapping;
- nested `mitm:` list;
- list scalar host patterns;
- exact, wildcard, deny, and `host:*` port-wildcard host patterns accepted by
  the policy core;
- unsupported certificate-material keys `ca` and `ca-passphrase`.

Parser output:

- accepted diagnostics with section `MITM` and action `mitm`;
- accepted diagnostics with section `MITM` and action `force-http-engine`;
- ignored diagnostics with section `MITM` and action `ca` or `ca-passphrase`;
- host patterns visible through `anixops_engine_mitm_pattern_count`;
- existing MITM evaluation semantics for exact, wildcard, deny, and normalized
  `host:*` inputs;
- `force-http-engine: true` visible as the existing QUIC fallback policy-core
  decision signal for matched trusted hosts;
- no rewrite, script, task, argument, route, proxy, DNS, certificate lifecycle,
  or platform UI behavior from Stash YAML input.

Current CI evidence:

- positive fixture `tests/fixtures/Stash.HttpMitm.yaml`;
- negative fixture `tests/fixtures/Stash.HttpMitm.Malformed.yaml`;
- unsupported port-specific fixture
  `tests/fixtures/Stash.HttpMitm.PortSpecificUnsupported.yaml`;
- unsupported certificate-material fixture
  `tests/fixtures/Stash.HttpMitmCertificateUnsupported.yaml`;
- positive option fixture `tests/fixtures/Stash.HttpForceHttpEngine.yaml`;
- negative option fixture
  `tests/fixtures/Stash.HttpForceHttpEngine.Malformed.yaml`;
- `config/stash_http_mitm_fixture_exposes_host_patterns`;
- `config/stash_http_mitm_malformed_fixture_rejects_invalid_host`;
- `config/stash_http_mitm_port_specific_fixture_stays_unsupported`;
- `config/stash_http_mitm_certificate_unsupported_fixture_keeps_material_ignored`;
- `config/stash_http_force_http_engine_fixture_exposes_quic_signal`;
- `config/stash_http_force_http_engine_malformed_fixture_rejects_invalid_bool`;
- `config/stash_migration_guard_fixture_stays_parser_unsupported` continues to
  guard unsupported Stash app-profile routing syntax.

Unimplemented items:

- full YAML parser behavior;
- port-specific MITM matching such as `host:443`;
- certificate material loading and platform trust-store mutation;
- transport-level HTTP engine behavior, transparent/expanded rewrite, script,
  cron, DNS, routing, proxy, and UI behavior;
- certificate lifecycle and platform trust handling.

### Stash URL Rewrite Request Subset

Detailed contract:
[Stash URL Rewrite Source Contract](stash-url-rewrite.md).

Capability: parse Stash `http.url-rewrite` request URL reject and 302/307
redirect policy intent without claiming full Stash YAML profile support.

Input form:

- top-level YAML `http:` mapping;
- nested `url-rewrite:` list;
- list scalar entries shaped as `pattern - reject`;
- `reject-NNN` and existing reject body variants already accepted by the
  policy-core rewrite parser;
- list scalar entries shaped as `pattern replacement 302`;
- list scalar entries shaped as `pattern replacement 307`.

Parser output:

- request-phase rewrite reject or redirect rules visible through
  `anixops_rewrite_evaluate_url`;
- accepted diagnostics with section `Rewrite` and action `url-rewrite`;
- ignored diagnostics for unsupported transparent or route-like `url-rewrite`
  entries;
- no script, task, MITM, argument, route, proxy, DNS, certificate lifecycle, or
  platform UI behavior from Stash `url-rewrite` input.

Current CI evidence:

- positive fixture `tests/fixtures/Stash.UrlRewrite.yaml`;
- negative fixture `tests/fixtures/Stash.UrlRewrite.Malformed.yaml`;
- positive redirect fixture `tests/fixtures/Stash.UrlRewriteRedirect.yaml`;
- negative redirect fixture
  `tests/fixtures/Stash.UrlRewriteRedirect.Malformed.yaml`;
- `config/stash_url_rewrite_fixture_maps_reject_subset`;
- `config/stash_url_rewrite_malformed_fixture_rejects_invalid_regex`;
- `config/stash_url_rewrite_redirect_fixture_maps_redirect_subset`;
- `config/stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex`;
- `config/stash_migration_guard_fixture_stays_parser_unsupported` continues to
  guard unsupported Stash app-profile routing/proxy syntax.

Unimplemented items:

- transparent URL rewrites and redirect status codes outside 302/307;
- `DIRECT`, `PROXY`, proxy groups, route policy names, and rule providers;
- DNS, TUN, VPN, packet-capture, app-profile UI, proxy-node parsing, and
  platform networking behavior.

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
- `[MITM] h2 = ...`;
- unsupported certificate-material keys `ca-p12`, `ca-passphrase`, and
  `ca-cert`.

Current CI evidence:

- positive fixture `tests/fixtures/Shadowrocket.CommonConfig.conf`;
- negative fixture `tests/fixtures/Shadowrocket.CommonConfig.Malformed.conf`;
- unsupported certificate-material fixture
  `tests/fixtures/Shadowrocket.MitmCertificateUnsupported.conf`;
- `config/shadowrocket_common_config_fixture_is_supported`;
- `config/shadowrocket_common_config_fixture_rejects_invalid_regex`;
- `config/shadowrocket_mitm_certificate_unsupported_fixture_keeps_material_ignored`;
- app-profile guard fixture `tests/fixtures/Shadowrocket.MigrationGuard.conf`
  remains unsupported syntax evidence.

Unimplemented items:

- full Shadowrocket app-level profile grammar beyond the dedicated rule-reject
  subset;
- proxy nodes, routing policy, DNS, VPN, packet-capture, or UI behavior;
- certificate lifecycle and certificate material loading;
- runtime and scheduler behavior.

### Shadowrocket Rule Reject Subset

Detailed contract:
[Shadowrocket Rule Reject Source Contract](shadowrocket-rule-reject.md).

Capability: parse Shadowrocket `[Rule]` URL-regex, exact-domain,
domain-keyword, domain-suffix, and final fallback reject policy intent.

Input form:

- `[Rule]` `URL-REGEX,<pattern>,REJECT`;
- `[Rule]` `URL-REGEX,<pattern>,REJECT-NNN` and other common reject variants
  already supported by the policy-core rewrite parser;
- `[Rule]` `DOMAIN,<host>,REJECT`;
- `[Rule]` `DOMAIN,<host>,REJECT-NNN` and other common reject variants already
  supported by the policy-core rewrite parser;
- `[Rule]` `DOMAIN-KEYWORD,<keyword>,REJECT`;
- `[Rule]` `DOMAIN-KEYWORD,<keyword>,REJECT-NNN` and other common reject
  variants already supported by the policy-core rewrite parser;
- `[Rule]` `DOMAIN-SUFFIX,<host>,REJECT`;
- `[Rule]` `DOMAIN-SUFFIX,<host>,REJECT-NNN` and other common reject variants
  already supported by the policy-core rewrite parser;
- `[Rule]` `FINAL,REJECT`;
- `[Rule]` `FINAL,REJECT-NNN` and other common reject variants already
  supported by the policy-core rewrite parser.

Parser output:

- request-phase rewrite reject rules visible through
  `anixops_rewrite_evaluate_url`;
- accepted diagnostics with section `Rule` and action `rule`;
- ignored diagnostics for unsupported direct/proxy route-selection lines;
- no proxy-node, DNS, route, MITM, script, task, argument, UI, or network IO
  behavior from Shadowrocket `[Rule]` input.

Current CI evidence:

- positive fixture `tests/fixtures/Shadowrocket.RuleReject.conf`;
- negative fixture `tests/fixtures/Shadowrocket.RuleReject.Malformed.conf`;
- positive fixture `tests/fixtures/Shadowrocket.RuleDomainExactReject.conf`;
- negative fixture
  `tests/fixtures/Shadowrocket.RuleDomainExactReject.Malformed.conf`;
- positive fixture `tests/fixtures/Shadowrocket.RuleDomainKeywordReject.conf`;
- negative fixture
  `tests/fixtures/Shadowrocket.RuleDomainKeywordReject.Malformed.conf`;
- positive fixture `tests/fixtures/Shadowrocket.RuleDomainReject.conf`;
- negative fixture
  `tests/fixtures/Shadowrocket.RuleDomainReject.Malformed.conf`;
- positive fixture `tests/fixtures/Shadowrocket.RuleFinalReject.conf`;
- negative fixture `tests/fixtures/Shadowrocket.RuleFinalReject.Malformed.conf`;
- `config/shadowrocket_rule_reject_fixture_maps_url_regex_rejects`;
- `config/shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex`;
- `config/shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects`;
- `config/shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain`;
- `config/shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects`;
- `config/shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword`;
- `config/shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects`;
- `config/shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain`;
- `config/shadowrocket_rule_final_reject_fixture_maps_final_reject`;
- `config/shadowrocket_rule_final_reject_malformed_fixture_rejects_missing_action`;
- `config/shadowrocket_migration_guard_fixture_stays_parser_unsupported`
  continues to guard unsupported app-profile route/proxy syntax.

Unimplemented items:

- `DIRECT`, `PROXY`, proxy groups, and route policy names;
- unsupported matchers such as `IP-CIDR` and `GEOIP`;
- `no-resolve`, DNS behavior, app-profile UI, proxy-node parsing, and platform
  networking behavior.

### Stash Migration And Shadowrocket App-Profile Boundary

Detailed notes:
[Stash And Shadowrocket Migration Notes](stash-shadowrocket-migration.md).

Capability: record migration guidance for Stash and for Shadowrocket syntax
outside currently parser-supported subsets.

Input form:

- `tests/fixtures/Stash.MigrationGuard.yaml` as a non-support guard;
- `tests/fixtures/Shadowrocket.MigrationGuard.conf` as a non-support guard;
- Stash `http.mitm` host metadata is covered separately by
  [Stash HTTP MITM Source Contract](stash-http-mitm.md);
- Stash `http.url-rewrite` request URL policy intent is covered separately by
  [Stash URL Rewrite Source Contract](stash-url-rewrite.md);
- Shadowrocket `[Rule]` URL-regex, exact-domain, domain-keyword, domain-suffix,
  and final fallback reject policy intent is covered separately by
  [Shadowrocket Rule Reject Source Contract](shadowrocket-rule-reject.md);
- Shadowrocket app-level profile sections remain unsupported unless they are
  explicitly covered by `shadowrocket-common-config.md` or
  `shadowrocket-rule-reject.md`.

Current CI evidence:

- Stash app-profile routing/proxy compatibility remains `planned`;
- Stash `http.mitm` compatibility matrix row is `partial`;
- Stash URL rewrite request matrix row is `partial`;
- Shadowrocket common-config matrix row is `partial`;
- Shadowrocket rule-reject matrix row is `partial`;
- `config/stash_migration_guard_fixture_stays_parser_unsupported`;
- `config/stash_http_mitm_fixture_exposes_host_patterns`;
- `config/stash_http_mitm_malformed_fixture_rejects_invalid_host`;
- `config/stash_url_rewrite_fixture_maps_reject_subset`;
- `config/stash_url_rewrite_malformed_fixture_rejects_invalid_regex`;
- `config/stash_url_rewrite_redirect_fixture_maps_redirect_subset`;
- `config/stash_url_rewrite_redirect_malformed_fixture_rejects_invalid_regex`;
- `config/shadowrocket_rule_reject_fixture_maps_url_regex_rejects`;
- `config/shadowrocket_rule_reject_malformed_fixture_rejects_invalid_regex`;
- `config/shadowrocket_rule_domain_exact_reject_fixture_maps_exact_domain_rejects`;
- `config/shadowrocket_rule_domain_exact_reject_malformed_fixture_rejects_invalid_domain`;
- `config/shadowrocket_rule_domain_keyword_reject_fixture_maps_domain_keyword_rejects`;
- `config/shadowrocket_rule_domain_keyword_reject_malformed_fixture_rejects_invalid_keyword`;
- `config/shadowrocket_rule_domain_reject_fixture_maps_domain_suffix_rejects`;
- `config/shadowrocket_rule_domain_reject_malformed_fixture_rejects_invalid_domain`;
- `config/shadowrocket_rule_final_reject_fixture_maps_final_reject`;
- `config/shadowrocket_rule_final_reject_malformed_fixture_rejects_missing_action`;
- `config/shadowrocket_migration_guard_fixture_stays_parser_unsupported`;
- governance checks require migration and app-profile boundary markers plus
  guard fixture/test links.

Unimplemented items:

- expanded Stash app-level profile source contracts beyond URL rewrite request,
  parser fixtures, positive tests, and negative tests;
- expanded Shadowrocket app-level profile source contracts beyond rule reject,
  parser fixtures, positive tests, and negative tests.

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
- positive fixture `tests/fixtures/QuantumultX.HeaderMutation.snippet`;
- positive fixture `tests/fixtures/QuantumultX.HeaderDelete.snippet`;
- negative fixture `tests/fixtures/HeaderMutation.Common.Malformed.conf`;
- negative fixture `tests/fixtures/QuantumultX.HeaderMutation.Malformed.snippet`;
- negative fixture `tests/fixtures/QuantumultX.HeaderDelete.Malformed.snippet`;
- `config/header_mutation_common_fixture_is_supported`;
- `config/header_mutation_common_fixture_rejects_invalid_regex`;
- `config/quantumultx_header_mutation_fixture_maps_response_header_regex`;
- `config/quantumultx_header_mutation_malformed_fixture_rejects_invalid_regex`;
- `config/quantumultx_header_delete_fixture_maps_request_header_delete`;
- `config/quantumultx_header_delete_malformed_fixture_rejects_missing_header_name`;
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
- positive fixture `tests/fixtures/QuantumultX.BodyMutation.snippet`;
- negative fixture `tests/fixtures/BodyMutation.Common.Malformed.conf`;
- negative fixture `tests/fixtures/QuantumultX.BodyMutation.Malformed.snippet`;
- `config/body_mutation_common_fixture_is_supported`;
- `config/body_mutation_common_fixture_rejects_invalid_body_regex`;
- `config/quantumultx_body_mutation_fixture_maps_response_body_regex`;
- `config/quantumultx_body_mutation_malformed_fixture_rejects_invalid_regex`;
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

Capability: parse cron, interval, Quantumult X event, and Surge event task
trigger metadata without claiming scheduler/runtime support.

Input form:

- direct `[Script]` cron task declarations;
- `[Script]` attr-list rules with `type=cron`, `type=interval`, or
  `type=task` plus a concrete cron expression or interval;
- Quantumult X `event-network` and `event-interaction` descriptor lines;
- Surge `type=event` descriptor lines for `network-changed` and
  `notification`;
- `script-path`, `tag`, `argument`, `timeout`, `max-size`, and `enabled`
  metadata;
- positive parser fixture `tests/fixtures/CronTaskTrigger.HttpScriptGuard.conf`;
- unsupported parser fixture `tests/fixtures/CronTaskTrigger.Unsupported.conf`;
- Loon unsupported parser fixture
  `tests/fixtures/Loon.TaskUnsupported.plugin`;
- malformed parser fixture `tests/fixtures/CronTaskTrigger.Malformed.conf`;
- ecosystem-specific parser fixtures including
  `tests/fixtures/QuantumultX.TaskMetadata.snippet`,
  `tests/fixtures/QuantumultX.TaskMetadata.UnsupportedEvent.snippet`,
  `tests/fixtures/Surge.TaskEvent.sgmodule`,
  `tests/fixtures/Surge.TaskEvent.Unsupported.sgmodule`, and
  `tests/fixtures/Surge.TaskMetadata.sgmodule`.

Current CI evidence:

- `config/cron_task_trigger_common_fixture_emits_task_descriptors` proves HTTP
  script triggers stay separate from cron and interval task descriptors;
- `config/cron_task_trigger_unsupported_fixture_does_not_register_descriptors`
  proves unsupported scheduler-like lines do not register as HTTP scripts or
  task descriptors;
- `config/cron_task_trigger_malformed_fixture_rejects_invalid_cron` proves
  malformed cron descriptors are rejected;
- `config/loon_task_unsupported_fixture_keeps_non_task_types_ignored` proves
  unsupported Loon `[Script]` task-like types do not register as HTTP scripts or
  task descriptors;
- `script/malformed_and_non_http_script_rules_are_ignored_or_rejected` proves a
  bare cron rule does not register as an HTTP script rule when using the HTTP
  script parser directly;
- `config/surge_task_metadata_fixture_emits_task_descriptors` and
  `config/surge_task_metadata_malformed_fixture_rejects_invalid_cron` prove the
  documented Surge `type=cron` shape is covered;
- `config/surge_task_event_fixture_emits_event_descriptors` and
  `config/surge_task_event_malformed_fixture_rejects_missing_event_name` prove
  the documented Surge `type=event` descriptor shape is covered;
- public ABI functions `anixops_engine_task_descriptor_count` and
  `anixops_engine_copy_task_descriptor`;
- GitHub Actions governance requires the contract and matrix row.

Unimplemented items:

- scheduler/runtime replay or E2E evidence;
- Quantumult X event dispatch runtime evidence;
- Surge event dispatch runtime evidence;
- task JavaScript bindings and concurrency policy.
