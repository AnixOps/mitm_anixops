# Release Dry-Run Source Contract

This contract defines the release dry-run that must exist before v1.0.0 release
automation is allowed to publish assets.

Current mode:

```text
release-dry-run-current-mode=workflow-defined
release-dry-run-workflow=.github/workflows/release-dry-run.yml
release-dry-run-publication=blocked
release-dry-run-ci-gate=equivalent-full-check-in-workflow
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
continues to prove the equivalent artifact, checksum, manifest, release-note,
and rollback evidence without creating a GitHub Release.

## Required Jobs

The dry-run workflow includes these logical gates:

1. `release-policy`: validate version format, branch, event, and source docs.
2. `dry-run`: run an equivalent full CI gate in the same workflow before
   packaging.
3. `release-readiness-dry-run`: run the same stable release-readiness check
   used by the tag release workflow.
4. `artifact-contract`: output artifact names, target platforms, staging paths,
   checksum algorithm, manifest path, and release-note path.
5. `package-*`: build the dry-run artifact in GitHub Actions.
6. `checksum-*`: generate SHA-256 sidecars with two-space file-name records.
7. `manifest-*`: generate JSON manifest and manifest checksum.
8. `release-notes-dry-run`: generate notes containing compatibility scope,
   known gaps, manual-intervention status, and rollback path.
9. `publish-eligibility-dry-run`: aggregate gates and report whether a tag
   publication would be eligible.
10. `summary`: publish a GitHub Step Summary with artifact, checksum, manifest,
   CI, compatibility, and manual-intervention fields.

## Required Outputs

The dry-run must output these fields before a tag release workflow is added:

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
manifest_name
manifest_path
manifest_sha256
manifest_sha256_file
release_notes_path
manual_intervention_status
release_readiness_status
release_readiness_blocking_reason
publish_eligibility_status
publish_blocking_reason
```

## Failure Rules

The dry-run must fail when:

- version format is invalid;
- run is not from `main`;
- same-commit CI success cannot be proven;
- artifact is missing, empty, or created outside the workflow;
- checksum sidecar format is invalid;
- manifest omits commit, version, artifact names, checksum values, or supported
  compatibility scope;
- release notes omit known gaps, rollback path, or manual-intervention status;
- a pending manual-intervention marker is required for the requested target.
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
