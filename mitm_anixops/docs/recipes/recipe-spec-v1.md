# Recipe Module Format, Version 1

Capability: declarative composition of supported rewrite primitives.

Ecosystem: `portable`, plus per-ecosystem projections for `surge`, `loon`,
`quantumultx`, `shadowrocket`, and `stash` where the projection exists.

Status: `documentation-only`. This specification defines a file convention. It
adds no parser branch, no ABI symbol, and no compatibility capability.

## Purpose

This document fixes the v1 format for a *recipe module*: one file that a user
loads through any host that already accepts `mitm_anixops` grammar, and that
describes a complete, reproducible rewrite outcome for one target.

The format exists so that adding a target costs one file and one document,
rather than a new code path. It is deliberately expressible in grammar the
policy core already parses, so a recipe can be validated by
`anixops-mitm-runner scan`, `trace`, and `replay` without any new build flag.

## Non-Goals

A recipe module does not own and cannot own:

- TLS interception, certificate generation, or trust decisions
  ([certificate-lifecycle](../architecture/certificate-lifecycle.md));
- HTTP parsing, framing, compression, or streaming
  ([body-mutation-common](../compatibility/body-mutation-common.md));
- JavaScript execution ([script_runtime_contract](../script_runtime_contract.md));
- QUIC handling, routing, or proxy selection
  ([policy-intent-common](../compatibility/policy-intent-common.md));
- telemetry, trace export, or redaction
  ([adapter-redaction-policy](../architecture/adapter-redaction-policy.md)).

A recipe that needs one of those states the dependency explicitly and marks the
step `adapter-required`.

## File Shape

A v1 recipe module is a single text file with an optional `#!` metadata block
followed by standard sections. Nothing below requires a new section name.

```text
#!name = <display name>
#!desc = <one line description>
#!recipe = recipe/v1
#!recipe.id = <reverse-dns id>
#!recipe.version = <semver of this recipe file>
#!recipe.scope = <host, repeated per host>
#!recipe.authority = local | server | dual
#!recipe.strategy = clamp | max | replay | observe
#!recipe.guard = key=value, key=value
#!recipe.evidence = <runner command class>

[Argument]
<key> = <type>,<default>,tag=[...],desc=...

[MITM]
hostname = <exact hosts only>

[URL Rewrite]
<supported rewrite line>

[Header Rewrite]
<supported header mutation line>

[Body Rewrite]
<supported body mutation line>

[Script]
http-request  <pattern> requires-body=1, max-size=..., timeout=..., script-path=..., tag=..., argument=[{Key}]
http-response <pattern> requires-body=1, max-size=..., timeout=..., script-path=..., tag=..., argument=[{Key}]
```

Metadata lines are tolerated `#!` metadata. The policy core records them as
diagnostics and gives them zero policy behavior, exactly as documented for Loon
`#!` metadata tolerance
([loon-hashbang-metadata](../compatibility/loon-hashbang-metadata.md)) and
Surge `#!requirement` tolerance
([surge-requirement-metadata](../compatibility/surge-requirement-metadata.md)).
A reader that ignores every `#!recipe.*` line still gets a working module,
because the behavior lives only in the standard sections. That is the
compatibility guarantee of this format.

## Metadata Keys

