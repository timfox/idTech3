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
    line="${line%$'\r'}"
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
echo "glTF GPU tangent topo: GLTF_GPU_ADJ_TRIS_MAX (C vs gen_vert.tmpl):"
TOPO_H="$PROJECT_ROOT/src/renderers/vulkan/tr_gltf_topo.h"
GEN_VERT="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl/gen_vert.tmpl"
adj_c="$(sed -n 's/^#define GLTF_GPU_ADJ_TRIS_MAX[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TOPO_H" | head -1)"
adj_glsl="$(grep -E 'const uint GLTF_GPU_ADJ_TRIS_MAX' "$GEN_VERT" | head -1 | sed -n 's/.*GLTF_GPU_ADJ_TRIS_MAX = \([0-9][0-9]*\)u.*/\1/p')"
if [[ -z "$adj_c" || -z "$adj_glsl" ]]; then
  fail "could not parse GLTF_GPU_ADJ_TRIS_MAX from tr_gltf_topo.h or gen_vert.tmpl"
elif [[ "$adj_c" != "$adj_glsl" ]]; then
  fail "GLTF_GPU_ADJ_TRIS_MAX mismatch: C=$adj_c GLSL=$adj_glsl"
else
  pass "GLTF_GPU_ADJ_TRIS_MAX=$adj_c (C + GLSL)"
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
echo "PBR fragment spec: Forward+ shade constant_id vs vk_create_pipeline.c:"
GEN_FRAG="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
VK_PIPE="$PROJECT_ROOT/src/renderers/vulkan/vk_create_pipeline.c"
fp_cid="$(grep -F 'forward_plus_shade_strength' "$GEN_FRAG" | grep 'constant_id' | sed -n 's/.*constant_id[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' | head -1)"
fp_add="$(grep 'forward_plus_shade_strength' "$VK_PIPE" | grep 'ADD_FRAG_SPEC' | sed -n 's/.*ADD_FRAG_SPEC([[:space:]]*\([0-9][0-9]*\)[[:space:]]*,[[:space:]]*forward_plus_shade_strength.*/\1/p' | head -1)"
if [[ -z "$fp_cid" || -z "$fp_add" ]]; then
  fail "could not parse forward_plus_shade_strength constant_id from gen_frag.tmpl or ADD_FRAG_SPEC from vk_create_pipeline.c"
elif [[ "$fp_cid" != "$fp_add" ]]; then
  fail "Forward+ shade specialization mismatch: gen_frag.tmpl constant_id=$fp_cid vs vk_create_pipeline.c ADD_FRAG_SPEC=$fp_add"
else
  pass "forward_plus_shade_strength uses constant_id=$fp_cid (shader + pipeline agree)"
fi

echo ""
echo "Forward+ tile cull: sphere overlap uses viewport-derived tile pixels (not hard-coded 16px):"
if grep -q 'tileX \* 16u\|tileY \* 16u' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must not hard-code 16px tiles; derive tilePx from viewport / tileGrid (see vk_forward_plus VK_FP_TILE_DIM)"
elif ! grep -q 'tilePxX' "$FP_COMP" 2>/dev/null || ! grep -q 'sphere_tile_overlap' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp expected tilePxX/tilePxY sphere_tile_overlap path"
else
  fp_dim="$(sed -n 's/^#define VK_FP_TILE_DIM[[:space:]]*\([0-9][0-9]*\)u*$/\1/p' "$FP_C" | head -1)"
  pass "forward_plus_tile_cull uses dynamic tile pixels (VK_FP_TILE_DIM=$fp_dim on host grid)"
fi

echo ""
echo "Forward+ PBR fragment: tile SSBO stride (tileId * N) matches MAX_PER_TILE:"
fp_stride="$(grep -E 'tileId \* [0-9]+u' "$GEN_FRAG" 2>/dev/null | sed -n 's/.*tileId \* \([0-9][0-9]*\)u.*/\1/p' | head -1)"
if [[ -z "$fp_stride" ]]; then
  fail "gen_frag.tmpl: expected tileId * <N>u for Forward+ tile base (fp_tiles stride)"
elif [[ -z "$max_tile_sh" ]]; then
  fail "could not re-use MAX_PER_TILE from forward_plus_tile_cull.comp for gen_frag stride check"
elif [[ "$fp_stride" != "$max_tile_sh" ]]; then
  fail "gen_frag.tmpl tile stride uses * ${fp_stride}u but MAX_PER_TILE=$max_tile_sh - tile SSBO layout vs fragment disagree"
