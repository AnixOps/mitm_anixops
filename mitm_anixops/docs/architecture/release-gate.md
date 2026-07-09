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

Current dry-run workflow status:

```text
release-dry-run-publication=blocked
release-dry-run-linux-artifact=linux-x64-tarball-with-checksum
release-dry-run-windows-artifact=windows-x64-zip-with-checksum
release-dry-run-compatibility-summary=status-counts-in-manifest-notes-summary
```

Current CI workflow status:

```text
ci-workflow-current-jobs=governance,lint,format-check,compatibility-matrix,macos-smoke,linux-test,windows-binary
ci-workflow-lint-status=shellcheck-error-severity
ci-workflow-format-status=static-format-check
ci-workflow-compatibility-matrix-status=dedicated-job
ci-workflow-compatibility-summary-static-check=scripts/compatibility-status-summary-check.sh
ci-workflow-manual-intervention-status=static-schema-check
ci-workflow-manual-intervention-transition-status=scripts/manual-intervention-transition-check.sh
ci-workflow-script-runtime-security-gate=scripts/script-runtime-security-gate.sh
ci-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
ci-workflow-v1-acceptance-status=static-evidence-check
ci-workflow-repository-governance-status=static-contract-check
ci-workflow-macos-status=policy-core-smoke
ci-workflow-trigger-scope=pull-request-push-workflow-dispatch-including-tag-push
ci-workflow-trigger-static-check=scripts/ci-trigger-check.sh
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
release-workflow-trigger-scope=vstar-tag-push-and-manual-main-validation
release-workflow-trigger-static-check=scripts/ci-trigger-check.sh
release-workflow-publication=tag-publish-enabled
release-workflow-ci-gate=same-commit-main-build-success
release-workflow-publication-gate=same-commit-ci-release-metadata-and-github-release-publication-environment
release-workflow-v1-readiness-gate=required-before-v1-manual-markers-and-no-planned-matrix-rows
release-workflow-manual-intervention-static-check=scripts/manual-intervention-check.sh
release-workflow-manual-intervention-transition-check=scripts/manual-intervention-transition-check.sh
release-workflow-release-checklist-static-check=scripts/release-checklist-check.sh
release-workflow-compatibility-summary-static-check=scripts/compatibility-status-summary-check.sh
release-workflow-script-runtime-security-gate=scripts/script-runtime-security-gate.sh
release-workflow-integration-adapter-readiness-gate=scripts/integration-adapter-readiness-check.sh
release-workflow-sensitive-material-gate=scripts/release-sensitive-material-check.sh
release-workflow-linux-artifact=linux-x64-tarball-with-checksum
release-workflow-windows-artifact=windows-x64-zip-with-checksum
release-workflow-compatibility-summary=status-counts-in-manifest-notes-summary
release-workflow-manifest-schema=release-manifest-v1
release-workflow-digest-format=sha256-sidecars
release-workflow-run-evidence=run-id-url-in-manifest-notes-summary
release-workflow-adapter-readiness-manifest=ci-gated-alpha-boundary-fields
release-workflow-metadata-static-check=scripts/release-metadata-check.sh
release-workflow-metadata=checksums-manifest-notes-summary
release-checklist-static-check=scripts/release-checklist-check.sh
repository-governance-status=confirmed-for-v1-publication
release-rollback-policy=accepted
```

The release workflow builds artifacts in GitHub Actions for `v*` tags and
manual validation from `main`. It also generates checksum sidecars, a JSON
manifest, manifest checksum, release notes, and a GitHub Step Summary. Before
packaging, it verifies that the same commit has a successful `build.yml` run on
`main`, that the release rollback/replacement policy exists, that manual
intervention markers have valid schema and confirmation evidence, that release
checklist and metadata static gates pass, that script runtime security markers
still match the no-embedded-engine and pending production runtime decisions,
that integration adapter readiness markers, alpha package contents,
runner/proxy shim targets, binding package checks, E2E entrypoints, and release
registrations remain connected, and that stable release readiness has passed.
Generated Linux tarballs and Windows
zip artifacts are scanned for
private keys, credential-like filenames, and common token patterns before
metadata validation and again before GitHub Release publication. The stable
readiness gate blocks `v1.0.0` while
manual-intervention markers required before `v1.0.0` or `v1.0.0-release` remain
pending, or while the compatibility matrix contains `planned` rows. For `v*`
tag runs only, it publishes workflow-generated Linux x64 tarball and Windows
x64 zip assets to a GitHub Release after metadata validation and the
`github-release-publication` environment gate. Manual `workflow_dispatch` runs
from `main` remain validation-only and do not publish public release assets.

Repository-level branch protection, `v*` tag protection or rulesets, and
release environment approval are defined in
[Repository Governance Contract](repository-governance.md). These settings are
external to the source tree; the v1 publication controls are confirmed in
`docs/manual-intervention.md`, and the release workflow still requires the
`github-release-publication` environment gate for tag publication.

The release manifest, release notes, and GitHub Step Summary include
compatibility matrix counts for `supported`, `partial`, `planned`,
`unsupported`, and total rows. These counts make the release artifact state
auditable without expanding any compatibility claim beyond the source contracts.
The compatibility summary static check requires each count key to appear
exactly once, use a non-negative integer value, and sum to the total row count
before those values are written into release metadata.

The release manifest also carries `manifest_schema_version=release-manifest-v1`.
The same schema version is repeated in release notes and the GitHub Step
Summary so downstream release evidence readers can reject unknown or drifting
manifest shapes before trusting artifact metadata.

The release manifest also carries `artifact_digest_algorithm=sha256` and
`checksum_sidecar_format=sha256sum-two-space-filename`. Release notes and
GitHub Step Summaries repeat those values so release evidence readers can
validate checksum semantics before consuming artifact digest fields.

The release manifest also carries the release workflow run ID and URL. Release
notes and GitHub Step Summaries repeat those fields so each public artifact set
can be traced back to the exact GitHub Actions release run that produced and
published it.

The release manifest, release notes, and GitHub Step Summary also include
adapter readiness status, gate, scope, and production-boundary fields. The
current value is the CI-gated alpha adapter boundary: policy core, runner,
proxy shim, bindings, and docs are release-packaged, while production
NetworkCore data-plane support remains explicitly unclaimed.

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
- compatibility status summary output has missing, duplicate, non-numeric, or
  inconsistent count fields;
- manual-intervention marker is pending for a required release gate;
- checksum or manifest generation fails;
- release metadata omits compatibility status counts;
- release metadata omits manifest schema version;
- release metadata omits artifact digest algorithm or checksum sidecar format;
- release metadata omits release workflow run ID or URL;
- release metadata omits adapter readiness status, gate, scope, or production boundary;
- release notes omit compatibility scope, known gaps, or rollback path;
- stable release readiness is blocked for the requested version;
- release artifacts contain private keys, credential-like filenames, or common
  token patterns;
- artifact was not created by the release workflow.
