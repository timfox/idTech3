#!/usr/bin/env bash
# Static gate: Raster Ultra 2.0 production hardening / frame contract.
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

need "docs/RASTER_ULTRA_2.0.md"
need "config/modern_raster_ultra_2.cfg"
need "renderers/vulkan/vk_pass_registry.c"
need "renderers/vulkan/vk_deferred_gbuffer.c"

grep -q 'Production frame contract\|authoritative' "$ROOT/docs/RASTER_ULTRA_2.0.md" || {
  echo "FAIL doc must declare production frame contract"
  fail=1
}
grep -q 'VK_SPINE_PASS_DEFERRED_LIGHTING' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || {
  echo "FAIL deferred lighting must be spine-instrumented"
  fail=1
}
grep -q 'vk_spine_validate_ultra_frame_contract' "$ROOT/renderers/vulkan/vk_pass_registry.c" || {
  echo "FAIL ultra frame contract validator missing"
  fail=1
}
if grep -n 'vk_spine_validate_ultra_frame_contract' "$ROOT/renderers/vulkan/vk_pass_registry.c" | head -1 | grep -q .; then
  # Call site must appear inside vk_spine_frame_end (before the function definition of the validator itself).
  call_line=$(grep -n 'vk_spine_validate_ultra_frame_contract();' "$ROOT/renderers/vulkan/vk_pass_registry.c" | head -1 | cut -d: -f1)
  def_line=$(grep -n '^void vk_spine_validate_ultra_frame_contract' "$ROOT/renderers/vulkan/vk_pass_registry.c" | head -1 | cut -d: -f1)
  end_line=$(grep -n '^void vk_spine_frame_end' "$ROOT/renderers/vulkan/vk_pass_registry.c" | head -1 | cut -d: -f1)
  if [[ -n "$call_line" && -n "$end_line" && -n "$def_line" && "$call_line" -gt "$end_line" && "$call_line" -lt "$def_line" ]]; then
    echo "OK  ultra contract wired into frame_end (line $call_line)"
  else
    echo "FAIL ultra contract not called from frame_end (call=$call_line end=$end_line def=$def_line)"
    fail=1
  fi
fi
grep -q 'raster_gi' "$ROOT/renderers/vulkan/tr_init_diagnostics.inc" || {
  echo "FAIL havenrp status must report raster_gi owner"
  fail=1
}
grep -q 'RC_BlockedBetween' "$ROOT/renderers/vulkan/vk_radiance_clipmap.c" || {
  echo "FAIL thin-wall block-between missing"
  fail=1
}
if ! grep -F '0.85 * leak' "$ROOT/renderers/vulkan/shaders/glsl/raster_gi/rgi_clipmap_sample.comp" >/dev/null; then
  echo "FAIL clipmap sample leak mute missing"
  fail=1
else
  echo "OK  clipmap leak mute"
fi
if ! grep -F '0.8 * cacheLeak' "$ROOT/renderers/vulkan/shaders/glsl/raster_gi/rgi_resolve.comp" >/dev/null; then
  echo "FAIL resolve cache leak mute missing"
  fail=1
else
  echo "OK  resolve cache leak mute"
fi

# Boot must not force Ultra 2.0
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*exec[[:space:]]+modern_raster_ultra_2\.cfg' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not exec ultra_2"
      fail=1
    else
      echo "OK  $cfg does not exec ultra_2"
    fi
  fi
done

CFG="$ROOT/config/modern_raster_ultra_2.cfg"
grep -q 'seta r_rasterUltra 1' "$CFG" || { echo "FAIL ultra_2 missing r_rasterUltra 1"; fail=1; }
grep -q 'seta r_oit 1' "$CFG" || { echo "FAIL ultra_2 must use WBOIT"; fail=1; }
grep -q 'seta r_aaMode 2' "$CFG" || { echo "FAIL ultra_2 must use SMAA"; fail=1; }
grep -q 'seta r_taa 0' "$CFG" || { echo "FAIL ultra_2 must keep TAA off"; fail=1; }
grep -q 'seta r_ssao 0' "$CFG" || { echo "FAIL ultra_2 must not dual-AO"; fail=1; }
grep -q 'seta r_ssr 0' "$CFG" || { echo "FAIL ultra_2 keeps SSR off until waterfall cert"; fail=1; }
grep -q 'seta r_hybrid1 0' "$CFG" || { echo "FAIL ultra_2 RT lock"; fail=1; }
grep -q 'seta r_rtx 0' "$CFG" || { echo "FAIL ultra_2 RT lock"; fail=1; }
grep -q 'seta r_spineValidate 1' "$CFG" || { echo "FAIL ultra_2 should enable spine validate"; fail=1; }
# Must not force MBOIT / terrain / TAA on
if grep -E '^[[:space:]]*seta[[:space:]]+r_oit[[:space:]]+2([^0-9]|$)' "$CFG" >/dev/null 2>&1; then
  echo "FAIL ultra_2 must not force MBOIT"
  fail=1
fi
if grep -E '^[[:space:]]*seta[[:space:]]+r_cbtTerrain[[:space:]]+1([^0-9]|$)' "$CFG" >/dev/null 2>&1; then
  echo "FAIL ultra_2 must not force terrain"
  fail=1
fi
if grep -E '^[[:space:]]*seta[[:space:]]+r_taa[[:space:]]+1([^0-9]|$)' "$CFG" >/dev/null 2>&1; then
  echo "FAIL ultra_2 must not force TAA"
  fail=1
fi

grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays' \
  "$ROOT/renderers/vulkan/vk_radiance_clipmap.c" && {
  echo "FAIL radiance clipmap must not use RT APIs"
  fail=1
} || echo "OK  no RT APIs in radiance clipmap"

grep -q 'internal candidate\|INTERNAL CANDIDATE' "$ROOT/docs/RASTER_ULTRA_2.0.md" "$CFG" || {
  echo "FAIL must label ultra_2 as internal candidate"
  fail=1
}

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_2_0_check: FAIL"
  exit 1
fi
echo "raster_ultra_2_0_check: PASS (static)"
exit 0
