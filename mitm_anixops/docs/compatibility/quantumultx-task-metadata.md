# Quantumult X Task Metadata Source Contract

Capability: Quantumult X `[task_local]` cron task metadata.

Ecosystem: `quantumultx`.

Status: `partial`.

## Purpose

This contract fixes the Quantumult X parser evidence for local cron and event
metadata. It follows the `[task_local]` shape shown in the Quantumult X sample
configuration, but the policy core only exposes task descriptors to adapters.
It does not schedule tasks, execute JavaScript, or handle platform event
dispatch.

Reference sample:
<https://github.com/crossutility/Quantumult-X/blob/master/sample.conf>

## Input Forms

The parser accepts cron task lines inside `[task_local]` or `#[task_local]`:

```text
#[task_local]
*/15 * * * * https://scripts.example/task.js, tag=qx.task.cron
0 30 8 * * * https://scripts.example/task-six-field.js, tag=qx.task.six
event-network https://scripts.example/network.js, tag=qx.task.network
event-interaction https://scripts.example/interaction.js, tag=qx.task.interaction
```

Supported fields:

- five-field and six-field cron expressions;
- `event-network` and `event-interaction` trigger tokens;
- local or remote script path token after the cron expression;
- `tag`;
- `argument`;
- `timeout` seconds and `timeout-ms`/`timeout_ms` milliseconds;
- `max-size` and `max_size`;
- `enable` and `enabled`.

Other event trigger forms stay ignored in the portable profile and must not
register task descriptors.

## Parser Output

For valid task lines, the parser must:

- register one task descriptor per supported cron line;
- register one task descriptor per supported event line;
- emit an accepted diagnostic with section `Script`, action `task`, and message
  `task descriptor accepted`;
- expose cron schedule, script path, tag, resolved argument, timeout, max-size,
  enabled state, and parser origin through
  `anixops_engine_copy_task_descriptor`;
- expose event trigger type through task kind and the `schedule` field while
  keeping dispatch adapter-owned;
- keep task descriptors separate from HTTP request/response script URL
  dispatch.

Malformed cron lines are rejected when the parser recognizes a cron-like task
line but cannot validate enough cron fields or a script path. Malformed event
lines are rejected when a supported event token is present without a script
path.

## Positive Case

Parser case:

```text
tests/fixtures/QuantumultX.TaskMetadata.snippet
```

Expected behavior:

- config load succeeds;
- two cron task descriptors and two event task descriptors are registered;
- no HTTP script URL dispatch rules are registered by task lines;
- task descriptors expose scheduling metadata through the public ABI.

## Negative Case

Parser case:

```text
tests/fixtures/QuantumultX.TaskMetadata.Malformed.snippet
tests/fixtures/QuantumultX.TaskMetadata.EventMalformed.snippet
```

Expected behavior:

- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects the malformed cron task line;
- `ANIXOPS_COMPAT_QUANTUMULTX_STRICT` rejects the malformed event task line;
- no script rules or task descriptors are registered after the failure;
- a rejected diagnostic is recorded with section `Script`, action `task`, and
  cron expression or event script path failure context.

## Runtime And Security Boundary

Quantumult X task metadata must not:

- execute JavaScript inside the C policy core;
- create a scheduler or background worker;
- download, cache, or validate remote script assets;
- dispatch `event-network`, `event-interaction`, or other platform event
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
  `config/quantumultx_task_metadata_event_malformed_fixture_rejects_missing_path`;
- `tests/test_config.c` registers
  `config/quantumultx_task_metadata_malformed_fixture_rejects_invalid_cron`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Quantumult X task metadata
```

The row remains `partial` because cron and event task descriptors are covered
while scheduler execution, event dispatch, task JavaScript bindings,
concurrency, and permission policy remain adapter-owned.
