#!/usr/bin/env bash
# Static gate: Raster Ultra 1.9 virtualized raster shadows.
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

need "renderers/vulkan/vk_vshadow.c"
need "renderers/vulkan/vk_vshadow.h"
need "docs/RASTER_ULTRA_1.9.md"
need "config/vulkan_overlay_raster_ultra_1_9_virtual_shadows.cfg"

# Genuine virtual page contract (not renamed CSM)
for token in "virtual page" "page table" "physical" "evict" "demand" "invalidat" "resident" "uninitialized"; do
  if ! grep -qi "$token" "$ROOT/renderers/vulkan/vk_vshadow.c"; then
    echo "FAIL vk_vshadow.c missing genuine concept: $token"
    fail=1
  fi
done
echo "OK  genuine page-system concepts present"

grep -q 'vk_vshadow_update' "$ROOT/renderers/vulkan/tr_backend.c" || {
  echo "FAIL sun shadow path missing vshadow update"
  fail=1
}
grep -q 'vk_vshadow_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL frame_submit missing vshadow begin_frame"
  fail=1
}
grep -q 'vk_vshadow_on_camera_cut' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL camera cut missing vshadow invalidation"
  fail=1
}
grep -q 'vk_vshadow_on_map_change' "$ROOT/renderers/vulkan/tr_bsp.c" || {
  echo "FAIL map load missing vshadow invalidation"
  fail=1
}
grep -q 'vk_vshadow_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL tr_init missing vshadow init"
  fail=1
}
grep -q 'vk_vshadow_fallback_csm\|fallback_csm\|r_vshadowFallbackCsm' "$ROOT/renderers/vulkan/vk_vshadow.c" || {
  echo "FAIL missing CSM fallback policy"
  fail=1
}
grep -q 'localAtlasFallbacks\|atlasFallback' "$ROOT/renderers/vulkan/vk_vshadow.c" || {
  echo "FAIL missing local atlas fallback accounting"
  fail=1
}

for token in "r_shadowLocalLightBudget" "r_shadowCasterDrawBudget" "r_shadowPageRequestBudget" "budgetDrops" "vk_vshadow_budget"; do
  grep -q "$token" "$ROOT/renderers/vulkan/vk_vshadow.c" "$ROOT/renderers/vulkan/vk_vshadow.h" "$ROOT/docs/RASTER_ULTRA_1.9.md" || {
    echo "FAIL missing explicit shadow budget contract: $token"
    fail=1
  }
done

# Boot / Ultra must not force r_vshadow
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_raster_ultra.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_vshadow[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_vshadow 1"
      fail=1
    else
      echo "OK  $cfg does not force r_vshadow"
    fi
  fi
done

grep -q 'seta r_vshadow 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_9_virtual_shadows.cfg" || {
  echo "FAIL overlay missing r_vshadow 1"
  fail=1
}
grep -q 'seta r_vshadowFallbackCsm 1' "$ROOT/config/vulkan_overlay_raster_ultra_1_9_virtual_shadows.cfg" || {
  echo "FAIL overlay must keep CSM fallback"
  fail=1
}
grep -q 'seta r_hybrid1 0' "$ROOT/config/vulkan_overlay_raster_ultra_1_9_virtual_shadows.cfg" || {
  echo "FAIL overlay must lock RT off"
  fail=1
}

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|\bBLAS\b|\bTLAS\b' \
  "$ROOT/renderers/vulkan/vk_vshadow.c"; then
  echo "FAIL vshadow must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in vshadow"
fi

# Must not falsely claim "VSM" without pages — allow "virtual" in docs/module only
if grep -qi 'virtual shadow maps' "$ROOT/renderers/vulkan/vk_sun_csm.c"; then
  echo "FAIL CSM must not be mislabeled as virtual shadow maps"
  fail=1
else
  echo "OK  CSM not mislabeled as VSM"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_9_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_9_check: PASS (static)"
exit 0