| Key | Required | Type | Meaning |
| --- | --- | --- | --- |
| `#!recipe` | yes | `family/major` | Format version. A reader that does not know the family stops reading the file. |
| `#!recipe.id` | yes | string | Stable identity, reverse-DNS, lowercase, dots and hyphens only. |
| `#!recipe.version` | yes | semver | Version of this recipe file, independent of the target's version. |
| `#!recipe.scope` | yes | host | One line per host. Union must equal the `[MITM] hostname` list. |
| `#!recipe.authority` | yes | enum | `local`, `server`, or `dual`. Determined by the probe procedure in [adding-a-recipe.md](adding-a-recipe.md#step-2-authority). |
| `#!recipe.strategy` | yes | enum | `observe`, `clamp`, `max`, or `replay`. See strategy selection. |
| `#!recipe.guard` | conditional | attrs | Required for `clamp` and `replay`. Keys below. |
| `#!recipe.evidence` | yes | enum | `runner-scan`, `runner-trace`, `runner-replay`, or `manual-trace`. |
| `#!recipe.reviewed` | no | date | Date of the last human review of the target behavior. |

Guard keys:

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `clamp-ratio` | float 0..1 | `0.25` | Maximum relative change per mutation. Prevents a single step from becoming an outlier. |
| `cooldown` | seconds | `60` | Minimum spacing between two mutations on the same key. |
| `monotonic` | bool | `1` | Forbid decreasing a previously increased counter. |
| `host-allowlist` | bool | `1` | Refuse to apply when the evaluated host is outside `#!recipe.scope`. |
| `max-size` | bytes | `262144` | Per-rule body ceiling. Mirrors the adapter ceiling in [body-mutation-common](../compatibility/body-mutation-common.md). |

A guard is a declaration of intent. Where the policy core cannot enforce a guard
by itself, the recipe must also carry the enforcing primitive, such as a
`max-size` attribute on the script rule or a `[Script]` rule that applies the
clamp. A recipe must never claim enforcement it does not implement.

## Primitive Mapping

Each recipe step maps to exactly one grammar line whose acceptance is already
proven by a fixture. The mapping table is the contract for reviewers.

| Step | Grammar | Proven by |
| --- | --- | --- |
| Response JSON field replacement | `response-body-json-replace $.path literal` in `[URL Rewrite]`, `[Body Rewrite]` | [body-mutation-common](../compatibility/body-mutation-common.md) |
| Request JSON field replacement | `request-body-json-replace $.path literal` in `[Body Rewrite]` | [body-mutation-common](../compatibility/body-mutation-common.md) |
| Response regex replacement | `response-body-replace-regex pattern replacement` | [body-mutation-common](../compatibility/body-mutation-common.md) |
| Computed body mutation | `http-response-jq '<filter>'`, `response-body-jq '<filter>'` | [body-mutation-common](../compatibility/body-mutation-common.md) |
| Request header mutation | `header-add`, `header-replace`, `header-delete`, `header-replace-regex` in `[Header Rewrite]` | [header-mutation-common](../compatibility/header-mutation-common.md) |
| Host selection | `[MITM] hostname =` with exact hosts, globs, and `-deny` entries | [loon-mitm-options](../compatibility/loon-mitm-options.md) |
| Parameterization | `[Argument]` defaults, `#!arguments`, `argument=[{Key}]` | [loon-argument-section](../compatibility/loon-argument-section.md) |
| Deferred logic | `[Script]` `http-request` / `http-response` dispatch with `requires-body`, `timeout`, `max-size`, `tag`, `argument` | [loon-script-metadata](../compatibility/loon-script-metadata.md) |
| Fixed response | `echo-response` (Quantumult X `url` form) | [response-rewrite-common](../compatibility/response-rewrite-common.md) |

Anything outside this table is `adapter-required` and must be labeled as such
in the recipe.

## Composition And Ordering

Within one phase the policy core exposes an aggregated plan
([plan-api-parity](../compatibility/plan-api-parity.md)). A v1 recipe assumes
this order and must be written so the order does not matter for correctness:

1. URL rewrite and redirect intent;
2. header mutation;
3. body mutation, single rule then body chain;
4. script dispatch metadata, executed by the host runtime.

Rules that must observe the same body belong in one rule or in one script, not
in two competing rules. Two `response-body-json-replace` rules that target the
same JSON path are a recipe defect: the outcome then depends on merge order
rather than on the recipe.

## Strategy Selection

| Strategy | Use when | Primitive shape |
| --- | --- | --- |
| `observe` | Reconnaissance only. Record traffic, change nothing. | `[MITM]` plus `max-size`-bounded `[Script]` that logs and passes through. |
| `clamp` | The value is decided remotely and only a bounded step is plausible. | JQ or script that applies a guarded factor. |
| `max` | The value is client-decided and presentation-only. | Static `response-body-json-replace`. |
| `replay` | A single legitimate transaction is idempotent and can be repeated with paced timing. | Runner `replay` first, then a paced loop outside this library. |

`max` is the cheapest and the most visible. `clamp` produces the smallest step
and needs a script runtime. `replay` is the most fragile and must always be
paced.

## Verification Contract

Every recipe must be reproducible through the existing no-UI runner:

```sh
# 1. The module parses, with the expected rule counts.
build/anixops-mitm-runner scan path/to/recipe.sgmodule

# 2. The intended phase matches the intended host, and no other phase does.
build/anixops-mitm-runner trace --plugin path/to/recipe.plugin \
  --url https://api.example.test/v1/settle --phase response

# 3. The mutation is deterministic for a fixed input.
build/anixops-mitm-runner replay --plugin path/to/recipe.plugin \
  --fixture path/to/recipe.tsv
```

Expected stable output fields:

- `scan`: accepted rule count, ignored diagnostic count, rejected diagnostic
  count, and one diagnostic per tolerated `#!recipe.*` line;
- `trace`: matched rule identity, phase, and `requires_body`;
- `replay`: per-case mutated body, where a fixed input must produce a fixed
  output byte for byte.

A recipe whose replay output changes between two runs on the same input is a
recipe defect. Non-determinism almost always means an unordered multi-rule body
chain or a script with hidden state.

## Versioning

- `v1` freezes the keys in the two tables above.
- New optional keys are additive inside `v1` and must be ignorable.
- A semantics change to an existing key requires `v2`.
- A reader must stop at the first key it cannot parse and report the file as
  `unsupported-recipe-version` rather than guessing.

## Conformance Checklist

A v1 recipe is accepted for review when all of these hold:

- [ ] `#!recipe` is `recipe/v1`.
- [ ] `#!recipe.scope` and `[MITM] hostname` are identical sets.
- [ ] `#!recipe.id` is unique among the modules its author publishes.
- [ ] Every step appears in the primitive mapping table, or is labeled
      `adapter-required`.
- [ ] Guard keys required by the strategy are present.
- [ ] No step depends on an ordering accident between two body rules.
- [ ] `runner scan` and `runner trace` succeed with the documented counters.
- [ ] The document states known gaps.
- [ ] The recipe touches no host outside its declared scope.
