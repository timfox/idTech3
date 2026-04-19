#!/usr/bin/env bash
set -euo pipefail

# Headless renderer regression checks (repo + optional game base).
#
# Always: verify regression docs, key generated shader blobs, and GLSL tree.
# Optional: if GAME_BASE is set, require listed BSPs from OPTIONAL_GAME_ASSETS.txt.
#
# Usage:
#   ./scripts/renderer_regression_check.sh
#   GAME_BASE=/path/to/game/base ./scripts/renderer_regression_check.sh
#
# Prerequisites: glslangValidator for GLSL validation (same as smoke_test.sh).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find project root" >&2
  exit 1
fi

cd "$PROJECT_ROOT"

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }

echo "=== Renderer regression check (headless) ==="
echo ""

echo "Repo manifest (docs + generated shaders):"
MANIFEST="$PROJECT_ROOT/docs/samples/renderer_regression/REGRESSION_REPO_MANIFEST.txt"
if [ ! -f "$MANIFEST" ]; then
  fail "Missing manifest: $MANIFEST"
else
  while IFS= read -r line || [ -n "$line" ]; do
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    p="$PROJECT_ROOT/$line"
    if [ -f "$p" ]; then
      pass "present: $line"
    else
      fail "missing: $line"
    fi
  done < "$MANIFEST"
fi

echo ""
echo "GLSL stage files (glslangValidator -V):"
if command -v glslangValidator &>/dev/null; then
  shader_dir="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl"
  shader_errors=0
  shader_count=0
  while IFS= read -r -d '' shader; do
    shader_count=$((shader_count + 1))
    rel="${shader#"$PROJECT_ROOT"/}"
    if ! err="$(glslangValidator -V "$shader" -o /dev/null 2>&1)"; then
      echo "$err" >&2
      fail "GLSL: $rel"
      shader_errors=$((shader_errors + 1))
    fi
  done < <(find "$shader_dir" -type f \( -name '*.vert' -o -name '*.frag' -o -name '*.geom' -o -name '*.comp' \) -print0 | sort -z)
  if [ "$shader_count" -eq 0 ]; then
    fail "No GLSL files under $shader_dir"
  elif [ "$shader_errors" -eq 0 ]; then
    pass "all $shader_count GLSL stages validate"
  fi
else
  echo "  ⚠ glslangValidator not found, skipping GLSL validation" >&2
fi

echo ""
echo "GPU morph SSBO: IQM_MORPH_TOP_K (C vs GLSL must match):"
TR_LOCAL="$PROJECT_ROOT/src/renderers/vulkan/tr_local.h"
GEN_VERT="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl/gen_vert.tmpl"
LIGHT_VERT="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl/light_vert.tmpl"
k_c="$(sed -n 's/^#define IQM_MORPH_TOP_K[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL" | head -1)"
k_g="$(sed -n 's/^[[:space:]]*const int IQM_MORPH_TOP_K[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*;$/\1/p' "$GEN_VERT" | head -1)"
k_l="$(sed -n 's/^[[:space:]]*const int IQM_MORPH_TOP_K[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*;$/\1/p' "$LIGHT_VERT" | head -1)"
if [[ -z "$k_c" || -z "$k_g" || -z "$k_l" ]]; then
  fail "could not parse IQM_MORPH_TOP_K from tr_local.h / gen_vert.tmpl / light_vert.tmpl"
elif [[ "$k_c" != "$k_g" || "$k_c" != "$k_l" ]]; then
  fail "IQM_MORPH_TOP_K mismatch: tr_local.h=$k_c gen_vert.tmpl=$k_g light_vert.tmpl=$k_l"
else
  pass "IQM_MORPH_TOP_K=$k_c (C + both vertex templates)"
fi

echo ""
echo "glTF morph cap: GLTF_MAX_MORPH_TARGETS vs IQM_MORPH_TOP_K (GPU SSBO packing):"
GLTF_H="$PROJECT_ROOT/src/renderers/vulkan/tr_model_gltf.h"
gltf_k="$(sed -n 's/^#define GLTF_MAX_MORPH_TARGETS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$GLTF_H" | head -1)"
if [[ -z "$gltf_k" || -z "$k_c" ]]; then
  fail "could not parse GLTF_MAX_MORPH_TARGETS from tr_model_gltf.h or IQM_MORPH_TOP_K from tr_local.h"
elif [[ "$gltf_k" != "$k_c" ]]; then
  fail "GLTF_MAX_MORPH_TARGETS ($gltf_k) != IQM_MORPH_TOP_K ($k_c) - GPU morph SSBO layout will disagree with glTF loader"
else
  pass "GLTF_MAX_MORPH_TARGETS=$gltf_k matches IQM_MORPH_TOP_K"
fi

