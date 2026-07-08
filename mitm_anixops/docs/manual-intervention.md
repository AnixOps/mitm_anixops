# Manual Intervention

This file records items that cannot be completed by repository automation. Do
not silently skip these items, and do not use them to bypass GitHub Actions.

## Current Pending Items

```text
branch-protection-status=pending
branch-protection-scope=main
branch-protection-required-before=v1.0.0
branch-protection-next-action=configure-required-github-actions-checks-and-protected-main-in-github
```

```text
protected-tags-status=pending
protected-tags-scope=v*
protected-tags-required-before=v1.0.0-release
protected-tags-next-action=configure-tag-protection-or-repository-ruleset-in-github
```

```text
release-environment-approval-status=pending
release-environment-scope=github-release-publication
release-environment-required-before=v1.0.0-release
release-environment-next-action=decide-whether-release-publication-needs-protected-environment-approval
```

```text
third-party-plugin-corpus-license-status=pending
third-party-plugin-corpus-scope=expanded-real-plugin-corpus
third-party-plugin-corpus-required-before=bundling-non-representative-third-party-fixtures
third-party-plugin-corpus-next-action=confirm-source-url-license-redaction-and-redistribution-policy-before-adding-fixtures
```

```text
script-runtime-engine-selection-status=pending
script-runtime-engine-selection-scope=production-embedded-javascript-runtime
script-runtime-engine-selection-required-before=claiming-production-embedded-js-runtime-support
script-runtime-engine-selection-current-v1-policy-core-decision=no-embedded-js-engine
script-runtime-engine-selection-next-action=choose-quickjs-javascriptcore-adapter-owned-runtime-or-external-runner-after-source-contract-license-review-platform-review-and-github-actions-evidence
```

```text
ca-root-generation-policy-status=pending
ca-root-generation-policy-scope=mitm-root-ca-materials
ca-root-generation-policy-required-before=claiming-production-certificate-lifecycle-support
ca-root-generation-policy-next-action=document-ca-key-generation-storage-rotation-export-backup-and-destruction-policy-per-platform
```

```text
platform-certificate-store-status=pending
platform-certificate-store-scope=system-browser-mobile-enterprise-certificate-stores
platform-certificate-store-required-before=claiming-production-certificate-lifecycle-support
platform-certificate-store-next-action=document-install-trust-detect-revoke-remove-and-rollback-flow-for-each-target-platform
```

```text
protected-runtime-environment-status=pending
protected-runtime-environment-scope=ca-private-keys-leaf-signing-and-runtime-secrets
protected-runtime-environment-required-before=handling-production-mitm-signing-materials
protected-runtime-environment-next-action=confirm-secret-storage-access-control-audit-logging-and-runtime-isolation-per-platform
```

```text
mitm-signing-materials-status=pending
mitm-signing-materials-scope=root-ca-private-key-leaf-certificate-signing-materials
mitm-signing-materials-required-before=claiming-production-dynamic-leaf-certificate-generation
mitm-signing-materials-next-action=confirm-key-generation-signing-algorithm-validity-period-rotation-and-redaction-policy
```

```text
windows-signing-status=pending
windows-signing-scope=windows-release-artifacts
windows-signing-required-before=signed-windows-v1-release
windows-signing-next-action=confirm-code-signing-certificate-timestamp-service-and-secret-storage
```

```text
macos-signing-notarization-status=pending
macos-signing-notarization-scope=macos-release-artifacts
macos-signing-notarization-required-before=macos-artifact-release
macos-signing-notarization-next-action=confirm-apple-developer-team-signing-notarization-and-secret-storage
```

```text
ios-network-extension-status=pending
ios-network-extension-scope=ios-adapter
ios-network-extension-required-before=ios-mitm-support-claim
ios-network-extension-next-action=confirm-apple-developer-entitlement-provisioning-profile-app-review-and-testflight-path
```

```text
certificate-trust-ux-status=pending
certificate-trust-ux-scope=platform-adapters
certificate-trust-ux-required-before=claiming-production-certificate-lifecycle-support
certificate-trust-ux-next-action=document-user-consent-trust-install-revocation-and-rollback-flow-per-platform
```

```text
networkcore-v1-adapter-boundary-status=pending
networkcore-v1-adapter-boundary-scope=host-adapter-domain-mutation-http-tls-data-plane
networkcore-v1-adapter-boundary-required-before=claiming-networkcore-v1-production-mitm-adapter-support
networkcore-v1-adapter-boundary-next-action=confirm-networkcore-domain-mutation-model-http-tls-data-plane-certificate-lifecycle-script-runtime-and-platform-adapters-in-github-actions
```

```text
route-selection-adapter-status=pending
route-selection-adapter-scope=direct-proxy-policy-intent-and-upstream-route-selection
route-selection-adapter-required-before=claiming-direct-or-proxy-routing-support
route-selection-adapter-current-policy-core-decision=reject-only-url-decision-direct-proxy-ignored
route-selection-adapter-next-action=confirm-adapter-route-contract-proxy-group-resolution-dns-fallback-platform-network-extension-behavior-and-github-actions-evidence
```

```text
task-scheduler-runtime-status=pending
task-scheduler-runtime-scope=cron-task-descriptor-scheduler-and-background-javascript-runtime
task-scheduler-runtime-required-before=claiming-cron-or-task-trigger-support
task-scheduler-runtime-current-policy-core-decision=no-task-descriptor-api-and-task-like-lines-do-not-register-as-http-scripts
task-scheduler-runtime-next-action=confirm-task-descriptor-api-scheduler-runtime-bindings-permission-model-concurrency-policy-and-github-actions-evidence
```

```text
stash-shadowrocket-parser-support-status=pending
stash-shadowrocket-parser-support-scope=dedicated-stash-and-shadowrocket-profile-parsers
stash-shadowrocket-parser-support-required-before=claiming-stash-or-shadowrocket-parser-support
stash-shadowrocket-parser-support-current-policy-core-decision=migration-guard-fixtures-only-no-first-class-parser-support
stash-shadowrocket-parser-support-next-action=confirm-redistributable-fixtures-source-contract-supported-and-malformed-parser-cases-and-github-actions-evidence
```

## Completed Items

```text
github-remote-status=confirmed
github-remote=https://github.com/AnixOps/mitm_anixops.git
```

```text
alpha-0.45.10-release-status=confirmed
alpha-0.45.10-release-tag=v0.45.10-alpha
alpha-0.45.10-release-url=https://github.com/AnixOps/mitm_anixops/releases/tag/v0.45.10-alpha
alpha-0.45.10-ci-run=https://github.com/AnixOps/mitm_anixops/actions/runs/28911145604
alpha-0.45.10-release-assets=anixops-mitm-alpha-0.45.10.tar.gz,anixops-mitm-windows-x64.zip
```

```text
script-runtime-policy-core-dependency-decision-status=confirmed
script-runtime-policy-core-dependency-decision=no-embedded-quickjs-javascriptcore-or-other-js-engine-in-v1-policy-core
script-runtime-policy-core-dependency-decision-doc=docs/architecture/script-runtime-dependency.md
```

## Rules

- Pending items must stay pending until a human or external platform completes
  the action.
- A pending marker may block a release gate, but it must not be used to skip
  CI, tests, checksum, manifest, or release-note generation.
- Any future credential, certificate, signing key, provisioning profile,
  protected environment, store account, or platform entitlement must remain out
  of the repository and be referenced here only as a redacted marker.
