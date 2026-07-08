# Compatibility Documentation

This directory is the v1.0.0 compatibility source of truth.

The older [../compatibility_matrix.md](../compatibility_matrix.md) remains the
Alpha implementation matrix. New v1.0.0 work should update this directory first,
then keep the older matrix aligned until it can be replaced.

## Status Values

- `supported`: implemented and covered by CI with positive and negative tests.
- `partial`: implemented for a documented subset, with explicit gaps.
- `planned`: source contract exists, but implementation or tests are missing.
- `unsupported`: intentionally not implemented, or blocked by adapter/platform
  ownership.

## Required Evidence Per Capability

Every compatibility capability must have:

- source contract;
- parser case;
- positive test;
- negative test;
- compatibility note;
- unimplemented item list when not fully supported.

New rows must not be marked `supported` until GitHub Actions proves the
corresponding tests.

## Files

- [source-contracts.md](source-contracts.md): source contract rules and current
  contracts.
- [loon-plugin-common-fields.md](loon-plugin-common-fields.md): first P1 Loon
  common-fields parser contract.
- [loon-hashbang-metadata.md](loon-hashbang-metadata.md): Loon `#!`
  metadata tolerance contract.
- [quantumultx-common-config.md](quantumultx-common-config.md): P1
  Quantumult X rewrite/MITM common-config parser contract.
- [surge-module-common-config.md](surge-module-common-config.md): P1 Surge
  module common-config parser contract.
- [shadowrocket-common-config.md](shadowrocket-common-config.md): P1
  Shadowrocket common-config parser contract.
- [stash-shadowrocket-migration.md](stash-shadowrocket-migration.md): Stash
  migration notes and Shadowrocket app-profile boundary guards.
- [request-rewrite-common.md](request-rewrite-common.md): request URL rewrite
  parser and policy-core contract.
- [policy-intent-common.md](policy-intent-common.md): reject/direct/proxy
  policy-intent parser and routing-boundary contract.
- [response-rewrite-common.md](response-rewrite-common.md): response-phase
  rewrite parser and policy-core contract.
- [header-mutation-common.md](header-mutation-common.md): bounded request and
  response header mutation parser and policy-core contract.
- [body-mutation-common.md](body-mutation-common.md): bounded request and
  response body mutation parser and policy-core contract.
- [decision-trace-schema.md](decision-trace-schema.md): structured MITM,
  rewrite, header, body, script, and policy-intent trace contract.
- [plan-api-parity.md](plan-api-parity.md): plan API and legacy
  `evaluate_*`/`apply_*` parity contract.
- [binding-parity.md](binding-parity.md): C runner, Go wrapper, and Rust
  wrapper parity contract for shared policy fixtures.
- [script-runtime-common.md](script-runtime-common.md): portable script runtime
  adapter contract for globals, timeout/error behavior, and double `$done`.
- [cron-task-trigger.md](cron-task-trigger.md): cron/task descriptor parser
  contract with scheduler and runtime behavior still adapter-owned.
- [../architecture/certificate-lifecycle.md](../architecture/certificate-lifecycle.md):
  P4 certificate lifecycle and MITM trust boundary contract.
- [../architecture/adapter-redaction-policy.md](../architecture/adapter-redaction-policy.md):
  adapter trace and telemetry redaction boundary contract.
- [matrix.md](matrix.md): v1.0.0 compatibility matrix.
