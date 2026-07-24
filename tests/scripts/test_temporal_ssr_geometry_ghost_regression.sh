#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
"$ROOT/tests/scripts/test_temporal_ssr_quarantine.sh"

grep -q 'geometry-shaped silhouette ghost' \
	"$ROOT/docs/TEMPORAL_SSR_EXPERIMENTAL.md" ||
	{ echo "FAIL: regression artifact is not documented" >&2; exit 1; }

echo "PASS: geometry-ghost regression remains owned by quarantined Temporal SSR"
