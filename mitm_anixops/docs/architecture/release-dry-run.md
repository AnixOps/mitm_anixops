# Release Dry-Run Source Contract

This contract defines the release dry-run that must exist before v1.0.0 release
automation is allowed to publish assets.

Current mode:

```text
release-dry-run-current-mode=workflow-defined
release-dry-run-workflow=.github/workflows/release-dry-run.yml
release-dry-run-trigger-scope=workflow-dispatch-pull-request-main-push-main
release-dry-run-trigger-static-check=scripts/ci-trigger-check.sh
release-dry-run-publication=blocked
release-dry-run-ci-gate=equivalent-full-check-in-workflow
release-dry-run-compatibility-summary=status-counts-in-manifest-notes-summary
release-dry-run-manual-intervention-static-check=scripts/manual-intervention-check.sh
release-dry-run-manual-intervention-transition-check=scripts/manual-intervention-transition-check.sh
release-dry-run-release-checklist-static-check=scripts/release-checklist-check.sh
release-dry-run-linux-artifact=linux-x64-tarball-with-checksum
release-dry-run-windows-artifact=windows-x64-zip-with-checksum
release-dry-run-metadata-static-check=scripts/release-metadata-check.sh
release-dry-run-next-action=keep-dry-run-non-publishing-while-tag-release-workflow-publishes-after-gates
```

## Purpose

The release dry-run proves the release pipeline without creating a GitHub
Release or uploading public release assets. It must run in GitHub Actions and
must not consume local build output.

## Triggers

The dry-run workflow supports:

- `workflow_dispatch` from `main`;
- `pull_request` targeting `main`;
- `push` to `main`;
- version input using `vMAJOR.MINOR.PATCH`, `vMAJOR.MINOR.PATCH-alpha.N`, or
  `vMAJOR.MINOR.PATCH-rc.N`;
- optional artifact target selection only after each target has a source
  contract.

Tag-triggered artifact builds and GitHub Release asset publication are handled
by `.github/workflows/release.yml`. Dry-run mode remains non-publishing and
continues to prove the equivalent Linux x64 artifact, Windows x64 artifact,
checksum, manifest, release-note, and rollback evidence without creating a
GitHub Release.

## Required Jobs

The dry-run workflow includes these logical gates:

1. `release-policy`: validate version format, branch, event, and source docs.
2. `dry-run`: run an equivalent full CI gate in the same workflow before
   packaging.
3. `manual-intervention-dry-run`: validate manual marker schema, confirmation
   evidence, and pending-to-confirmed transition behavior.
4. `release-contract-dry-run`: validate release checklist and metadata static
   gates before packaging.
5. `release-readiness-dry-run`: run the same stable release-readiness check
   used by the tag release workflow.
6. `compatibility-summary`: summarize compatibility matrix status counts for
   `supported`, `partial`, `planned`, and `unsupported` rows.
7. `artifact-contract`: output artifact names, target platforms, staging paths,
   checksum algorithm, manifest path, and release-note path.
8. `package-*`: build the dry-run artifact in GitHub Actions.
9. `checksum-*`: generate SHA-256 sidecars with two-space file-name records.
10. `manifest-*`: generate JSON manifest and manifest checksum.
11. `release-notes-dry-run`: generate notes containing compatibility scope,
   compatibility status counts, known gaps, manual-intervention status, and
   rollback path.
12. `publish-eligibility-dry-run`: aggregate gates and report whether a tag
   publication would be eligible.
13. `summary`: publish a GitHub Step Summary with artifact, checksum, manifest,
   CI, compatibility, and manual-intervention fields.

## Required Outputs

The dry-run must output these fields:

```text
release_version
release_kind
release_commit
ci_run_id
ci_run_url
ci_run_conclusion
artifact_name
artifact_path
artifact_sha256
artifact_sha256_file
windows_artifact_name
windows_artifact_path
windows_artifact_sha256
windows_artifact_sha256_file
manifest_name
manifest_path
manifest_sha256
manifest_sha256_file
release_notes_path
manual_intervention_status
release_readiness_status
release_readiness_blocking_reason
compatibility_supported_count
compatibility_partial_count
compatibility_planned_count
compatibility_unsupported_count
compatibility_total_count
publish_eligibility_status
publish_blocking_reason
```

## Failure Rules

The dry-run must fail when:

- version format is invalid;
- run is not from `main`;
- the same-workflow CI gate fails;
- artifact is missing, empty, or created outside the workflow;
- checksum sidecar format is invalid;
- manifest omits commit, version, artifact names, checksum values, or supported
  compatibility scope;
- manifest, notes, or summary omit compatibility status counts;
- release notes omit known gaps, rollback path, or manual-intervention status;
- a pending manual-intervention marker is required for the requested target;
- stable release readiness is blocked for the requested version.

## Publication Boundary

Dry-run mode must not:

- call `gh release create`;
- upload GitHub Release assets;
- overwrite tags;
- sign, notarize, or publish store assets;
- read platform signing secrets.

`workflow_dispatch`, `pull_request`, and `push` dry-runs may upload workflow
artifacts for inspection, but public release assets remain blocked until the
tag-triggered release workflow adds its same-commit CI lookup, checksum,
manifest, release-note, rollback, and publish eligibility gates.

## GitHub Actions Evidence

GitHub Actions governance checks must prove this contract and workflow exist.
The workflow uses Actions logs, artifacts, checksums, manifests, release notes,
and step summaries as acceptance evidence.
