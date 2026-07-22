#!/usr/bin/env bash
# Static gate: mode 3 + WBOIT + fog docs; modern_clustered or overlay r_oit 1.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

FOG_DOC="$ROOT/docs/WBOIT_FOG_LAYERS.md"
CERT_DOC="$ROOT/docs/WBOIT_GPU_CERTIFICATION.md"
OVERLAY="$ROOT/config/vulkan_overlay_oit_clustered.cfg"
CLUSTERED="$ROOT/config/modern_clustered.cfg"

[[ -f "$FOG_DOC" ]] || fail "missing $FOG_DOC"
[[ -f "$CERT_DOC" ]] || fail "missing $CERT_DOC"

grep -iq 'WBOIT' "$FOG_DOC" || fail 'WBOIT_FOG_LAYERS.md must mention WBOIT'
grep -q 'r_oitFogMode' "$FOG_DOC" || fail 'WBOIT_FOG_LAYERS.md must document r_oitFogMode'
grep -q 'mode 3' "$FOG_DOC" || fail 'WBOIT_FOG_LAYERS.md must mention mode 3'

grep -iq 'WBOIT' "$CERT_DOC" || fail 'WBOIT_GPU_CERTIFICATION.md must mention WBOIT'
grep -q 'r_oitFogMode\|oit_fog_status' "$CERT_DOC" || fail 'cert doc must reference fog commands/cvars'

overlay_ok=0
clustered_ok=0
if [[ -f "$OVERLAY" ]] && grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+1' "$OVERLAY"; then
	overlay_ok=1
	pass 'vulkan_overlay_oit_clustered.cfg sets r_oit 1'
fi
if [[ -f "$CLUSTERED" ]] && grep -Eq 'seta[[:space:]]+r_oit[[:space:]]+1' "$CLUSTERED"; then
	clustered_ok=1
	pass 'modern_clustered.cfg sets r_oit 1'
fi
if [[ $overlay_ok -eq 0 && $clustered_ok -eq 0 ]]; then
	fail 'modern_clustered.cfg or vulkan_overlay_oit_clustered.cfg must set r_oit 1'
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT fog mode-3 checks passed."
