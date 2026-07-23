#!/usr/bin/env bash
# Static gate: Renderer IQ P0 remediation — SceneHDR ownership, GI gate,
# reversed-Z gamma depth, OIT fog, MBOIT safety.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

OWN_H="$ROOT/renderers/vulkan/vk_scene_hdr_ownership.h"
OWN_C="$ROOT/renderers/vulkan/vk_scene_hdr_ownership.c"
BACKEND="$ROOT/renderers/vulkan/tr_backend.c"
RGI="$ROOT/renderers/vulkan/vk_raster_gi.c"
GAMMA="$ROOT/renderers/vulkan/shaders/glsl/gamma.frag"
VOL="$ROOT/renderers/vulkan/vk_volumetric_pass.c"
POST="$ROOT/renderers/vulkan/vk_postfx_passes.c"
TEMP="$ROOT/renderers/vulkan/vk_temporal.c"
INIT="$ROOT/renderers/vulkan/tr_init.c"
DOC="$ROOT/docs/COLOR_PIPELINE.md"

[[ -f "$OWN_H" ]] || fail "missing $OWN_H"
[[ -f "$OWN_C" ]] || fail "missing $OWN_C"
grep -q 'SCENE_HDR_WBOIT_RESOLVE' "$OWN_H" || fail 'SceneHDR stage enum missing WBOIT_RESOLVE'
grep -q 'vk_scene_hdr_allows_pre_oit_gi' "$OWN_H" || fail 'allows_pre_oit_gi missing'
grep -q 'scene_hdr_status' "$OWN_C" || fail 'scene_hdr_status command missing'
pass 'P0-A SceneHDR ownership module present'

grep -q 'vk_scene_hdr_allows_pre_oit_gi' "$BACKEND" || fail 'tr_backend must gate post-OIT GI'
grep -q 'vk_scene_hdr_allows_pre_oit_gi' "$RGI" || fail 'raster_gi must check SceneHDR ownership'
pass 'P0-B post-OIT GI gates present'

grep -q 'IQ P0-C' "$TEMP" || fail 'temporal weapon/bloom P0-C comment missing'
grep -q 'r_weaponBloomMode && r_weaponBloomMode->integer == 1' "$TEMP" || \
	fail 'weapon-after-world must consider bloom mode 1'
grep -qv 'vk_temporal_reconstruction_wanted()' <(grep -A6 'vk_temporal_defer_bloom_for_weapon' "$TEMP") || \
	true
# defer_bloom must NOT require reconstruction_wanted
if grep -A8 'qboolean vk_temporal_defer_bloom_for_weapon' "$TEMP" | grep -q 'reconstruction_wanted'; then
	fail 'defer_bloom_for_weapon must not require TAA reconstruction (P0-C)'
else
	pass 'P0-C bloom defer no longer requires TAA'
fi

grep -q 'depth_view.glsl' "$GAMMA" || fail 'gamma.frag must include depth_view.glsl'
grep -q 'Depth_LinearizeReversedZ' "$GAMMA" || fail 'gamma.frag must use reversed-Z linearization'
if grep -q 'zFar - depthNdc \* ( zFar - zNear )' "$GAMMA"; then
	fail 'gamma.frag still contains forward-Z linearDepth formula'
else
	pass 'P0-D gamma.frag reversed-Z depth'
fi

grep -q 'r_oitFogMode 0 refused' "$VOL" || fail 'volumetric must refuse fogMode 0 with r_oit'
grep -q 'double-fog prevention' "$POST" || fail 'bloom path must skip post-WBOIT volumetric'
pass 'P0-E OIT double-fog prevention'

grep -q 'r_oitAllowExperimentalMboit' "$INIT" || fail 'MBOIT allow cvar missing'
grep -q 'r_oitAllowExperimentalMboit' "$POST" || fail 'OIT pass must gate MBOIT on allow cvar'
pass 'P0-F MBOIT experimental safety'

grep -q 'vk_scene_hdr_ownership' "$DOC" || fail 'COLOR_PIPELINE.md must document SceneHDR ownership'
pass 'COLOR_PIPELINE.md documents P0 ownership'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All Renderer IQ P0 remediation checks passed."
