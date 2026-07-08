#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

required_files="
docs/architecture/certificate-lifecycle.md
docs/architecture/adapter-redaction-policy.md
docs/manual-intervention.md
docs/compatibility/matrix.md
tests/test_mitm.c
"

for file in $required_files; do
	test -f "$file"
done

grep -q "certificate-lifecycle-policy-core-ca-trust=adapter-supplied" docs/architecture/certificate-lifecycle.md
grep -q "certificate-lifecycle-automatic-root-trust=forbidden" docs/architecture/certificate-lifecycle.md
grep -q "certificate-lifecycle-non-target-host-decryption=forbidden" docs/architecture/certificate-lifecycle.md
grep -q "certificate-lifecycle-signing-materials=manual-intervention-only" docs/architecture/certificate-lifecycle.md
grep -q "adapter-redaction-policy-core-sensitive-payload-logging=forbidden" docs/architecture/adapter-redaction-policy.md
grep -q "adapter-redaction-default-trace-export=metadata-only" docs/architecture/adapter-redaction-policy.md
grep -q "adapter-redaction-runner-trace-body=not-serialized" docs/architecture/adapter-redaction-policy.md
grep -q "adapter-redaction-runner-header-map=matched-policy-output-only" docs/architecture/adapter-redaction-policy.md
grep -q "adapter-redaction-production-adapter-implementation=pending" docs/architecture/adapter-redaction-policy.md
grep -q "ca-root-generation-policy-status=pending" docs/manual-intervention.md
grep -q "platform-certificate-store-status=pending" docs/manual-intervention.md
grep -q "protected-runtime-environment-status=pending" docs/manual-intervention.md
grep -q "mitm-signing-materials-status=pending" docs/manual-intervention.md
grep -q "certificate-trust-ux-status=pending" docs/manual-intervention.md
grep -q "adapter-redaction-implementation-status=pending" docs/manual-intervention.md
grep -q "mitm/certificate_state_matrix_blocks_untrusted_states" tests/test_mitm.c
grep -q "mitm/no_host_match_bypasses" tests/test_mitm.c
grep -q "mitm/malformed_runtime_hosts_do_not_intercept_even_with_wildcard" tests/test_mitm.c

forbidden_claims='automatic root trust supported|automatic root trust is supported|root CA auto-install supported|auto-install root CA supported|automatically trusts the root CA|automatically trust root certificates by default|decrypts all hostnames by default|decrypt every hostname by default|non-target hostname decryption supported|hidden traffic capture supported|bypass user authorization supported'
forbidden_logging_claims='logs raw request bodies by default|logs raw response bodies by default|logs raw headers by default|exports raw request bodies by default|exports raw response bodies by default|exports full header maps by default|payload logging enabled by default|sensitive payload logging supported by default'

if grep -RInE "$forbidden_claims" README.md ROADMAP.md TODO.md CHANGELOG.md CONTRIBUTING.md docs; then
	printf '%s\n' "forbidden certificate or MITM safety claim found" >&2
	exit 1
fi

if grep -RInE "$forbidden_logging_claims" README.md ROADMAP.md TODO.md CHANGELOG.md CONTRIBUTING.md docs; then
	printf '%s\n' "forbidden payload logging or redaction claim found" >&2
	exit 1
fi

printf '%s\n' "security claim check passed"
