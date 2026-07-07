#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DOC="$ROOT/docs/script_runtime_contract.md"

assert_contains() {
	pattern=$1
	if ! grep -F "$pattern" "$DOC" >/dev/null; then
		echo "adapter ordering contract missing: $pattern" >&2
		exit 1
	fi
}

assert_contains "## Adapter Ordering"
assert_contains "### Request Pipeline"
assert_contains "1. Call \`anixops_mitm_evaluate\` before TLS interception."
assert_contains "2. Apply request URL rewrite with \`anixops_rewrite_evaluate_url\` before upstream routing."
assert_contains "3. Enumerate request header rewrites with \`anixops_rewrite_evaluate_header\`."
assert_contains "4. Apply request body rewrites with \`anixops_rewrite_apply_body_chain\` after buffering plain text."
assert_contains "5. Dispatch request scripts with \`anixops_script_evaluate_url\` after static request rewrites."
assert_contains "6. Send the final request upstream."
assert_contains "### Response Pipeline"
assert_contains "1. Dechunk and decompress the upstream response body when later stages need text."
assert_contains "2. Enumerate response header rewrites with \`anixops_rewrite_evaluate_header\`."
assert_contains "3. Apply response body rewrites with \`anixops_rewrite_apply_body_chain\` after plain-text decoding."
assert_contains "4. Dispatch response scripts with \`anixops_script_evaluate_url\` after static response rewrites."
assert_contains "5. Recompute response framing before writing to the client."

echo "adapter ordering contract doc check passed"
