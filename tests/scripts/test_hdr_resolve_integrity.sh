#!/usr/bin/env bash
# Color Pipeline Phase 2.4: HDR resolve integrity + Phase 2.3.3 soft-particle depth.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/HDR_RESOLVE_INTEGRITY.md"
H="$ROOT/renderers/vulkan/vk_hdr_resolve_contract.h"
C="$ROOT/renderers/vulkan/vk_hdr_resolve_contract.c"
RESOLVE="$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag"
SOFT="$ROOT/renderers/vulkan/shaders/glsl/raster_fx/gp_soft_splat.comp"
COPY="$ROOT/renderers/vulkan/vk_postfx_passes.c"
BF="$ROOT/renderers/vulkan/vk_black_frame.c"

[[ -f "$DOC" ]] || fail "missing HDR_RESOLVE_INTEGRITY.md"
[[ -f "$H" ]] || fail "missing vk_hdr_resolve_contract.h"
[[ -f "$C" ]] || fail "missing vk_hdr_resolve_contract.c"

grep -q 'hdrResolveContract_t' "$H" || fail "contract struct missing"
grep -q 'HDR_RESOLVE_CONTRACT_VERSION' "$H" || fail "VERSION missing"
grep -q 'emptyPreservesOpaque' "$H" || fail "emptyPreservesOpaque missing"
grep -q 'opaqueFromFogSceneCopy' "$H" || fail "fog_scene policy missing"
grep -q 'requireFogSceneCopyThisFrame' "$H" || fail "fog copy requirement missing"
pass "hdrResolveContract_t surface"

grep -q 'hdr_resolve_status' "$C" || fail "hdr_resolve_status missing"
grep -q 'hdr_resolve_validate' "$C" || fail "validate missing"
grep -q 'vk_hdr_resolve_note_fog_scene_copy' "$C" || fail "fog copy note missing"
grep -q 'vk_hdr_resolve_note_scene_hdr_recreate' "$C" || fail "scene recreate note missing"
grep -q 'vk_hdr_resolve_note_depth_recreate' "$C" || fail "depth recreate note missing"
pass "vk_hdr_resolve_contract implementation"

grep -q 'Empty accumulation: preserve opaque' "$RESOLVE" || fail "resolve empty-pixel policy"
grep -q 'vk_hdr_resolve_note_fog_scene_copy' "$COPY" || fail "copy must note fog_scene gen"
grep -q 'vk_hdr_resolve_runtime_validate' "$COPY" || fail "resolve must gate on integrity validate"
grep -q 'vk_hdr_resolve_begin_frame' "$BF" || fail "black-frame must begin hdr-resolve frame"
pass "resolve path wiring"

grep -q 'depth_view.glsl' "$SOFT" || fail "soft splat must include depth_view.glsl"
grep -q 'Depth_LinearizeReversedZ' "$SOFT" || fail "soft splat must use certified linearize"
pass "soft particle Phase 2.3.3"

grep -q 'fog_scene' "$DOC" || fail "doc fog_scene"
grep -q 'SCENE_LINEAR_HDR' "$DOC" || fail "doc space"
grep -q 'hdr_resolve_status' "$DOC" || fail "doc status"
grep -q 'Depth_LinearizeReversedZ\|soft particle' "$DOC" || fail "doc soft particles"
pass "HDR_RESOLVE_INTEGRITY.md"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All HDR resolve integrity checks passed."
