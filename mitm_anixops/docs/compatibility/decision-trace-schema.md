# Decision Trace Schema Source Contract

Capability: structured policy decision trace.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`.

Status: `partial`.

## Purpose

This contract defines the v1.0.0 source of truth for policy-core trace fields.
The trace schema is the stable shape adapters can use to audit MITM hostname,
URL rewrite, reject policy intent, header mutation, body mutation, and script
trigger decisions without logging sensitive payloads by default.

The current contract is built from the public C ABI and the runner `trace`
output. It does not introduce a new serialized ABI object yet.

## Schema Sources

Host/MITM decision:

- `anixops_mitm_decision_t.decision`
- `anixops_mitm_decision_t.reason`
- `anixops_mitm_decision_t.matched_pattern`
- `anixops_mitm_decision_t.message`

Plan envelope:

- `anixops_rewrite_plan_t.phase`
- `anixops_rewrite_plan_t.body_available`
- `anixops_rewrite_plan_t.requires_body`
- `anixops_rewrite_plan_t.rewrite`
- `anixops_rewrite_plan_t.header_rewrite_count`
- `anixops_rewrite_plan_t.header_rewrite_truncated`
- `anixops_rewrite_plan_t.header_rewrites`
- `anixops_rewrite_plan_t.script`

URL rewrite and reject policy intent:

- `anixops_rewrite_result_t.action`
- `anixops_rewrite_result_t.status_code`
- `anixops_rewrite_result_t.rule_index`
- `anixops_rewrite_result_t.matched_pattern`
- `anixops_rewrite_result_t.value`
- `anixops_rewrite_result_t.message`

Header mutation:

- `anixops_header_rewrite_result_t.action`
- `anixops_header_rewrite_result_t.phase`
- `anixops_header_rewrite_result_t.rule_index`
- `anixops_header_rewrite_result_t.matched_pattern`
- `anixops_header_rewrite_result_t.header_name`
- `anixops_header_rewrite_result_t.value`
- `anixops_header_rewrite_result_t.message`

Body mutation:

- `anixops_rewrite_result_t` for the matched body rule;
- adapter-owned body output buffer;
- `body_available` and `requires_body` on the plan envelope.

Script trigger:

- `anixops_script_result_t.kind`
- `anixops_script_result_t.phase`
- `anixops_script_result_t.requires_body`
- `anixops_script_result_t.timeout_ms`
- `anixops_script_result_t.max_size`
- `anixops_script_result_t.rule_index`
- `anixops_script_result_t.matched_pattern`
- `anixops_script_result_t.script_path`
- `anixops_script_result_t.tag`
- `anixops_script_result_t.argument`
- `anixops_script_result_t.message`

Runner JSON `trace` currently serializes:

- `version`
- `loadStatus`
- `url`
- `phase`
- `planStatus`
- `requiresBody`
- `rewrite`
- `headers`
- `headersTruncated`
- `script`

## Positive Case

Parser case:

```text
tests/fixtures/DecisionTrace.Schema.conf
```

Expected behavior:

- config load succeeds;
- one MITM hostname is registered;
- redirect and reject URL policy decisions expose action, status, rule index,
  matched pattern, value, and message fields;
- header mutation appears in the plan header list;
- body mutation appears in the plan rewrite slot and reports that body input was
  available;
- script trigger appears in the plan script slot with timeout-ms, max-size, tag,
  argument, and body requirement fields.

## Negative Case

Parser case:

```text
tests/fixtures/DecisionTrace.Schema.UnsupportedPolicy.conf
```

Expected behavior:

- unsupported `direct` and `proxy` policy-intent lines are ignored, not routed;
- ignored diagnostics are recorded for both lines;
- no rewrite rules are registered for unsupported route selection.

## Runtime And Security Boundary

This contract covers policy-core decision fields only.

Adapters must not log raw sensitive header or body values by default. Payload
logging, redaction, retention, sampling, and export are adapter-owned privacy
decisions.

This contract does not implement:

- automatic root certificate trust;
- TLS interception;
- production network I/O;
- direct/proxy route selection;
- full serialized JSON trace object for every ABI decision;
- platform redaction policy.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/decision_trace_schema_fixture_covers_policy_fields`;
- `tests/test_config.c` registers
  `config/decision_trace_schema_fixture_ignores_unsupported_policy_intent`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
decision trace schema
```

The row remains `partial` until golden JSON trace fixtures, full runner MITM
trace coverage, adapter redaction policy, and direct/proxy route-selection
contracts exist.
