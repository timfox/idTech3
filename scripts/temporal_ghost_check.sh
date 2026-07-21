#!/usr/bin/env bash
# Static gate: first-person weapon temporal ghosting fix.
# Guards the weapon-after-world-post isolation (Architecture B): weapon
# color/depth must stay out of SSR/temporal consumers, the deferred weapon
# path must queue every weapon command, and the bisect tooling must remain.
# Root cause + verification: docs/RENDERER_TEMPORAL_GHOSTING.md
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

need "docs/RENDERER_TEMPORAL_GHOSTING.md"
need "renderers/vulkan/vk_temporal.c"
need "renderers/vulkan/tr_backend.c"

# --- Isolation gate: weapon deferred past world SSR/temporal post ---
grep -q 'vk_temporal_want_weapon_after_world_post' "$ROOT/renderers/vulkan/vk_temporal.h" || {
  echo "FAIL vk_temporal_want_weapon_after_world_post missing from vk_temporal.h"; fail=1; }
grep -q 'r_weaponSsrIsolation' "$ROOT/renderers/vulkan/vk_aa_policy.c" || {
  echo "FAIL r_weaponSsrIsolation cvar registration missing"; fail=1; }
grep -q 'r_ssao' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL isolation must also key off r_ssao (depth consumer)"; fail=1; }
if grep -n 'vk_temporal_want_weapon_after_world_post' -A16 "$ROOT/renderers/vulkan/vk_temporal.c" | grep -q 'vk.fboActive'; then
  echo "OK  isolation guarded by fboActive"
else
  echo "FAIL isolation must guard vk.fboActive (no FBO = no SSR to isolate from)"
  fail=1
fi

# --- Deferred weapon path must be a queue, not a single overwritten slot ---
grep -q 's_deferredWeaponCmds\[' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL deferred weapon commands must be queued (multi-part view models)"; fail=1; }
grep -q 's_deferredWeaponCmdCount' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL deferred weapon queue counter missing"; fail=1; }
if grep -qE 'static[[:space:]]+drawSurfsCommand_t[[:space:]]+s_deferredWeaponCmd;' "$ROOT/renderers/vulkan/tr_backend.c"; then
  echo "FAIL single deferred weapon slot reintroduced (drops weapon parts)"
  fail=1
else
  echo "OK  no single-slot deferred weapon storage"
fi

# --- Flush ordering: weapon composited after world TAA/SSR, never into history ---
grep -q 'RB_FlushDeferredWeaponAfterTaa' "$ROOT/renderers/vulkan/vk_frame_end.c" || {
  echo "FAIL vk_frame_end must flush deferred weapon after world post"; fail=1; }
grep -q 'RB_TryDeferWeaponDrawSurfs' "$ROOT/renderers/vulkan/tr_local.h" || {
  echo "FAIL RB_TryDeferWeaponDrawSurfs declaration missing"; fail=1; }

# --- Bisect / debug tooling stays available ---
grep -q 'temporal_ghost_status' "$ROOT/renderers/vulkan/vk_temporal.c" || {
  echo "FAIL temporal_ghost_status command missing"; fail=1; }
for gate in r_tsr r_temporalAO r_temporalSSR r_temporalFog r_temporalTransparency; do
  grep -rq "\"$gate\"" "$ROOT/renderers/vulkan/" || {
    echo "FAIL independent temporal gate $gate missing"; fail=1; }
done
grep -q 'fresnelExponent < -0.5' "$ROOT/renderers/vulkan/shaders/glsl/ssr.frag" || {
  echo "FAIL SSR weapon-depth debug encoding missing"; fail=1; }

# --- Consumers must respect the gates ---
grep -q 'r_temporalSSR' "$ROOT/renderers/vulkan/vk_postfx.c" || {
  echo "FAIL PostFX_SSR_IsEnabled must honor r_temporalSSR"; fail=1; }
grep -q 'r_temporalAO' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || {
  echo "FAIL SSAO pass must honor r_temporalAO"; fail=1; }

# --- Documentation must record the root cause ---
grep -qi 'r_weaponSsrIsolation' "$ROOT/docs/RENDERER_TEMPORAL_GHOSTING.md" || {
  echo "FAIL ghosting doc must describe r_weaponSsrIsolation"; fail=1; }
grep -qi 'DEPTH_RANGE_WEAPON' "$ROOT/docs/RENDERER_TEMPORAL_GHOSTING.md" || {
  echo "FAIL ghosting doc must record the weapon depth-range root cause"; fail=1; }

# --- Temporal Weapon Resolve: class mask + TAA reject + weapon MVP ---
need "renderers/vulkan/vk_temporal_class.h"
need "renderers/vulkan/vk_temporal_class.c"
need "renderers/vulkan/shaders/glsl/temporal_class_stamp.frag"
need "renderers/vulkan/shaders/glsl/reactive_stamp_weapon.frag"
grep -q 'TEMPORAL_CLASS_WEAPON' "$ROOT/renderers/vulkan/vk_temporal_class.h" || {
  echo "FAIL TEMPORAL_CLASS_WEAPON enum missing"; fail=1; }
grep -q 'TEMPORAL_CLASS_WORLD' "$ROOT/renderers/vulkan/vk_temporal_class.h" || {
  echo "FAIL TEMPORAL_CLASS_WORLD enum missing"; fail=1; }
grep -q 'r_weaponTemporalMode' "$ROOT/renderers/vulkan/vk_aa_policy.c" || {
  echo "FAIL r_weaponTemporalMode cvar registration missing"; fail=1; }
grep -q 'layout(set = 6' "$ROOT/renderers/vulkan/shaders/glsl/taa.frag" || {
  echo "FAIL taa.frag must sample temporal class on set 6"; fail=1; }
grep -q 'classReject' "$ROOT/renderers/vulkan/shaders/glsl/taa.frag" || {
  echo "FAIL taa.frag must implement class mismatch rejection"; fail=1; }
grep -q 'weaponMatricesHavePrev' "$ROOT/renderers/vulkan/vk.h" || {
  echo "FAIL weapon prev-MVP storage missing from vk.temporal"; fail=1; }
grep -q 'vk_capture_weapon_matrices\|weaponPrevModelMatrix' "$ROOT/renderers/vulkan/vk_view_state.c" || {
  echo "FAIL weapon prev-MVP capture missing from vk_view_state.c"; fail=1; }
grep -q 'vk_reactive_mask_stamp_weapon_from_depth' "$ROOT/renderers/vulkan/vk_reactive_mask.c" || {
  echo "FAIL reactive weapon stamp missing"; fail=1; }
grep -q 'vk_temporal_class_stamp_weapon_from_depth' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL deferred weapon flush must stamp temporal class"; fail=1; }
grep -qi 'r_weaponTemporalMode' "$ROOT/docs/RENDERER_TEMPORAL_GHOSTING.md" || {
  echo "FAIL ghosting doc must describe r_weaponTemporalMode / TAA-on policy"; fail=1; }

if [[ "$fail" -ne 0 ]]; then
  echo "temporal_ghost_check: FAIL"
  exit 1
fi
echo "temporal_ghost_check: PASS (static)"
exit 0
