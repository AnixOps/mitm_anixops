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
- [matrix.md](matrix.md): v1.0.0 compatibility matrix.
