#!/usr/bin/env bash
# Static gate: live temporal history notes wired into owners.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

grep -q 'vk_temporal_history_note( HISTORY_TAA' "$ROOT/renderers/vulkan/vk_temporal.c" || fail 'TAA note in temporal'
grep -q 'vk_temporal_history_note( HISTORY_TAA' "$ROOT/renderers/vulkan/vk_frame_end.c" || fail 'TAA note on resolve'
grep -q 'vk_temporal_history_note( HISTORY_WEAPON' "$ROOT/renderers/vulkan/tr_backend.c" || fail 'weapon note'
grep -q 'vk_temporal_history_note( HISTORY_SSR' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail 'SSR note'
grep -q 'vk_temporal_history_note( HISTORY_AO' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail 'AO note'
grep -q 'vk_temporal_history_note( HISTORY_VOLUMETRIC' "$ROOT/renderers/vulkan/vk_volumetric_pass_compute.c" || fail 'volumetric note'
grep -q 'vk_temporal_history_note( HISTORY_EXPOSURE' "$ROOT/renderers/vulkan/vk_temporal.c" || fail 'exposure note'
grep -q 'vk_temporal_history_note( HISTORY_BLOOM' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail 'bloom note'
grep -q 'vk_temporal_history_unowned_active' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c" || fail 'unowned check'
grep -q 'notedThisFrame' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c" || fail 'per-frame note flag'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All iq_temporal_history_live checks passed."
