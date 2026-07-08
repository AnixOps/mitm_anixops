# Cron And Task Trigger Source Contract

Capability: cron and scheduled task trigger metadata.

Ecosystem: `loon`, `quantumultx`, `surge`, `portable`.

Status: `partial`.

## Purpose

This contract fixes the P3 boundary for cron and task trigger compatibility.
The policy core now parses a narrow task descriptor subset, but it still does
not implement a scheduler, JavaScript task runtime, background execution, or
platform permission flow.

## Input Forms

The current task descriptor subset accepts task-like declarations in `[Script]`
sections after HTTP request/response script triggers have had the first chance
to parse:

- `cron "0 * * * *" script-path=..., tag=...`;
- attr-list rules with `type=cron` and `cronexp=...`;
- attr-list rules with `type=interval` and `interval=...`;
- attr-list `type=task` when `cronexp` or `interval` identifies the concrete
  scheduler kind;
- Quantumult X `[task_local]` `event-network` and `event-interaction`
  descriptor lines;
- Surge `[Script]` `type=event` descriptor lines for `network-changed` and
  `notification`;
- `script-path` or `script_path`;
- `tag`, `argument`, `timeout`, `timeout-ms`, `max-size`, `max_size`,
  `enable`, and `enabled` metadata.

The parser validates that cron expressions have five or six fields and that
interval values are positive integer seconds. It records unsupported scheduler
types as ignored diagnostics in the portable profile.

## Parser Output

The parser produces adapter-owned task descriptors through:

- `anixops_engine_task_descriptor_count`;
- `anixops_engine_copy_task_descriptor`.

Descriptors separate:

- task kind: cron, interval, manual placeholder, Quantumult X event descriptor,
  or Surge event descriptor;
- cron expression or interval seconds;
- script path;
- resolved argument string;
- tag/name;
- timeout and max-size limits when present;
- disabled/enabled state;
- parser origin.

Task descriptors are separate from HTTP request/response script rules and are
not returned by `anixops_script_evaluate_url`.

## Positive Case

Parser case:

```text
tests/fixtures/CronTaskTrigger.HttpScriptGuard.conf
tests/fixtures/Loon.TaskMetadata.plugin
tests/fixtures/QuantumultX.TaskMetadata.snippet
tests/fixtures/Surge.TaskEvent.sgmodule
tests/fixtures/Surge.TaskMetadata.sgmodule
```

Expected behavior:

- config load succeeds;
- one HTTP request script trigger is registered;
- three task descriptors are registered;
- the HTTP request script trigger remains matchable by
  `anixops_script_evaluate_url`;
- cron and interval task descriptors are observable through the public ABI;
- the Loon parser fixture exposes the same descriptor ABI while preserving HTTP
  script URL dispatch separation;
- the Quantumult X parser fixture exposes `[task_local]` cron and event
  descriptors while preserving HTTP script URL dispatch separation;
- the Surge parser fixture exposes `[Script]` `type=cron` and `type=interval`
  task descriptors while preserving HTTP script URL dispatch separation;
- the Surge event parser fixture exposes `[Script]` `type=event` descriptors
  while preserving HTTP script URL dispatch separation;
- response-phase URL script evaluation does not return a task descriptor.

## Unsupported Case

Parser case:

```text
tests/fixtures/CronTaskTrigger.Unsupported.conf
```

Expected behavior:

- config load succeeds in the portable profile;
- unsupported scheduler-like lines are ignored with diagnostics;
- no HTTP script rules are registered;
- no task descriptors are registered.

## Negative Case

Parser case:

```text
tests/fixtures/CronTaskTrigger.Malformed.conf
tests/fixtures/Loon.TaskMetadata.Malformed.plugin
tests/fixtures/QuantumultX.TaskMetadata.Malformed.snippet
tests/fixtures/QuantumultX.TaskMetadata.EventMalformed.snippet
tests/fixtures/Surge.TaskEvent.Malformed.sgmodule
tests/fixtures/Surge.TaskEvent.Unsupported.sgmodule
tests/fixtures/Surge.TaskMetadata.Malformed.sgmodule
```

