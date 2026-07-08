#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

required_files="
docs/architecture/certificate-lifecycle.md
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
grep -q "ca-root-generation-policy-status=pending" docs/manual-intervention.md
grep -q "platform-certificate-store-status=pending" docs/manual-intervention.md
grep -q "protected-runtime-environment-status=pending" docs/manual-intervention.md
grep -q "mitm-signing-materials-status=pending" docs/manual-intervention.md
grep -q "certificate-trust-ux-status=pending" docs/manual-intervention.md
grep -q "mitm/certificate_state_matrix_blocks_untrusted_states" tests/test_mitm.c
grep -q "mitm/no_host_match_bypasses" tests/test_mitm.c
grep -q "mitm/malformed_runtime_hosts_do_not_intercept_even_with_wildcard" tests/test_mitm.c

forbidden_claims='automatic root trust supported|automatic root trust is supported|root CA auto-install supported|auto-install root CA supported|automatically trusts the root CA|automatically trust root certificates by default|decrypts all hostnames by default|decrypt every hostname by default|non-target hostname decryption supported|hidden traffic capture supported|bypass user authorization supported'

if grep -RInE "$forbidden_claims" README.md ROADMAP.md TODO.md CHANGELOG.md CONTRIBUTING.md docs; then
	printf '%s\n' "forbidden certificate or MITM safety claim found" >&2
	exit 1
fi

printf '%s\n' "security claim check passed"
