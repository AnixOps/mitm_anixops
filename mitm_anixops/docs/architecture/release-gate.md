# Release Gate Design

This document defines the intended v1.0.0 release gate. It is a design baseline;
the current workflow has not implemented every item.

## Principles

- Release artifacts must be built in GitHub Actions.
- Local artifacts must never be uploaded as release assets.
- Tag-triggered release publication must require same-commit CI success.
- Artifacts must have checksums and a manifest.
- Release notes must state compatibility scope and rollback path.
- Failed gates must stop publication.

## Required Workflows

### CI Workflow

The CI workflow must eventually cover:

- governance baseline checks;
- lint;
- format check;
- unit tests;
- parser fixture tests;
- compatibility matrix tests;
- integration smoke tests;
- package smoke tests;
- release dry-run checks.

Current workflow: `.github/workflows/build.yml`.

Current gap: there is no separate release dry-run or matrix-specific job yet.

### Release Workflow

The release workflow must eventually support:

- push tag trigger for `v*`;
- manual dry-run trigger from `main`;
- version policy validation;
- same-commit CI lookup;
- artifact build jobs;
- checksum generation;
- manifest generation;
- release notes generation;
- publish eligibility gate;
- GitHub Release asset upload;
- summary output with artifact, checksum, manifest, CI, and rollback fields.

Current gap: no dedicated release workflow exists in this repository.

## Minimum Artifact Set

Future v1.0.0 releases should publish at least:

- source tag archive from GitHub;
- Linux package or runner artifact;
- Windows package or runner artifact;
- checksums for each uploaded artifact;
- manifest JSON;
- release notes.

macOS and iOS artifacts remain planned until platform adapter, signing,
notarization, entitlement, and manual-intervention gates exist.

## Publish Blocking Conditions

Publication must be blocked when:

- same-commit main CI is missing or failed;
- required tests are missing for a claimed capability;
- compatibility matrix has unsupported rows described as supported;
- manual-intervention marker is pending for a required release gate;
- checksum or manifest generation fails;
- release notes omit compatibility scope, known gaps, or rollback path;
- artifact was not created by the release workflow.
