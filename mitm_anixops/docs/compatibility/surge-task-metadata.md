# Surge Task Metadata Source Contract

Capability: Surge `[Script]` cron, interval, and event task metadata.

Ecosystem: `surge`.

Status: `partial`.

## Purpose

This contract fixes Surge task parser evidence for the v1.0.0 compatibility
matrix. It covers the descriptor syntax that overlaps with the current
policy-core task ABI. It does not claim scheduler, JavaScript runtime,
background execution, event dispatch, or platform permission behavior.

The source form is based on the Surge scripting manual, which documents
`[Script]` entries where the line name is followed by `type=cron`,
`cronexp=...`, `type=event`, `event-name=...`, and `script-path=...`
parameters:

```text
https://manual.nssurge.com/scripting/common.html
```

## Input Forms

The parser accepts task-like declarations in `[Script]`:

```text
Surge.Task = type=cron, cronexp="*/20 * * * *", script-path=https://scripts.example/task.js
Surge.Interval = type=interval, interval=1800, script-path=https://scripts.example/interval.js
Surge.Event = type=event, event-name=network-changed, script-path=https://scripts.example/event.js
```

Supported fields:

- `type=cron`, `type=scheduled`, `type=scheduled-script`, `type=interval`, and
  `type=task` when concrete `cronexp` or `interval` metadata is present;
- `type=event` when `event-name` is `network-changed` or `notification`;
- `cronexp`, `cron`, and `schedule` for five- or six-field cron expressions;
- `interval` as positive integer seconds;
- `event-name` and `event_name` for supported event descriptor names;
- `script-path` and `script_path`;
- `tag`, with the left-hand script name used as the fallback tag;
- `argument`, including current argument-template substitution;
- `timeout`, `timeout-ms`, `max-size`, `max_size`, `enable`, and `enabled`.

Unsupported Surge script types such as `type=dns`, `type=rule`, and
`type=policy-group` remain ignored until separate source contracts and tests
exist. Unknown event names are rejected because accepting them would overstate
event dispatch compatibility.

## Parser Output

For valid Surge task lines, the parser must:

- register one task descriptor per supported task line;
- emit accepted diagnostics with section `Script` and action `task`;
- expose task kind, cron schedule, interval, or event name, script path, tag,
  resolved argument, timeout, max-size, enabled state, and parser origin through
  `anixops_engine_copy_task_descriptor`;
- keep task descriptors separate from HTTP request/response script URL
  dispatch.

Malformed task lines are rejected when the parser recognizes a supported task
kind but cannot validate the cron expression, interval, event name, or script
path.

## Positive Case

Parser case:

```text
tests/fixtures/Surge.TaskMetadata.sgmodule
tests/fixtures/Surge.TaskEvent.sgmodule
```

Expected behavior:

- config load succeeds;
- one inline argument default is registered;
- three task descriptors are registered;
- two event task descriptors are registered by the event fixture;
- one HTTP request script trigger remains registered separately;
- cron, six-field cron, and interval task descriptors are observable through
  the public ABI;
- `network-changed` and `notification` event descriptors are observable through
  the public ABI;
- task script paths do not match through `anixops_script_evaluate_url`.

## Negative Case

Parser case:

```text
tests/fixtures/Surge.TaskMetadata.Malformed.sgmodule
tests/fixtures/Surge.TaskEvent.Malformed.sgmodule
tests/fixtures/Surge.TaskEvent.Unsupported.sgmodule
```

Expected behavior:

- `ANIXOPS_COMPAT_SURGE_STRICT` rejects the malformed cron task line;
- `ANIXOPS_COMPAT_SURGE_STRICT` rejects the malformed event task line;
- `ANIXOPS_COMPAT_SURGE_STRICT` rejects unknown event names;
- no task descriptors are registered;
- a rejected diagnostic is recorded with section `Script` and action `task`;
- last error reports cron expression, missing event name, or unsupported event
  name failure at the malformed line.

## Unsupported Case

Parser case:

```text
tests/fixtures/Surge.ScriptUnsupported.sgmodule
```

Expected behavior:

- config load succeeds in the portable profile;
- unsupported `[Script]` `type=dns`, `type=rule`, and `type=policy-group`
  lines record ignored diagnostics;
- no script rules or task descriptors are registered.

## Runtime And Security Boundary

Surge task metadata must not:

- schedule or execute JavaScript;
- dispatch platform events;
- fetch, cache, or update remote scripts;
- expose task JavaScript globals or bindings;
- bypass platform permission requirements;
- alter certificate trust, MITM target scope, routing, DNS, or proxy behavior;
- log sensitive headers or body content.

Scheduler, event dispatch, runtime, permission, and concurrency behavior
remains adapter-owned.

## CI Evidence

Required CI evidence:

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
- `tests/test_config.c` registers
  `config/surge_script_unsupported_fixture_keeps_non_task_types_ignored`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Surge task metadata
```

The row remains `partial` because parser task descriptors are covered while
scheduler/runtime execution, event dispatch, DNS/rule/policy-group scripts,
task JavaScript bindings, unknown event names, permission policy, and broader
Surge task corpus coverage remain unimplemented.