else
  pass "gen_frag.tmpl tile stride * ${fp_stride}u matches MAX_PER_TILE"
fi

echo ""
echo "Forward+ SSBO: light header + record vec4 counts (vk_forward_plus.c vs tile_cull.comp vs gen_frag.tmpl):"
rec_stride_f="$(sed -n 's/^#define VK_FP_RECORD_STRIDE *(sizeof(float) *\*[[:space:]]*\([0-9][0-9]*\)).*$/\1/p' "$FP_C" | head -1)"
hdr_f="$(sed -n 's/^#define VK_FP_HEADER_BYTES *(sizeof(float) *\*[[:space:]]*\([0-9][0-9]*\)).*$/\1/p' "$FP_C" | head -1)"
rec_vec4s="$(sed -n 's/^#define REC_VEC4S[[:space:]]*\([0-9][0-9]*\)u*$/\1/p' "$FP_COMP" | head -1)"
if [[ -z "$rec_stride_f" || -z "$hdr_f" || -z "$rec_vec4s" ]]; then
  fail "could not parse VK_FP_RECORD_STRIDE / VK_FP_HEADER_BYTES from vk_forward_plus.c or REC_VEC4S from forward_plus_tile_cull.comp"
else
  exp_stride=$(( rec_vec4s * 4 ))
  if [[ "$rec_stride_f" != "$exp_stride" ]]; then
    fail "VK_FP_RECORD_STRIDE packs $rec_stride_f floats but REC_VEC4S=$rec_vec4s implies ${exp_stride} (4 floats per vec4)"
  elif [[ "$hdr_f" != "8" ]]; then
    fail "VK_FP_HEADER_BYTES uses sizeof(float)*$hdr_f; expected 8 (two header vec4s: lights.data[0..1])"
  else
    pass "Forward+ light SSBO: header ${hdr_f}f, record ${rec_stride_f}f (=REC_VEC4S $rec_vec4s * 4)"
  fi
fi
fp_frag_uniq="$(grep -E '2u \+ li \* [0-9]+u' "$GEN_FRAG" 2>/dev/null | sed -n 's/.*\* \([0-9][0-9]*\)u.*/\1/p' | sort -u | tr '\n' ' ' | xargs)"
if [[ -z "$fp_frag_uniq" ]]; then
  fail "gen_frag.tmpl: missing Forward+ light base index pattern '2u + li * <N>u'"
elif [[ "$(grep -E '2u \+ li \* [0-9]+u' "$GEN_FRAG" 2>/dev/null | sed -n 's/.*\* \([0-9][0-9]*\)u.*/\1/p' | sort -u | wc -l | tr -d ' ')" != "1" ]]; then
  fail "gen_frag.tmpl: multiple distinct '2u + li * Nu' multipliers ($fp_frag_uniq) — Forward+ record layout drift"
elif [[ "$fp_frag_uniq" != "$rec_vec4s" ]]; then
  fail "gen_frag.tmpl light record stride multiplier ($fp_frag_uniq) != REC_VEC4S ($rec_vec4s)"
else
  pass "gen_frag.tmpl Forward+ light base uses li * ${fp_frag_uniq}u (matches REC_VEC4S)"
fi

echo ""
echo "Forward+ tile cull: push constants (depthCull) host vs compute shader:"
if ! grep -q 'depth_cull' "$FP_C" 2>/dev/null || ! grep -q 'depthCull' "$FP_COMP" 2>/dev/null; then
  fail "vk_forward_plus.c vk_fp_push_t and forward_plus_tile_cull.comp Push must both declare depth_cull / depthCull"
elif ! grep -q 'push\.depth_cull = use_depth_cull' "$FP_C" 2>/dev/null; then
  fail "vk_forward_plus_dispatch_tile_cull_internal must assign push.depth_cull from use_depth_cull"
elif ! grep -q 'binding = 3' "$FP_COMP" 2>/dev/null || ! grep -q 'depthTexture' "$FP_COMP" 2>/dev/null; then
  fail "forward_plus_tile_cull.comp must declare depthTexture on binding 3 for r_forwardPlusDepthCull"
elif ! grep -q 'binds\[3\].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER' "$FP_C" 2>/dev/null; then
  fail "vk_forward_plus_create_set_layout must include binding 3 depth sampler for compute"
