# Release Gate Design

This document defines the intended v1.0.0 release gate. It is a design baseline;
some v1.0.0 gates remain pending until their GitHub Actions evidence exists.

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

Current dry-run workflow: .github/workflows/release-dry-run.yml.

Current release workflow: .github/workflows/release.yml.

Current CI workflow status:

```text
ci-workflow-current-jobs=governance,lint,format-check,compatibility-matrix,macos-smoke,linux-test,windows-binary
ci-workflow-lint-status=shellcheck-error-severity
ci-workflow-format-status=static-format-check
ci-workflow-compatibility-matrix-status=dedicated-job
ci-workflow-manual-intervention-status=static-schema-check
ci-workflow-macos-status=policy-core-smoke
```

Current CI gap: macOS release artifact, signing, notarization, entitlement, and
platform adapter coverage remain pending.

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
- summary output with artifact, checksum, manifest, compatibility status counts,
  CI, and rollback fields.

Current tag-triggered release workflow status:

```text
release-workflow-current-mode=tag-triggered-build-and-publish
release-workflow-file=.github/workflows/release.yml
release-workflow-publication=tag-publish-enabled
release-workflow-ci-gate=same-commit-main-build-success
release-workflow-publication-gate=same-commit-ci-release-metadata-and-github-release-publication-environment
release-workflow-v1-readiness-gate=required-before-v1-manual-markers-and-no-planned-matrix-rows
release-workflow-compatibility-summary=status-counts-in-manifest-notes-summary
release-workflow-metadata=checksums-manifest-notes-summary
release-rollback-policy=accepted
```

The release workflow builds artifacts in GitHub Actions for `v*` tags and
manual validation from `main`. It also generates checksum sidecars, a JSON
manifest, manifest checksum, release notes, and a GitHub Step Summary. Before
packaging, it verifies that the same commit has a successful `build.yml` run on
`main`, that the release rollback/replacement policy exists, and that stable
release readiness has passed. The stable readiness gate blocks `v1.0.0` while
manual-intervention markers required before `v1.0.0` or `v1.0.0-release` remain
pending, or while the compatibility matrix contains `planned` rows. For `v*`
tag runs only, it publishes those workflow-generated assets to a GitHub Release
after metadata validation and the `github-release-publication` environment gate.
Manual `workflow_dispatch` runs from `main` remain validation-only and do not
publish public release assets.

The release manifest, release notes, and GitHub Step Summary include
compatibility matrix counts for `supported`, `partial`, `planned`,
`unsupported`, and total rows. These counts make the release artifact state
auditable without expanding any compatibility claim beyond the source contracts.

The dry-run boundary that must precede release automation is defined in
[Release Dry-Run Source Contract](release-dry-run.md).

Rollback and replacement rules are defined in
[Release Rollback And Replacement Policy](release-rollback-policy.md).

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
- release metadata omits compatibility status counts;
- release notes omit compatibility scope, known gaps, or rollback path;
- stable release readiness is blocked for the requested version;
- artifact was not created by the release workflow.
