# Adapter Redaction Policy

Status: accepted for the v1.0.0 policy-core track.

This contract defines the privacy boundary for policy-core traces and host
adapter telemetry. It prevents v1.0.0 compatibility work from turning trace
support into payload logging.

## Stable Markers

```text
adapter-redaction-policy-core-sensitive-payload-logging=forbidden
adapter-redaction-default-trace-export=metadata-only
adapter-redaction-runner-trace-body=not-serialized
adapter-redaction-runner-header-map=matched-policy-output-only
adapter-redaction-production-adapter-implementation=pending
```

## Policy-Core Boundary

`mitm_anixops` policy-core APIs expose deterministic policy decisions. The
policy core may return:

- rule actions and status codes;
- rule indexes and matched rule patterns;
- bounded rewrite output values that are part of policy decisions;
- selected header mutation results;
- script dispatch metadata;
- MITM decision, reason, matched pattern, and message.

The policy core does not own:

- raw HTTP request or response capture;
- full platform header maps;
- raw request or response body logs;
- persistent telemetry storage;
- log shipping, sampling, retention, or access control.

## Default Runner Trace Behavior

The runner `trace` command is a CI and adapter-contract tool. Its default trace
output is metadata-oriented:

- it does not serialize raw request or response bodies;
- it does not serialize a full inbound or outbound header map;
- it emits only policy outputs required to prove URL, MITM, header mutation,
  body mutation decision metadata, and script trigger selection;
- MITM trace fields are emitted only when the caller supplies `--host`;
- certificate state and MITM enablement remain explicit trace inputs.

The runner golden traces are therefore release evidence for policy decisions,
not a production logging format.

## Adapter Requirements

Before a host adapter can claim production trace export, telemetry, or payload
logging support, it must document and test:

- which fields are exported by default;
- which fields require explicit user or operator enablement;
- header and body redaction rules;
- credential, cookie, authorization header, token, and personal-data handling;
- log sampling and retention;
- local storage path and permissions;
- remote export destinations;
- operator-visible controls and audit evidence;
- failure behavior when redaction cannot be applied.

Production adapters must default to metadata-only export. Raw payload logging
requires an adapter-owned opt-in contract outside the C policy core.

## Manual Intervention

Production adapter redaction implementation remains pending in
[manual-intervention.md](../manual-intervention.md) as
`adapter-redaction-implementation-status=pending`.

That marker must remain pending until a platform owner documents host adapter
redaction behavior and provides GitHub Actions or platform-specific evidence.

## CI Evidence

GitHub Actions verifies this contract through:

- `scripts/security-claim-check.sh`, which requires the stable markers above;
- governance checks in `.github/workflows/build.yml`;
- runner golden JSON traces that omit request/response bodies and full header
  maps.

## Unimplemented Items

- Production adapter redaction implementation.
- Adapter trace export storage and retention policy.
- Adapter UI or operator controls for explicit payload capture.
- Platform-specific audit logging and access control evidence.
