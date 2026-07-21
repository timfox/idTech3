#!/usr/bin/env bash
# Static gate: 4× temporal reprojection / velocity-space fix.
# Guards the canonical UV velocity convention, extent report, prev-frame age
# rejection, once-per-frame jitter advance, jitter-delta cancellation, double-
# resolve counters, and the Phase 9 reprojection debugger.
# Spec: docs/RENDERER_TEMPORAL_REPROJECTION.md
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fail=0

need() {
  local f="$1"
  if [[ ! -f "$ROOT/$f" ]]; then
    echo "FAIL missing: $f"
    fail=1
  else
    echo "OK  $f"
  fi
}

need "docs/RENDERER_TEMPORAL_REPROJECTION.md"
need "renderers/vulkan/vk_velocity_space.h"
need "config/demo_temporal_reprojection.cfg"

# --- Phase 1: canonical velocity space ---
grep -q 'VK_VELOCITY_SPACE_UV' "$ROOT/renderers/vulkan/vk_velocity_space.h" || {
  echo "FAIL VK_VELOCITY_SPACE_UV missing"; fail=1; }
grep -q 'VK_VELOCITY_SPACE_CANONICAL' "$ROOT/renderers/vulkan/vk_velocity_space.h" || {
  echo "FAIL VK_VELOCITY_SPACE_CANONICAL missing"; fail=1; }
grep -q 'out_motion = currUV - prevUV' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || {
  echo "FAIL gen_frag.tmpl must write currentUV - previousUV"; fail=1; }
grep -q 'historyUV = sampleUV - motion' "$ROOT/renderers/vulkan/shaders/glsl/taa.frag" || {
  echo "FAIL taa.frag must consume historyUV = sampleUV - motion"; fail=1; }
# Reject accidental NDC-as-UV producers (missing * 0.5).
if grep -nE 'out_motion\s*=\s*.*Ndc|out_motion\s*=\s*\(.*Clip\.xy\s*/' \
  "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" \
  "$ROOT/renderers/vulkan/shaders/glsl/light_frag.tmpl" 2>/dev/null | \
  grep -v '0\.5'; then
  echo "FAIL motion producer appears to skip NDC→UV * 0.5"
  fail=1
else
  echo "OK  motion producers apply NDC→UV * 0.5"
fi

# --- Phase 3/4: extent report ---
grep -q 'r_temporalResolutionDebug' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL r_temporalResolutionDebug cvar registration missing"; fail=1; }
grep -q 'vk_temporal_resolution_report' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL vk_temporal_resolution_report missing"; fail=1; }
grep -q 'temporal_resolution_status' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL temporal_resolution_status command missing"; fail=1; }

# --- Phase 2/10: CPU velocity probe ---
grep -q 'r_temporalVelocityProbe' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL r_temporalVelocityProbe cvar registration missing"; fail=1; }
grep -q 'vk_temporal_velocity_probe' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL vk_temporal_velocity_probe missing"; fail=1; }
grep -q '2x' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL velocity probe must warn on 2x scale"; fail=1; }

# --- Phase 5/8: prev ownership / dedupe / age ---
grep -q 'VK_MOTION_INVALID_STALE_PREV' "$ROOT/renderers/vulkan/vk_view_state.c" || {
  echo "FAIL stale-prev motion invalid reason missing"; fail=1; }
grep -q 'dupIndex' "$ROOT/renderers/vulkan/vk_view_state.c" || {
  echo "FAIL per-entity motion record dedupe missing"; fail=1; }
grep -q 'vk_prev_matrices_frame' "$ROOT/renderers/vulkan/vk_temporal.h" || {
  echo "FAIL vk_prev_matrices_frame ownership tracking missing"; fail=1; }
grep -q 'prevAge' "$ROOT/renderers/vulkan/vk_view_state.c" || {
  echo "FAIL prevAge field / status report missing"; fail=1; }

# --- Phase 6: double-resolve guards ---
grep -q 'TemporalResolveWorld' "$ROOT/renderers/vulkan/vk_frame_end.c" || {
  echo "FAIL TemporalResolveWorld GPU marker missing"; fail=1; }
grep -q 'TemporalResolveWeapon' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL TemporalResolveWeapon GPU marker missing"; fail=1; }
grep -q 'vk_temporal_note_world_resolve' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL world resolve counter missing"; fail=1; }
grep -q 'worldResolvesThisFrame' "$ROOT/renderers/vulkan/vk.h" || {
  echo "FAIL worldResolvesThisFrame field missing from vk.temporal"; fail=1; }

# --- Phase 7: jitter handling ---
grep -q 'vk_motion_rebase_prev_projection_jitter' "$ROOT/renderers/vulkan/vk_view_state.c" || {
  echo "FAIL previous-projection jitter rebase missing"; fail=1; }
grep -q 's_lastAdvanceFrame' "$ROOT/renderers/vulkan/vk_upscale.c" || {
  echo "FAIL once-per-frame Halton jitter advance missing"; fail=1; }
grep -q 'vk_prev_jitter_valid' "$ROOT/renderers/vulkan/vk_temporal.h" || {
  echo "FAIL vk_prev_jitter_* ownership missing"; fail=1; }

# --- Phase 9: reprojection debugger ---
grep -q 'velocity error ratio' "$ROOT/renderers/vulkan/shaders/glsl/taa.frag" || {
  echo "FAIL taa.frag mode 32 (velocity error ratio) missing"; fail=1; }
grep -q 'reprojection correspondence' "$ROOT/renderers/vulkan/shaders/glsl/taa.frag" || {
  echo "FAIL taa.frag mode 35 (reprojection correspondence) missing"; fail=1; }
grep -q '"0", "35"' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL r_temporalDebug range must allow modes 28–35"; fail=1; }
# Weapon debug window must not steal modes 28–35.
if grep -n 'debugMode >= 15.5 && debugMode < 33.5' \
  "$ROOT/renderers/vulkan/shaders/glsl/weapon_taa.frag" >/dev/null; then
  echo "FAIL weapon_taa.frag still owns modes 28–33 (must end at 27)"
  fail=1
else
  echo "OK  weapon_taa.frag debug window narrowed below mode 28"
fi

# --- Docs ---
grep -qi 'VK_VELOCITY_SPACE_UV' "$ROOT/docs/RENDERER_TEMPORAL_REPROJECTION.md" || {
  echo "FAIL reprojection doc must define VK_VELOCITY_SPACE_UV"; fail=1; }
grep -qi 'r_temporalDebug 28' "$ROOT/docs/RENDERER_TEMPORAL_REPROJECTION.md" || {
  echo "FAIL reprojection doc must describe modes 28–35"; fail=1; }

if [[ "$fail" -ne 0 ]]; then
  echo "temporal_reprojection_check: FAIL"
  exit 1
fi
echo "temporal_reprojection_check: PASS (static)"
exit 0