echo ""
echo "glTF joint cap: GLTF_MAX_JOINTS vs IQM_MAX_JOINTS (skin matrix layout):"
IQM_H="$PROJECT_ROOT/src/renderers/vulkan/iqm.h"
iqm_j="$(sed -n 's/^#define[[:space:]]*IQM_MAX_JOINTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$IQM_H" | head -1)"
gltf_j="$(sed -n 's/^#define GLTF_MAX_JOINTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$GLTF_H" | head -1)"
if [[ -z "$gltf_j" || -z "$iqm_j" ]]; then
  fail "could not parse GLTF_MAX_JOINTS from tr_model_gltf.h or IQM_MAX_JOINTS from iqm.h"
elif [[ "$gltf_j" != "$iqm_j" ]]; then
  fail "GLTF_MAX_JOINTS ($gltf_j) != IQM_MAX_JOINTS ($iqm_j) - skinning matrix buffers disagree between glTF and IQM paths"
else
  pass "GLTF_MAX_JOINTS=$gltf_j matches IQM_MAX_JOINTS"
fi

echo ""
echo "IQM_MAX_JOINTS: OpenGL vs Vulkan iqm.h (duplicate header drift guard):"
IQM_H_GL="$PROJECT_ROOT/src/renderers/opengl/iqm.h"
iqm_j_gl="$(sed -n 's/^#define[[:space:]]*IQM_MAX_JOINTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$IQM_H_GL" | head -1)"
if [[ -z "$iqm_j_gl" || -z "$iqm_j" ]]; then
  fail "could not parse IQM_MAX_JOINTS from opengl/iqm.h or vulkan/iqm.h"
elif [[ "$iqm_j_gl" != "$iqm_j" ]]; then
  fail "IQM_MAX_JOINTS mismatch: opengl/iqm.h=$iqm_j_gl vulkan/iqm.h=$iqm_j"
else
  pass "IQM_MAX_JOINTS=$iqm_j (both iqm.h copies)"
fi

echo ""
echo "IQM_MORPH_TOP_K: OpenGL vs Vulkan tr_local.h (entity morph channel arrays):"
TR_LOCAL_GL="$PROJECT_ROOT/src/renderers/opengl/tr_local.h"
k_gl="$(sed -n 's/^#define IQM_MORPH_TOP_K[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL_GL" | head -1)"
if [[ -z "$k_gl" || -z "$k_c" ]]; then
  fail "could not parse IQM_MORPH_TOP_K from opengl/tr_local.h or vulkan/tr_local.h"
elif [[ "$k_gl" != "$k_c" ]]; then
  fail "IQM_MORPH_TOP_K mismatch: opengl/tr_local.h=$k_gl vulkan/tr_local.h=$k_c"
else
  pass "IQM_MORPH_TOP_K=$k_c (OpenGL + Vulkan tr_local.h)"
fi

echo ""
echo "IQM_MORPH_MAX_CHANNELS: OpenGL vs Vulkan tr_local.h (pending morph name slots):"
ch_gl="$(sed -n 's/^#define IQM_MORPH_MAX_CHANNELS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL_GL" | head -1)"
ch_vk="$(sed -n 's/^#define IQM_MORPH_MAX_CHANNELS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL" | head -1)"
if [[ -z "$ch_gl" || -z "$ch_vk" ]]; then
  fail "could not parse IQM_MORPH_MAX_CHANNELS from opengl/tr_local.h or vulkan/tr_local.h"
elif [[ "$ch_gl" != "$ch_vk" ]]; then
  fail "IQM_MORPH_MAX_CHANNELS mismatch: opengl/tr_local.h=$ch_gl vulkan/tr_local.h=$ch_vk"
else
  pass "IQM_MORPH_MAX_CHANNELS=$ch_vk (OpenGL + Vulkan tr_local.h)"
fi

echo ""
echo "IQM morph pending slots vs GPU top-K (Vulkan tr_local.h):"
mc_top="$(sed -n 's/^#define IQM_MORPH_MAX_CHANNELS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_LOCAL" | head -1)"
if [[ -z "$mc_top" || -z "$k_c" ]]; then
  fail "could not parse IQM_MORPH_MAX_CHANNELS or IQM_MORPH_TOP_K from vulkan/tr_local.h"
elif [[ "$mc_top" != "$k_c" ]]; then
  fail "IQM_MORPH_MAX_CHANNELS ($mc_top) != IQM_MORPH_TOP_K ($k_c) in vulkan/tr_local.h - pending morph slots must match GPU morph top-K packing"
else
  pass "IQM_MORPH_MAX_CHANNELS=$mc_top matches IQM_MORPH_TOP_K (vulkan tr_local.h)"
fi

