# Loon Task Metadata Source Contract

Capability: Loon `[Script]` scheduled task metadata.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the Loon-specific parser evidence for scheduled task
metadata. The policy core exposes task descriptors to an adapter, but it does
not schedule tasks, execute JavaScript, or run background work.

## Input Forms

The parser accepts task-like rules inside `[Script]`:

```text
[Script]
cron "15 8 * * *" script-path=https://scripts.example/cron.js, tag=cron.tag, argument=Mode=cron, timeout=5, max-size=8192
Task = type=interval, interval=1800, script-path=https://scripts.example/interval.js, tag=interval.tag, enabled=false
```

Supported fields:

- `cron` direct rules with five- or six-field cron expressions;
- attr-list rules with `type=cron`, `type=scheduled`,
  `type=scheduled-script`, `type=interval`, or `type=task` plus a concrete
  cron expression or interval;
- `cronexp`, `cron`, `schedule`, and `interval`;
- `script-path` and `script_path`;
- `tag`;
- `argument`;
- `timeout` seconds and `timeout-ms`/`timeout_ms` milliseconds;
- `max-size` and `max_size`;
- `enable` and `enabled`.

## Parser Output

For valid task lines, the parser must:

- register one task descriptor per supported line;
- emit an accepted diagnostic with section `Script`, action `task`, and message
  `task descriptor accepted`;
- expose kind, schedule or interval seconds, script path, tag, resolved
  argument, timeout, max-size, enabled state, and parser origin through
  `anixops_engine_copy_task_descriptor`;
- keep task descriptors separate from HTTP request/response script URL
  dispatch.

Malformed task lines are rejected when the task parser recognizes the line but
cannot validate the cron expression, interval, or script path.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.TaskMetadata.plugin
```

Expected behavior:

- config load succeeds;
- two argument defaults are registered for task argument templates;
- one HTTP script rule is registered as a dispatch guard;
- two task descriptors are registered;
- request-phase URL script evaluation returns only the HTTP script rule;
- task descriptors expose resolved arguments and scheduling metadata through
  the public ABI;
- the fixture MITM hostname still parses as a normal host pattern.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.TaskMetadata.Malformed.plugin
```

Expected behavior:

- `ANIXOPS_COMPAT_LOON_STRICT` rejects the malformed task line;
- no argument defaults, script rules, task descriptors, or MITM host patterns
  are registered after the failure;
- a rejected diagnostic is recorded with section `Script`, action `task`, and
  cron expression failure context.

## Unsupported Case

Parser case:

```text
tests/fixtures/Loon.TaskUnsupported.plugin
```

Expected behavior:

- config load succeeds in the portable profile;
- unsupported `[Script]` `type=dns`, `type=rule`, and `type=policy` lines
  record ignored diagnostics;
- no script rules or task descriptors are registered.

## Runtime And Security Boundary

Loon task metadata must not:

- execute JavaScript inside the C policy core;
- create a scheduler or background worker;
- download, cache, or validate remote script assets;
- bypass timeout, max-size, concurrency, or user permission policy owned by the
  adapter/runtime;
- alter MITM, certificate, routing, or trust behavior.

Scheduler/runtime execution is covered by
[Cron And Task Trigger Source Contract](cron-task-trigger.md) and remains
adapter-owned until GitHub Actions evidence exists for a concrete runtime.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_task_metadata_fixture_emits_task_descriptors`;
- `tests/test_config.c` registers
  `config/loon_task_metadata_malformed_fixture_rejects_invalid_cron`;
- `tests/test_config.c` registers
  `config/loon_task_unsupported_fixture_keeps_non_task_types_ignored`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon task metadata
```

The row remains `partial` because parser descriptors and unsupported script-type
guards are covered while scheduler execution, task JavaScript bindings,
concurrency, and permission policy remain adapter-owned.
