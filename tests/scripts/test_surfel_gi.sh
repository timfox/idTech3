#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

SGI="renderers/vulkan/extensions/rtx/vk_surfel_gi.c"
REG_H="renderers/vulkan/vk_pass_registry.h"
REG_C="renderers/vulkan/vk_pass_registry.c"
RG_C="renderers/vulkan/vk_render_graph.c"
STABLE="config/modern_vulkan_stable.cfg"
RT="config/modern_vulkan_rt.cfg"
RTX="config/vulkan_overlay_rtx.cfg"
HYBRID="config/vulkan_overlay_hybrid1.cfg"
SAFE="config/gfx_safe.cfg"
DOC="docs/SURFEL_GI.md"

test -f "$SGI" || fail "vk_surfel_gi.c missing"
test -f "$DOC" || fail "SURFEL_GI docs missing"
test -f renderers/vulkan/shaders/glsl/surfel_gi/surfel_spawn.comp || fail "surfel spawn shader missing"
test -f renderers/vulkan/shaders/glsl/surfel_gi/surfel_update.comp || fail "surfel update shader missing"
test -f renderers/vulkan/shaders/glsl/surfel_gi/surfel_hash.comp || fail "surfel hash shader missing"
test -f renderers/vulkan/shaders/glsl/surfel_gi/surfel_resolve.comp || fail "surfel resolve shader missing"
test -f renderers/vulkan/shaders/glsl/surfel_gi/surfel_composite.comp || fail "surfel composite shader missing"

rg -q 'r_surfelGi = ri.Cvar_Get\( "r_surfelGi", "1"' "$SGI" || fail "Surfel GI cvar must default on"
rg -q 'ri.Cvar_Get\( "r_surfelGi", "1"' "$SGI" || fail "non-RTX stub must preserve default-on cvar"
rg -q 'vk_surfel_gi_active' "$SGI" || fail "active helper missing"
rg -q 'vk.rayQueryAvailable' "$SGI" || fail "ray-query availability gate missing"
rg -q 'vk_rtx_scene_ready' "$SGI" || fail "TLAS readiness gate missing"
rg -q 'vk_rtx_bind_world_albedo_ssbo' "$SGI" || fail "world albedo SSBO binding missing"
rg -q 'vk_rtx_bind_entity_albedo_ssbo' "$SGI" || fail "entity albedo SSBO binding missing"
rg -q 'vk_ambient_visibility_view' "$SGI" || fail "Ambient Visibility coupling missing"
rg -q 'vk_surfel_gi_hybrid1_fusion_active' "$SGI" || fail "Hybrid1 fusion helper missing"
rg -q 'surfel_gi_status' "$SGI" || fail "status command missing"

for res in SURFEL_POOL SURFEL_HASH SURFEL_IRRADIANCE; do
	rg -q "VK_SPINE_RES_${res}" "$REG_H" || fail "missing spine resource $res"
done
for pass in SURFEL_GI_UPDATE SURFEL_GI_HASH SURFEL_GI_RESOLVE SURFEL_GI_COMPOSITE; do
	rg -q "VK_SPINE_PASS_${pass}" "$REG_H" || fail "missing spine pass $pass"
	rg -q "VK_SPINE_PASS_${pass}" "$REG_C" || fail "spine pass $pass not declared"
done
rg -Fq 'vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_UPDATE' "$SGI" || fail "Surfel update pass not observed"
rg -Fq 'vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_HASH' "$SGI" || fail "Surfel hash pass not observed"
rg -Fq 'vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_RESOLVE' "$SGI" || fail "Surfel resolve pass not observed"
rg -Fq 'vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_COMPOSITE' "$SGI" || fail "Surfel composite pass not observed"
rg -q 'VK_SPINE_RES_SURFEL_IRRADIANCE' "$RG_C" || fail "render graph must import optional Surfel irradiance"

rg -q 'seta r_surfelGi 1' "$STABLE" || fail "stable profile must request Surfel GI by default"
rg -q 'seta r_surfelGiDensity 2' "$STABLE" || fail "stable profile must use balanced Surfel density"
rg -q 'seta r_surfelGi_hybrid1Fusion 1' "$STABLE" || fail "stable profile must enable Surfel fusion"
rg -q 'seta r_surfelGi 1' "$RT" || fail "RT profile must enable Surfel GI"
rg -q 'seta r_surfelGi 1' "$RTX" || fail "RTX overlay must enable Surfel GI"
rg -q 'seta r_surfelGi 1' "$HYBRID" || fail "Hybrid1 overlay must enable Surfel GI"
rg -q 'seta r_surfelGi 0' "$SAFE" || fail "safe profile must disable Surfel GI"
rg -q 'default indirect RT request' "$STABLE" || fail "stable profile must document Surfel fail-open default"
rg -q 'Production default' "$DOC" || fail "docs must state production default"

echo "PASS: test_surfel_gi.sh"