else
  pass "Forward+ depth cull: push depthCull + binding 3 depth sampler wired"
fi

echo ""
echo "Volumetric fog compute: VDB binding 17 + params (host vs volumetric_fog.comp):"
VF_COMP="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl/volumetric/volumetric_fog.comp"
VK_INIT="$PROJECT_ROOT/src/renderers/vulkan/vk_init_device.c"
VK_VOL_P="$PROJECT_ROOT/src/renderers/vulkan/vk_volumetric_params.h"
if [[ ! -f "$VF_COMP" ]]; then
  fail "missing volumetric_fog.comp"
elif ! grep -q 'binding = 17' "$VF_COMP" 2>/dev/null || ! grep -q 'vdbFogDensity' "$VF_COMP" 2>/dev/null; then
  fail "volumetric_fog.comp must declare sampler3D vdbFogDensity on binding 17"
elif ! grep -q 'compute_bindings\[18\]' "$VK_INIT" 2>/dev/null || ! grep -q 'compute_bindings\[17\].binding = 17' "$VK_INIT" 2>/dev/null; then
  fail "vk_init_device.c volumetric compute layout must include binding 17 (18 bindings total)"
elif ! grep -q 'vdbParams\[4\]' "$VK_VOL_P" 2>/dev/null || ! grep -q 'vdbWorldMin\[4\]' "$VK_VOL_P" 2>/dev/null; then
  fail "volumetric_params_t must include vdbParams / vdbWorldMin / vdbWorldMax for r_vdbFog"
else
  pass "VDB volumetric fog: binding 17 + UBO vdb fields present"
fi

echo ""
echo "Vulkan temporal: reset bitmask vs reason_string / log table:"
VK_TEMP_H="$PROJECT_ROOT/src/renderers/vulkan/vk_temporal.h"
VK_TEMP_C="$PROJECT_ROOT/src/renderers/vulkan/vk_temporal.c"
# One line per 1u<<N reset flag in the public enum (excludes VK_TEMPORAL_RESET_NONE = 0).
n_enum="$(grep -E '^[[:space:]]*VK_TEMPORAL_RESET_[A-Z0-9_]+[[:space:]]*=[[:space:]]*1u[[:space:]]*<<' "$VK_TEMP_H" 2>/dev/null | wc -l | tr -d ' ')"
n_case="$(grep -E '^[[:space:]]*case VK_TEMPORAL_RESET_' "$VK_TEMP_C" 2>/dev/null | wc -l | tr -d ' ')"
n_arr="$(awk '/knownReasons\[\][[:space:]]*=[[:space:]]*\{/,/\}[[:space:]]*;/' "$VK_TEMP_C" 2>/dev/null | grep -c 'VK_TEMPORAL_RESET_' | tr -d ' ')"
if [[ -z "$n_enum" || "$n_enum" -eq 0 ]]; then
  fail "could not count VK_TEMPORAL_RESET_* 1u<< lines in vk_temporal.h"
elif [[ "$n_enum" != "$n_case" ]]; then
  fail "vk_temporal reset count mismatch: enum 1u<< lines=$n_enum vs switch cases=$n_case in vk_temporal.c"
elif [[ "$n_enum" != "$n_arr" ]]; then
  fail "vk_temporal reset count mismatch: enum 1u<< lines=$n_enum vs knownReasons[] entries=$n_arr in vk_temporal.c"
else
  pass "vk_temporal: $n_enum reset flags, $n_case switch cases, $n_arr knownReasons entries"
fi

echo ""
echo "Vegetation wind dispatch ordering (staging must be populated before compute):"
TR_SHADE="$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c"
VK_FRAME_SUBMIT="$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c"
if awk '
  /PostFX_VegWind_IsEnabled\(\) && tess\.shader && \( tess\.shader->surfaceFlags & SURF_VEGETATION \)/ { guard=1 }
  /vk_vegetation_wind_prepare_draw\(\);/ { prepare=1 }
  END { exit !(guard && prepare) }
' "$TR_SHADE"; then
  pass "tr_shade.c prepares vegetation wind from SURF_VEGETATION batches before draw"
else
  fail "tr_shade.c is missing SURF_VEGETATION-gated vk_vegetation_wind_prepare_draw()"
fi
if grep -q 'vk_vegetation_wind_prepare_draw();' "$VK_FRAME_SUBMIT"; then
  fail "vk_frame_submit.c should not prepare vegetation wind at frame start"
