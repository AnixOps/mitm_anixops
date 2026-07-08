# Cron And Task Trigger Source Contract

Capability: cron and scheduled task trigger metadata.

Ecosystem: `loon`, `quantumultx`, `surge`, `portable`.

Status: `planned`.

## Purpose

This contract fixes the P3 boundary for cron and task trigger compatibility. It
does not implement a scheduler, parser fixtures, or JavaScript task runtime.
Until those exist, cron/task capability must remain `planned` and must not be
described as `partial` or `supported`.

## Input Forms

Future parser work must evaluate representative forms from the common
ecosystems before this row can move out of `planned`:

- Quantumult X task and cron forms from task sections or task-like URL
  prefixes.
- Surge module scheduled script forms, when represented as script metadata
  rather than HTTP request/response hooks.
- Loon or AnixOps-style scheduled script/task declarations if they are used by
  the supported plugin corpus.

No cron/task parser fixture is currently accepted as a supported capability.

## Parser Output

Future implementation must produce an adapter-owned task descriptor instead of
an HTTP request/response script rule. The descriptor must separate:

- task kind, such as cron expression, interval, event, or manual task;
- script path or local asset identity;
- resolved argument string;
- tag/name;
- timeout and max-size limits when present;
- disabled/enabled state;
- ecosystem/profile origin.

Current parser behavior must not register bare cron/task forms as HTTP script
rules. Existing script evaluation APIs only return HTTP request/response script
dispatch metadata.

## Positive Case

No positive parser fixture exists yet.

Before this capability can become `partial`, the same change must add:

- at least one representative cron/task parser fixture;
- one positive test proving the fixture emits a task descriptor;
- a runner or adapter contract showing how the task is scheduled and executed;
- a matrix update that names that evidence.

## Negative Case

Existing CI-covered guard:

```text
script/malformed_and_non_http_script_rules_are_ignored_or_rejected
```

That test includes a bare `cron "0 * * * *" script-path=...` rule and verifies
it does not register an HTTP script rule. This is only a non-support guard; it
does not prove cron/task compatibility.

Future negative tests must cover malformed cron expressions, missing script
assets, disabled tasks, duplicate tags, unsupported scheduler types, and
runtime timeout/error behavior.

## Runtime And Matching Behavior

Cron/task execution is scheduler-owned and must not be routed through
`anixops_script_evaluate_url`. A future adapter contract must define:

- how scheduled tasks are discovered;
- when they run;
- how `$request`, `$response`, `$argument`, `$persistentStore`, and `$done` are
  exposed or intentionally omitted;
- how timeout, exception, double `$done`, and cancellation are handled;
- whether concurrent runs of the same task are skipped, queued, or allowed;
- what result, if any, is written back to a caller.

## Diagnostics

Future task descriptors must expose diagnostics for at least:

- accepted task rule;
- ignored unsupported task form;
- rejected malformed cron expression;
- missing script path;
- disabled task;
- scheduler/runtime not configured;
- timeout or exception fail-open.

## Security Boundary

Task execution must be conservative by default:

- no automatic network access policy is implied by this contract;
- no credentials, sensitive headers, or body payloads should be logged;
- no background task may bypass user or platform permission requirements;
- no root certificate trust, TLS interception, or hidden traffic capture is
  implied by scheduled task support.

## CI Evidence

Current evidence:

- GitHub Actions `governance` requires this planned source contract and matrix
  row.
- GitHub Actions `linux-test` runs `sh scripts/check.sh`.
- `tests/test_script.c` contains
  `script/malformed_and_non_http_script_rules_are_ignored_or_rejected`, which
  guards against treating bare cron rules as supported HTTP scripts.

Missing evidence:

- positive cron/task parser fixture;
- negative malformed cron/task parser fixture;
- task descriptor public API;
- scheduler/runtime replay or E2E test;
- adapter compatibility note for each ecosystem.

## Compatibility Matrix Row

The matrix row is `cron/task trigger` in
[matrix.md](matrix.md).

## Unimplemented Items

- parser fixtures for Loon, Quantumult X, and Surge task forms;
- public task descriptor API;
- scheduler and execution adapter;
- task JavaScript runtime bindings;
- persistence/locking policy for scheduled runs;
- runtime cancellation and quota enforcement;
- release evidence proving task behavior through GitHub Actions.
