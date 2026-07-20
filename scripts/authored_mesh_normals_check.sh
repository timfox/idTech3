#!/usr/bin/env bash
# Static gate for experimental authored mesh normals (no GPU).
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

need "renderers/vulkan/tr_mesh_normal_policy.c"
need "renderers/vulkan/tr_mesh_normal_policy.h"
need "config/vulkan_overlay_authored_normals.cfg"
need "docs/AUTHORED_MESH_NORMALS.md"

grep -q 'r_meshNormalPolicy' "$ROOT/renderers/vulkan/tr_mesh_normal_policy.c" || { echo "FAIL cvar missing"; fail=1; }
grep -q 'R_MeshNormalPolicy_ProcessGLTFModel' "$ROOT/renderers/vulkan/tr_model_gltf.c" || { echo "FAIL glTF hook missing"; fail=1; }
grep -q 'R_MeshNormalPolicy_Init' "$ROOT/renderers/vulkan/tr_init.c" || { echo "FAIL init hook missing"; fail=1; }
grep -q 'meshNrm' "$ROOT/renderers/vulkan/tr_init_diagnostics.inc" || { echo "FAIL status line missing"; fail=1; }
grep -q 'tr_mesh_normal_policy.c' "$ROOT/engine/platform/win32/msvc2017/vulkan.vcxproj" || { echo "FAIL MSVC entry missing"; fail=1; }

# Certified default must stay 0 in overlay docs / code default
grep -q 'r_meshNormalPolicy", "0"' "$ROOT/renderers/vulkan/tr_mesh_normal_policy.c" || { echo "FAIL default policy not 0"; fail=1; }
# modern_vulkan.cfg must not force policy
if grep -q 'r_meshNormalPolicy' "$ROOT/config/modern_vulkan.cfg" 2>/dev/null; then
  echo "FAIL modern_vulkan.cfg must not set r_meshNormalPolicy"
  fail=1
else
  echo "OK  modern_vulkan.cfg untouched for mesh normals"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "authored_mesh_normals_check: FAIL"
  exit 1
fi
echo "authored_mesh_normals_check: PASS (static)"
exit 0
