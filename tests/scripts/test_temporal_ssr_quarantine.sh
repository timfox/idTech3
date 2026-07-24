#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IQ="$ROOT/renderers/vulkan/vk_renderer_iq_p1.c"
POST="$ROOT/renderers/vulkan/vk_postfx_passes.c"
STABLE="$ROOT/config/modern_vulkan_stable.cfg"
REF="$ROOT/config/modern_raster_iq_reference.cfg"

fail() { echo "FAIL: $*" >&2; exit 1; }

for profile in "$STABLE" "$REF"; do
	grep -q 'r_ssrTemporal 0' "$profile" || fail "$profile does not quarantine r_ssrTemporal"
	grep -q 'r_allowExperimentalTemporalSSR 0' "$profile" ||
		fail "$profile permits experimental Temporal SSR"
done

grep -q 'vk_temporal_history_note( HISTORY_SSR, qfalse' "$POST" ||
	fail "current-frame SSR registers production history"
grep -q 'history allocation: none' "$IQ" || fail "status omits zero allocation"
grep -q 'history sampling: none' "$IQ" || fail "status omits zero sampling"
grep -q 'Temporal SSR is experimental and not IQ-certified' "$IQ" ||
	fail "experimental warning missing"
grep -q 'ssr_temporal_validate' "$IQ" || fail "validation command missing"

echo "PASS: stable SSR contributes zero temporal history"
echo "PASS: experimental Temporal SSR is explicitly uncertified"
