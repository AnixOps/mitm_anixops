# Script Runtime Dependency Decision

Status: accepted for the v1.0.0 policy-core track.

Decision date: 2026-07-08.

## Decision

`mitm_anixops` will not embed QuickJS, JavaScriptCore, or any other production
JavaScript engine in the v1.0.0 C policy core.

The current dependency decision is:

```text
script-runtime-policy-core-dependency=none
script-runtime-reference-runner=node-contract-runner
script-runtime-production-engine-selection=pending
script-runtime-new-engine-before-implementation=requires-new-source-contract-license-review-platform-review-and-github-actions-evidence
```

The Alpha Node contract runner remains a CI/demo/reference adapter for script
contract replay. It is not the production embedded runtime and does not change
the C ABI dependency policy.

## Scope

This decision applies to:

- `include/mitm_anixops.h`;
- `src/mitm_anixops.c`;
- the default C library build;
- v1.0.0 compatibility claims for the policy core.

It does not prevent a future runner, adapter, or host project from adding a
runtime backend after a separate source contract, dependency review, and
GitHub Actions evidence exist.

## Rationale

- The policy core already returns script dispatch metadata and rewrite plans.
- Host adapters own production networking, storage, body decoding, and platform
  permissions, so they are also the correct boundary for production JavaScript
  sandbox policy.
- Adding an embedded engine now would expand ABI, packaging, security, and
  license-review scope before the parser, scheduler, and release gates are
  complete.
- The Node contract runner already provides no-network replay evidence for the
  portable script globals and failure behavior without adding a default C
  runtime dependency.

## Candidate Runtime Backlog

Future work may evaluate:

- QuickJS for desktop or no-UI runner scenarios;
- JavaScriptCore for Apple platform adapters;
- an adapter-owned runtime supplied by NetworkCore or another host;
- a continued external runner model for automation-only use.

No candidate is selected for production in this repository until the future
work adds:

- source contract and compatibility matrix update;
- dependency and license review;
- security sandbox boundary;
- packaging impact review;
- positive and negative runtime tests;
- GitHub Actions evidence for every claimed target.

## Security Boundary

Any future runtime must preserve the existing safety rules:

- no automatic root trust;
- no default decryption of non-target hostnames;
- no sensitive header/body logging by default;
- no background network access unless explicitly contracted and tested;
- fail-open script mutation behavior unless a later source contract changes
  that policy.

## Manual Intervention

The future production runtime selection is tracked in
[manual-intervention.md](../manual-intervention.md) as
`script-runtime-engine-selection-status=pending`.

The pending marker is required before claiming a production embedded JavaScript
runtime, but it is not a reason to bypass CI or to describe the Alpha Node
runner as production runtime support.

## CI Evidence

GitHub Actions governance must prove:

- this decision document exists;
- `docs/manual-intervention.md` contains the pending production runtime
  selection marker;
- `TODO.md` records the P3 dependency decision item as complete.
