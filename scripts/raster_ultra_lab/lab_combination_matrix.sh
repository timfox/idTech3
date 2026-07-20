#!/usr/bin/env bash
# Raster Ultra 1.11 — combination / pairwise coverage (static).
# Mandatory risk cases + pairwise Ultra overlays. No new rendering.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail=0

echo "=== Raster Ultra 1.11 combination matrix ==="

cvar_forced() {
  local cfg="$1" cv="$2"
  grep -E "^[[:space:]]*seta[[:space:]]+${cv}[[:space:]]+[1-9]" "$ROOT/config/$cfg" >/dev/null 2>&1
}

# Boot must remain certified / not force lab
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg modern_raster_ultra.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if cvar_forced "$cfg" "r_referenceLab"; then
      echo "FAIL $cfg forces r_referenceLab"
      fail=1
    else
      echo "OK  $cfg does not force r_referenceLab"
    fi
  fi
done

# Reference profile: no temporal, no RT
REF="$ROOT/config/modern_raster_reference.cfg"
if [[ -f "$REF" ]]; then
  grep -q 'r_taa 0' "$REF" || { echo "FAIL reference must disable TAA"; fail=1; }
  grep -q 'r_hybrid1 0' "$REF" || { echo "FAIL reference must lock RT"; fail=1; }
  grep -q 'r_bloom 0' "$REF" || { echo "FAIL reference material mode disables bloom"; fail=1; }
  echo "OK  modern_raster_reference.cfg material/lighting pins"
fi

# Overlay must lock RT + enable lab
OV="$ROOT/config/vulkan_overlay_raster_ultra_1_11_reference_lab.cfg"
if [[ -f "$OV" ]]; then
  grep -q 'seta r_referenceLab 1' "$OV" || { echo "FAIL overlay missing r_referenceLab 1"; fail=1; }
  grep -q 'seta r_hybrid1 0' "$OV" || { echo "FAIL overlay must lock RT"; fail=1; }
  grep -q 'seta r_pathtrace 0' "$OV" || { echo "FAIL overlay must lock pathtrace"; fail=1; }
  grep -q 'seta r_filmGrain 0' "$OV" || { echo "FAIL overlay must pin grain off for compare"; fail=1; }
  echo "OK  1.11 overlay risk pins"
else
  echo "FAIL missing overlay"
  fail=1
fi

# Forbidden: OIT + TAA together in lab overlay
if [[ -f "$OV" ]]; then
  if grep -q 'seta r_oit [12]' "$OV" && grep -q 'seta r_taa 1' "$OV"; then
    echo "FAIL lab overlay must not enable OIT+TAA together"
    fail=1
  else
    echo "OK  lab overlay does not force OIT+TAA"
  fi
fi

# Pairwise: each Ultra overlay remains independent (exists + RT lock)
for ov in \
  vulkan_overlay_raster_ultra_1_6_geometry.cfg \
  vulkan_overlay_raster_ultra_1_7_atmosphere.cfg \
  vulkan_overlay_raster_ultra_1_8_materials.cfg \
  vulkan_overlay_raster_ultra_1_9_virtual_shadows.cfg \
  vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg \
  vulkan_overlay_raster_ultra_1_11_reference_lab.cfg
do
  if [[ -f "$ROOT/config/$ov" ]]; then
    if grep -q 'seta r_hybrid1 0' "$ROOT/config/$ov"; then
      echo "OK  pairwise $ov RT-off"
    else
      echo "FAIL $ov missing r_hybrid1 0"
      fail=1
    fi
  else
    echo "FAIL missing $ov"
    fail=1
  fi
done

# Three-way risk notes documented
if grep -qi 'pairwise\|three-way\|combination matrix' "$ROOT/docs/RASTER_ULTRA_1.11.md" 2>/dev/null; then
  echo "OK  combination matrix documented"
else
  echo "FAIL docs missing combination matrix section"
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  echo "lab_combination_matrix: FAIL"
  exit 1
fi
echo "lab_combination_matrix: PASS"
exit 0
