#!/usr/bin/env bash
# Color Pipeline Reconstruction Phase 1: authoritative spaces + stage order.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/COLOR_PIPELINE.md"
H="$ROOT/renderers/vulkan/vk_color_contract.h"
C="$ROOT/renderers/vulkan/vk_color_contract.c"
BF="$ROOT/renderers/vulkan/vk_black_frame.c"
INIT="$ROOT/renderers/vulkan/tr_init.c"
ROUTE="$ROOT/renderers/vulkan/vk_transparency_route.c"

[[ -f "$DOC" ]] || fail "missing COLOR_PIPELINE.md"
[[ -f "$H" ]] || fail "missing vk_color_contract.h"
[[ -f "$C" ]] || fail "missing vk_color_contract.c"

for space in TEXTURE_SRGB TEXTURE_LINEAR SCENE_LINEAR_HDR PREEXPOSED_SCENE_LINEAR_HDR DISPLAY_LINEAR DISPLAY_ENCODED; do
	grep -q "$space" "$DOC" || fail "doc missing space $space"
	grep -q "$space" "$H" || grep -q "$space" "$C" || fail "code missing space $space"
done
pass "color spaces documented and coded"

grep -q 'OIT accumulation\|oit_accum\|OIT resolve' "$DOC" || fail "doc must cover OIT stages"
grep -q 'Weapon HDR\|weapon_hdr' "$DOC" || fail "doc must cover weapon HDR"
grep -q 'Display transform\|display_transform' "$DOC" || fail "doc must cover display transform"
grep -q 'UI composition\|stage.*ui' "$DOC" || fail "doc must cover UI"
grep -q 'r_oit 1' "$DOC" || fail "doc must declare WBOIT production"
grep -q 'r_oit 2' "$DOC" || fail "doc must declare MBOIT experimental"
pass "COLOR_PIPELINE.md contract surface"

grep -q 'VK_COLOR_STAGE_OIT_ACCUM' "$H" || fail "OIT accum stage missing"
grep -q 'VK_COLOR_STAGE_OIT_RESOLVE' "$H" || fail "OIT resolve stage missing"
grep -q 'VK_COLOR_STAGE_WEAPON_HDR' "$H" || fail "weapon stage missing"
grep -q 'VK_COLOR_STAGE_BLOOM' "$H" || fail "bloom stage missing"
grep -q 'VK_COLOR_STAGE_EXPOSURE' "$H" || fail "exposure stage missing"
grep -q 'VK_COLOR_STAGE_TONEMAP' "$H" || fail "tonemap stage missing"
grep -q 'VK_COLOR_STAGE_UI' "$H" || fail "UI stage missing"
grep -q 'color_pipeline_status' "$C" || fail "color_pipeline_status missing"
grep -q 'color_pipeline_validate' "$C" || fail "color_pipeline_validate missing"
grep -q 'r_colorContractDebug' "$C" || fail "r_colorContractDebug missing"
grep -q 'VK_COLOR_SPACE_SCENE_LINEAR_HDR' "$C" || fail "expected OIT space SCENE_LINEAR_HDR"
pass "vk_color_contract module present"

grep -q 'vk_color_contract_register' "$INIT" || fail "tr_init must register color contract"
grep -q 'vk_color_contract_begin_frame' "$BF" || fail "black-frame must begin color contract"
grep -q 'vk_color_contract_note_stage' "$BF" || fail "black-frame must note color stages"
pass "init / black-frame wiring"

grep -q 'bloom->exposure->tonemap' "$ROUTE" || fail "oit_status passOrder must include bloom/exposure/tonemap"
pass "oit_status passOrder aligned"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All color pipeline contract checks passed."