else
  pass "vk_frame_submit.c has no direct veg-wind prepare call"
fi

echo ""
echo "Vulkan mesh-shader extension gating (startup safety):"
TR_INIT_VK="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"
if grep -Fq 'r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0"' "$TR_INIT_VK"; then
  pass "r_vk_meshShaderNV cvar registered with default 0"
else
  fail "missing r_vk_meshShaderNV cvar registration with default 0 in tr_init.c"
fi

if grep -Fq 'vk.meshShaderNV = qfalse;' "$VK_INSTANCE"; then
  pass "vk.meshShaderNV reset each device creation"
else
  fail "vk.meshShaderNV is not reset before extension selection"
fi

if grep -Fq 'if ( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&' "$VK_INSTANCE"; then
  pass "VK_NV_mesh_shader enable path gated by support + cvar"
else
  fail "VK_NV_mesh_shader enable path is missing support/cvar gating"
fi

if grep -Fq 'device_extension_count < ARRAY_LEN( device_extension_list ) ) {' "$VK_INSTANCE"; then
  pass "VK_NV_mesh_shader enable path guards extension-list capacity"
else
  fail "VK_NV_mesh_shader enable path missing extension-list capacity guard"
fi

if grep -Fq 'mesh_shader_features_nv.meshShader = VK_TRUE;' "$VK_INSTANCE" && \
   grep -Fq 'mesh_shader_features_nv.pNext = (void *)(uintptr_t)device_desc.pNext;' "$VK_INSTANCE"; then
  pass "mesh-shader feature struct is chained to current device_desc.pNext head"
else
  fail "mesh-shader feature chain setup is missing required pNext/meshShader assignments"
fi

echo ""
echo "glTF topo: GLTF_GPU_TOPO_WORDS_PER_VERT macro (tr_gltf_topo.h vs gen_vert.tmpl formula):"
TOPO_H="$PROJECT_ROOT/src/renderers/vulkan/tr_gltf_topo.h"
if ! grep 'GLTF_GPU_TOPO_WORDS_PER_VERT' "$TOPO_H" | grep -q '1.*+.*GLTF_GPU_ADJ_TRIS_MAX'; then
  fail "tr_gltf_topo.h: GLTF_GPU_TOPO_WORDS_PER_VERT must expand to (1 + GLTF_GPU_ADJ_TRIS_MAX)"
elif ! grep -q 'const uint GLTF_GPU_TOPO_WORDS_PER_VERT = 1u + GLTF_GPU_ADJ_TRIS_MAX' "$GEN_VERT"; then
  fail "gen_vert.tmpl: GLTF_GPU_TOPO_WORDS_PER_VERT must be 1u + GLTF_GPU_ADJ_TRIS_MAX (match tr_gltf_topo.h)"
else
  pass "GLTF_GPU_TOPO_WORDS_PER_VERT = 1 + ADJ (C header + GLSL)"
fi

echo ""
echo "glTF topo: GLTF_GPU_PULL_UINTS_PER_VERT (tr_gltf_topo.h vs gen_vert.tmpl):"
pull_c="$(sed -n 's/^#define GLTF_GPU_PULL_UINTS_PER_VERT[[:space:]]*\([0-9][0-9]*\).*$/\1/p' "$TOPO_H" | head -1)"
pull_glsl="$(grep -E 'const uint GLTF_GPU_PULL_UINTS_PER_VERT' "$GEN_VERT" | head -1 | sed -n 's/.*GLTF_GPU_PULL_UINTS_PER_VERT = \([0-9][0-9]*\)u.*/\1/p')"
if [[ -z "$pull_c" || -z "$pull_glsl" ]]; then
  fail "could not parse GLTF_GPU_PULL_UINTS_PER_VERT from tr_gltf_topo.h or gen_vert.tmpl"
elif [[ "$pull_c" != "$pull_glsl" ]]; then
  fail "GLTF_GPU_PULL_UINTS_PER_VERT mismatch: C=$pull_c GLSL=$pull_glsl"
else
  pass "GLTF_GPU_PULL_UINTS_PER_VERT=$pull_c (C + GLSL)"
fi

echo ""
if [ -n "${GAME_BASE:-}" ]; then
  echo "Optional game base: $GAME_BASE"
  ASSETS_LIST="${GAME_ASSETS_LIST:-$PROJECT_ROOT/docs/samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt}"
  req=0
  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%$'\r'}"
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
