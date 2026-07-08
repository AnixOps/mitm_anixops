# Quantumult X Task Metadata Source Contract

Capability: Quantumult X `[task_local]` cron task metadata.

Ecosystem: `quantumultx`.

Status: `partial`.

## Purpose

This contract fixes the Quantumult X parser evidence for local cron task
metadata. It follows the `[task_local]` shape shown in the Quantumult X sample
configuration, but the policy core only exposes task descriptors to adapters.
It does not schedule tasks, execute JavaScript, or handle event-triggered
platform tasks.

Reference sample:
<https://github.com/crossutility/Quantumult-X/blob/master/sample.conf>

## Input Forms

The parser accepts cron task lines inside `[task_local]` or `#[task_local]`:

```text
#[task_local]
*/15 * * * * https://scripts.example/task.js, tag=qx.task.cron
0 30 8 * * * https://scripts.example/task-six-field.js, tag=qx.task.six
```

Supported fields:

- five-field and six-field cron expressions;
- local or remote script path token after the cron expression;
- `tag`;
- `argument`;
- `timeout` seconds and `timeout-ms`/`timeout_ms` milliseconds;
- `max-size` and `max_size`;
- `enable` and `enabled`.

Unsupported trigger forms such as `event-network` and `event-interaction` stay
ignored in the portable profile and must not register task descriptors.

## Parser Output

For valid cron lines, the parser must:

- register one task descriptor per supported cron line;
- emit an accepted diagnostic with section `Script`, action `task`, and message
  `task descriptor accepted`;
- expose cron schedule, script path, tag, resolved argument, timeout, max-size,
  enabled state, and parser origin through
  `anixops_engine_copy_task_descriptor`;
- keep task descriptors separate from HTTP request/response script URL
  dispatch.

Malformed cron lines are rejected when the parser recognizes a cron-like task
line but cannot validate enough cron fields or a script path.

## Positive Case

Parser case:

```text
tests/fixtures/QuantumultX.TaskMetadata.snippet
```

Expected behavior:

- config load succeeds;
- two cron task descriptors are registered;
- no HTTP script URL dispatch rules are registered by task lines;
- an unsupported event-triggered task line is ignored in the portable profile;
- task descriptors expose scheduling metadata through the public ABI.

## Negative Case

Parser case:

```text
tests/fixtures/QuantumultX.TaskMetadata.Malformed.snippet
```

Expected behavior:

- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects the malformed cron task line;
- no script rules or task descriptors are registered after the failure;
- a rejected diagnostic is recorded with section `Script`, action `task`, and
  cron expression failure context.

## Runtime And Security Boundary

Quantumult X task metadata must not:

- execute JavaScript inside the C policy core;
- create a scheduler or background worker;
- download, cache, or validate remote script assets;
- implement `event-network`, `event-interaction`, or other platform event
  triggers;
- bypass timeout, max-size, concurrency, or user permission policy owned by the
  adapter/runtime;
- alter MITM, certificate, routing, or trust behavior.

Scheduler/runtime execution is covered by
[Cron And Task Trigger Source Contract](cron-task-trigger.md) and remains
adapter-owned until GitHub Actions evidence exists for a concrete runtime.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/quantumultx_task_metadata_fixture_emits_task_descriptors`;
- `tests/test_config.c` registers
  `config/quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Quantumult X task metadata
```

The row remains `partial` because cron task descriptors are covered while
event triggers, scheduler execution, task JavaScript bindings, concurrency, and
permission policy remain adapter-owned.
