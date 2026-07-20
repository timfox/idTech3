#!/usr/bin/env bash
# Static gate: Raster Ultra 1.12 Frequency-Aware / Moiré Suppression.
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

need "renderers/vulkan/vk_frequency_aware.c"
need "renderers/vulkan/vk_frequency_aware.h"
need "docs/RASTER_ULTRA_1.12.md"
need "config/vulkan_overlay_frequency_aware.cfg"

grep -q 'vk_frequency_aware_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || {
  echo "FAIL frequency aware not wired into frame loop"
  fail=1
}
grep -q 'vk_frequency_aware_init' "$ROOT/renderers/vulkan/tr_init.c" || {
  echo "FAIL frequency aware init missing"
  fail=1
}
grep -q 'renderer_sampler_status' "$ROOT/renderers/vulkan/vk_frequency_aware.c" || {
  echo "FAIL sampler status command missing"
  fail=1
}
grep -q 'CorrectAlpha' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || {
  echo "FAIL coverage-preserving CorrectAlpha missing"
  fail=1
}
grep -q 'toksvig' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || {
  echo "FAIL Toksvig-style specular AA missing"
  fail=1
}
grep -q 'VK_MAT_FEAT_FREQUENCY' "$ROOT/renderers/vulkan/vk_material_ir.h" || {
  echo "FAIL material IR frequency feature bit missing"
  fail=1
}
grep -q 'no TAA\|Does NOT force TAA\|does not force TAA' "$ROOT/docs/RASTER_ULTRA_1.12.md" || {
  echo "FAIL doc must forbid forced TAA as the solution"
  fail=1
}

for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*seta[[:space:]]+r_frequencyAware[[:space:]]+1([^0-9]|$)' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not force r_frequencyAware 1"
      fail=1
    else
      echo "OK  $cfg does not force r_frequencyAware"
    fi
  fi
done

OV="$ROOT/config/vulkan_overlay_frequency_aware.cfg"
grep -q 'seta r_frequencyAware 1' "$OV" || { echo "FAIL overlay missing r_frequencyAware 1"; fail=1; }
grep -q 'seta r_hybrid1 0' "$OV" || { echo "FAIL overlay must lock RT off"; fail=1; }
grep -q 'seta r_taa 0' "$OV" || { echo "FAIL overlay must not force TAA"; fail=1; }
grep -q 'seta r_aaMode 2' "$OV" || { echo "FAIL overlay should keep SMAA (r_aaMode 2)"; fail=1; }
grep -q 'seta r_frequencySelectiveSS 0' "$OV" || { echo "FAIL selective SS must default off"; fail=1; }
grep -q 'seta r_frequencyStochastic 0' "$OV" || { echo "FAIL stochastic filter must default off"; fail=1; }

if grep -qiE 'accelerationStructure|rayQuery|vkCmdTraceRays|\bBLAS\b|\bTLAS\b' \
  "$ROOT/renderers/vulkan/vk_frequency_aware.c"; then
  echo "FAIL frequency module must not use RT APIs"
  fail=1
else
  echo "OK  no RT APIs in frequency module"
fi

# Must not enable global blur / FXAA / TAA as the primary moiré path
if grep -E '^[[:space:]]*seta[[:space:]]+r_bloom[[:space:]]+1([^0-9]|$)' "$OV" >/dev/null 2>&1 || \
   grep -E '^[[:space:]]*seta[[:space:]]+r_fxaa[[:space:]]+1([^0-9]|$)' "$OV" >/dev/null 2>&1 || \
   grep -E '^[[:space:]]*seta[[:space:]]+r_taa[[:space:]]+1([^0-9]|$)' "$OV" >/dev/null 2>&1; then
  echo "FAIL overlay must not force bloom/FXAA/TAA as moiré fix"
  fail=1
else
  echo "OK  overlay does not force blur/FXAA/TAA"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_1_12_check: FAIL"
  exit 1
fi
echo "raster_ultra_1_12_check: PASS (static)"
exit 0