echo ""
echo "Forward+ tile cull: MAX_LIGHTS vs MAX_DLIGHTS (packed light index range):"
TR_TYPES="$PROJECT_ROOT/src/renderers/common/tr_types.h"
FP_COMP="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp"
max_dl="$(sed -n 's/^#define[[:space:]]*MAX_DLIGHTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TR_TYPES" | head -1)"
max_sh="$(sed -n 's/^#define MAX_LIGHTS[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_COMP" | head -1)"
if [[ -z "$max_dl" || -z "$max_sh" ]]; then
  fail "could not parse MAX_DLIGHTS from tr_types.h or MAX_LIGHTS from forward_plus_tile_cull.comp"
elif [[ "$max_sh" != "$max_dl" ]]; then
  fail "forward_plus_tile_cull MAX_LIGHTS ($max_sh) != MAX_DLIGHTS ($max_dl) - tile cull and dlight index cap disagree"
else
  pass "forward_plus_tile_cull MAX_LIGHTS=$max_sh matches MAX_DLIGHTS"
fi

echo ""
echo "Forward+ tile cull: MAX_PER_TILE vs VK_FP_MAX_PER_TILE (tile SSBO stride):"
FP_C="$PROJECT_ROOT/src/renderers/vulkan/vk_forward_plus.c"
max_tile_sh="$(sed -n 's/^#define MAX_PER_TILE[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_COMP" | head -1)"
max_tile_c="$(sed -n 's/^#define VK_FP_MAX_PER_TILE[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_C" | head -1)"
if [[ -z "$max_tile_sh" || -z "$max_tile_c" ]]; then
  fail "could not parse MAX_PER_TILE from forward_plus_tile_cull.comp or VK_FP_MAX_PER_TILE from vk_forward_plus.c"
elif [[ "$max_tile_sh" != "$max_tile_c" ]]; then
  fail "MAX_PER_TILE ($max_tile_sh) != VK_FP_MAX_PER_TILE ($max_tile_c) - compute vs host tile layout disagree"
else
  pass "MAX_PER_TILE=$max_tile_sh matches VK_FP_MAX_PER_TILE"
fi

echo ""
echo "Forward+ tile cap: VK_FP_MIN_PER_TILE vs MAX_PER_TILE (shader slot layout):"
min_tile_c="$(sed -n 's/^#define VK_FP_MIN_PER_TILE[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$FP_C" | head -1)"
if [[ -z "$min_tile_c" || -z "$max_tile_sh" ]]; then
  fail "could not parse VK_FP_MIN_PER_TILE from vk_forward_plus.c or MAX_PER_TILE from forward_plus_tile_cull.comp"
elif [[ "$min_tile_c" -gt "$max_tile_sh" ]]; then
  fail "VK_FP_MIN_PER_TILE ($min_tile_c) > MAX_PER_TILE ($max_tile_sh) - r_forwardPlusMaxPerTile range would be empty"
else
  pass "VK_FP_MIN_PER_TILE=$min_tile_c <= MAX_PER_TILE=$max_tile_sh"
fi

echo ""
TR_INIT_VK="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
if ! grep -q 'vk_forward_plus_get_min_per_tile_cap' "$TR_INIT_VK" || ! grep -q 'vk_forward_plus_get_max_per_tile_cap' "$TR_INIT_VK"; then
  fail "vulkan/tr_init.c should use vk_forward_plus_get_*_per_tile_cap for r_forwardPlusMaxPerTile CheckRange"
else
  pass "r_forwardPlusMaxPerTile CheckRange wired to vk_forward_plus tile caps"
fi

echo ""
if [ -n "${GAME_BASE:-}" ]; then
  echo "Optional game base: $GAME_BASE"
  ASSETS_LIST="${GAME_ASSETS_LIST:-$PROJECT_ROOT/docs/samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt}"
  req=0
  while IFS= read -r line || [ -n "$line" ]; do
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    req=$((req + 1))
    f="$GAME_BASE/$line"
    if [ -f "$f" ]; then
      pass "game asset: $line"
    else
      fail "missing game asset: $line (expected under GAME_BASE)"
    fi
  done < "$ASSETS_LIST"
  if [ "$req" -eq 0 ]; then
    echo "  (no uncommented paths in OPTIONAL_GAME_ASSETS.txt - add BSP paths to enforce)"
  fi
else
  echo "GAME_BASE unset - skipping optional BSP checks (uncomment paths in OPTIONAL_GAME_ASSETS.txt to enforce in CI)."
fi

echo ""
echo "=== Summary ==="
echo "  Passed: $PASS  Failed: $FAIL"
if [ "$FAIL" -gt 0 ]; then
  echo "RENDERER REGRESSION CHECK FAILED" >&2
  exit 1
fi
echo "RENDERER REGRESSION CHECK PASSED"
exit 0
