#!/usr/bin/env bash
# Regression: non-split (mode 2) + WBOIT must capture G-buffer BEFORE OIT resolve.
# Post-OIT capture ended post_bloom and left SceneHDR invalid → all-black 3D (UI still alive).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "PASS: $*"; }

BE="$ROOT/renderers/vulkan/tr_backend.c"
RESOLVE="$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag"

[[ -f "$BE" ]] || fail "missing tr_backend.c"
[[ -f "$RESOLVE" ]] || fail "missing oit_resolve.frag"

# Capture must appear in the non-split+OIT branch before vk_oit_pass.
if awk '
  /else if \( r_oit && r_oit->integer/ { in_oit=1 }
  in_oit && /vk_deferred_gbuffer_capture_after_geometry/ { saw_cap=1 }
  in_oit && saw_cap && /vk_oit_pass\( cmd \)/ { ok=1; exit }
  in_oit && /vk_oit_pass\( cmd \)/ && !saw_cap { bad=1; exit }
  END { exit (ok ? 0 : 1) }
' "$BE"; then
	pass 'non-split OIT path captures G-buffer before vk_oit_pass'
else
	fail 'G-buffer capture must run before vk_oit_pass on non-split+OIT path'
fi

# Later sidecar must skip when OIT already resolved.
grep -q 'oitFrameState != VK_OIT_FRAME_RESOLVED' "$BE" || \
	fail 'post-geometry sidecar must skip when oitFrameState is RESOLVED'
pass 'post-OIT G-buffer capture suppressed when resolved'

# Empty-pixel preserve opaque (never black overwrite).
grep -q 'accum.a < 1e-4' "$RESOLVE" || fail 'oit_resolve must preserve opaque on empty accum'
grep -q 'mode == 17' "$RESOLVE" || fail 'r_oitDebug 17 empty-pixel preservation missing'
grep -q 'mode == 18' "$RESOLVE" || fail 'r_oitDebug 18 opaque input missing'
pass 'WBOIT empty-pixel / debug 17–18 present'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All black-frame OIT/G-buffer order checks passed."
