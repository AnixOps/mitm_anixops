# Plan API Parity Source Contract

Capability: plan API and legacy evaluation API parity.

Ecosystem: `portable`, `loon`, `quantumultx`, `surge`.

Status: `partial`.

## Purpose

This contract defines the v1.0.0 source of truth for `anixops_rewrite_build_plan`
parity. The plan API aggregates URL/body rewrite, header mutation, and script
trigger decisions for one phase. For the same config, URL, phase, body, and
header context, each field exposed by the plan must match the corresponding
legacy `evaluate_*` or `apply_*` API.

## Input Forms

The current parity subset accepts:

- request URL redirect rules;
- request and response header mutation rules;
- request and response current-header regex mutation rules;
- request and response body regex replacement rules;
- request and response script trigger metadata;
- request and response phase inputs.

## Positive Case

Parser case:

```text
tests/fixtures/PlanApiParity.Golden.conf
```

Expected behavior:

- config load succeeds;
- URL-only plans match `anixops_rewrite_evaluate_url`;
- body-aware request plans match `anixops_rewrite_apply_body`,
  `anixops_rewrite_evaluate_header`, and `anixops_script_evaluate_url`;
- body-aware response plans match the same legacy APIs for response phase;
- current-header request and response plans match
  `anixops_rewrite_evaluate_header` with the same current header value;
- Go and Rust wrappers expose explicit current-header plan helpers while
  preserving the default `BuildPlan` behavior;
- plan `requires_body`, `body_available`, header count, truncation flag,
  rule indexes, matched patterns, values, tags, timeout, max-size, and messages
  are stable.

## Negative Case

Parser case:

```text
tests/fixtures/PlanApiParity.PhaseMismatch.conf
```

Expected behavior:

- request-only rules do not appear in response-phase plans;
- response-only rules do not appear in request-phase plans;
- legacy APIs and plan API both return no matched rewrite, header, or script for
  the wrong phase;
- input body is preserved when no body rewrite matches.

## Runtime And Security Boundary

This contract covers policy-core aggregation only.

It does not implement:

- production HTTP request/response serialization;
- platform header map mutation;
- streaming body mutation;
- script runtime execution;
- direct/proxy routing;
- adapter-owned redaction or trace export.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/plan_api_parity_fixture_matches_legacy_evaluation`;
- `tests/test_config.c` registers
  `config/plan_api_parity_fixture_keeps_phase_mismatches_empty`;
- Go wrapper `BuildPlanWithCurrentHeader` is covered by
  `TestGoBindingLoadsSharedParityFixture`;
- Rust wrapper `build_plan_with_current_header` is covered by
  `rust_binding_loads_shared_parity_fixture`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
plan API parity
```

The row remains `partial` until runner golden JSON traces and adapter ordering
coverage exist.