Expected behavior:

- config load fails with `ANIXOPS_ERR_PARSE`;
- a rejected rule diagnostic is recorded with section `Script` and action
  `task`;
- last error reports cron expression, event script path, missing event name, or
  unsupported event name failure at the malformed line.

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

Until that adapter evidence exists, cron/task trigger support remains parser
metadata only.

## Diagnostics

Current diagnostics cover:

- accepted HTTP script rule beside task descriptors;
- accepted cron task descriptor;
- accepted interval task descriptor;
- accepted Quantumult X event task descriptor;
- accepted Surge event task descriptor;
- ignored unsupported scheduler type;
- rejected malformed cron expression;
- rejected malformed event task missing a script path;
- rejected Surge event task with an unknown event name.

Future diagnostics must cover missing script assets, disabled task scheduling
policy, scheduler/runtime not configured, timeout behavior, exception behavior,
and concurrency policy.

## Security Boundary

Task execution must be conservative by default:

- no automatic network access policy is implied by this contract;
- no credentials, sensitive headers, or body payloads should be logged;
- no background task may bypass user or platform permission requirements;
- no root certificate trust, TLS interception, or hidden traffic capture is
  implied by scheduled task support.

## CI Evidence

Current evidence:

- GitHub Actions `governance` requires this source contract and matrix row.
- GitHub Actions `linux-test` runs `sh scripts/check.sh`.
- `tests/test_config.c` registers
  `config/cron_task_trigger_common_fixture_emits_task_descriptors`;
- `tests/test_config.c` registers
  `config/cron_task_trigger_unsupported_fixture_does_not_register_descriptors`;
- `tests/test_config.c` registers
  `config/cron_task_trigger_malformed_fixture_rejects_invalid_cron`;
- `tests/test_config.c` registers
  `config/loon_task_metadata_fixture_emits_task_descriptors`;
- `tests/test_config.c` registers
  `config/loon_task_metadata_malformed_fixture_rejects_invalid_cron`;
- `tests/test_config.c` registers
  `config/quantumultx_task_metadata_fixture_emits_task_descriptors`;
- `tests/test_config.c` registers
  `config/quantumultx_task_metadata_event_malformed_fixture_rejects_missing_path`;
- `tests/test_config.c` registers
  `config/quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron`;
- `tests/test_config.c` registers
  `config/surge_task_metadata_fixture_emits_task_descriptors`;
- `tests/test_config.c` registers
  `config/surge_task_metadata_malformed_fixture_rejects_invalid_cron`;
- `tests/test_config.c` registers
  `config/surge_task_event_fixture_emits_event_descriptors`;
- `tests/test_config.c` registers
  `config/surge_task_event_malformed_fixture_rejects_missing_event_name`;
- `tests/test_config.c` registers
  `config/surge_task_event_unsupported_fixture_rejects_unknown_event_name`;
- `tests/test_script.c` contains
  `script/malformed_and_non_http_script_rules_are_ignored_or_rejected`, which
  guards against treating bare cron rules as supported HTTP scripts when using
  the HTTP script parser directly.

Missing evidence:

- scheduler/runtime replay or E2E test;
- Quantumult X event dispatch runtime evidence;
- Surge event dispatch runtime evidence;
- task JavaScript runtime bindings;
- adapter compatibility notes beyond the current Loon, Quantumult X, and Surge
  parser boundaries;
- permission, concurrency, and cancellation policy.

## Compatibility Matrix Row

The matrix row is `cron/task trigger` in
[matrix.md](matrix.md).

The row remains `partial` because parser metadata and ABI exposure are covered,
while scheduling and task runtime behavior remain unimplemented.

## Unimplemented Items

- scheduler and execution adapter;
- Quantumult X event dispatch adapter;
- Surge event dispatch adapter;
- task JavaScript runtime bindings;
- persistence/locking policy for scheduled runs;
- runtime cancellation and quota enforcement;
- broader ecosystem task corpus;
- release evidence proving task runtime behavior through GitHub Actions.
