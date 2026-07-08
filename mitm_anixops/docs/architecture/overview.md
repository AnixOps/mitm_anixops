# Architecture Overview

`mitm_anixops` is an embeddable MITM plugin compatibility layer. It should stay
small enough for host applications such as NetworkCore to embed, while exposing
enough structured policy output for adapters to implement traffic handling.

## Layers

### Policy Core

Owned by this repository:

- parse plugin/config text;
- normalize supported Loon, Quantumult X, Surge, and portable AnixOps subsets;
- evaluate MITM hostname policy;
- evaluate URL, header, body, and script-trigger rules;
- return structured diagnostics and mutation plans;
- preserve ABI stability and feature-gated optional backends.

Not owned by the policy core:

- socket IO;
- TLS handshakes;
- root CA trust;
- dynamic leaf certificate generation;
- HTTP/1.1 or HTTP/2 parsing;
- body streaming;
- compression and chunk framing in production adapters;
- JavaScript runtime scheduling;
- platform permissions or user prompts.

### Runtime And Runner

Owned by this repository as no-UI tooling:

- runner scan, trace, and replay;
- fixture corpus validation;
- Node-based script contract runner for Alpha/demos;
- optional proxy shim for integration smoke tests;
- packaging smoke for C, Go, Rust, CMake, and pkg-config consumers.

The runner and shim may demonstrate HTTP/TLS behavior, but production platform
clients must still own platform networking and trust decisions.

### Platform Adapters

Owned by host projects such as NetworkCore or downstream clients:

- network IO and upstream routing;
- certificate generation, storage, install, trust, revoke, and rollback;
- HTTP parsers and serializers;
- HTTP/2 and QUIC behavior;
- body buffering, decompression, recompression, and framing;
- production JavaScript runtime;
- storage, quotas, and concurrency for persistent stores;
- app-store, OS, or enterprise policy compliance.

The v1.0.0 policy-core runtime dependency decision is recorded in
[Script Runtime Dependency Decision](script-runtime-dependency.md): the default
C policy core does not embed QuickJS, JavaScriptCore, or another production
JavaScript engine.

The certificate lifecycle boundary is recorded in
[Certificate Lifecycle Architecture Contract](certificate-lifecycle.md): the
policy core consumes adapter-supplied trust state and never owns automatic root
trust or non-target hostname decryption.

The adapter telemetry and trace privacy boundary is recorded in
[Adapter Redaction Policy](adapter-redaction-policy.md): the runner and policy
core provide metadata-oriented traces and do not claim default raw payload or
full header-map logging.

The repository-level publication boundary is recorded in
[Repository Governance Contract](repository-governance.md): branch protection,
release tag protection, and release publication environment approval remain
manual GitHub settings until maintainers record confirmation evidence.

## NetworkCore Alignment

`networkcore_anixops` currently integrates `mitm_anixops` as a MITM policy
backend through a Rust sys crate and safe adapter. That upstream design keeps
live request/response mutation deferred until NetworkCore has a domain mutation
model and HTTP/TLS data plane.

This repository should therefore expose stable policy and trace outputs, not
take over NetworkCore's platform or network responsibilities.

## Security Defaults

v1.0.0 work must keep these defaults:

- fail closed for unsafe trust states;
- fail open for script/runtime mutation failures unless a source contract says
  otherwise;
- never trust a root CA automatically;
- never decrypt non-target hostnames;
- never log sensitive header/body content by default;
- keep remote script execution disabled unless explicitly configured and tested;
- record external trust, signing, or platform permission requirements in
  `docs/manual-intervention.md`.

## Compatibility Strategy

The v1.0.0 compatibility strategy is corpus-driven:

- Loon high-frequency plugin subset first.
- Quantumult X and Surge common grammar where it maps cleanly.
- Stash `http.mitm` and Shadowrocket common-config subsets where fixtures and
  tests exist; remaining app-profile syntax stays migration-guarded.
- Unsupported behavior must be structured, documented, and testable.
- Compatibility matrix status values are `supported`, `partial`, `planned`, and
  `unsupported`.
