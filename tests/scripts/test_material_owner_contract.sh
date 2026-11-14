#!/usr/bin/env bash
# Static contract for the material fields shared by every production owner.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }
need() { grep -q "$2" "$1" || fail "$3"; }

DEC="$ROOT/renderers/vulkan/shaders/glsl/surface_material_decode.glsl"
need "$DEC" 'SurfaceMaterialDecodeCanonical' 'canonical material decoder missing'
need "$DEC" 'SurfaceMaterialDecodeLegacy' 'legacy material decoder missing'
need "$DEC" 'SurfaceAlphaTestPass' 'shared alpha-test comparison missing'
need "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" 'SurfaceAlphaTestPassLegacy' 'Forward+/deferred alpha-test semantics are not shared'
need "$ROOT/renderers/vulkan/shaders/glsl/light_frag.tmpl" 'SurfaceAlphaTestPassLegacy' 'legacy light alpha-test semantics are not shared'
need "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" 'lightmap_decode.glsl' 'deferred lightmap contract missing'
need "$ROOT/renderers/vulkan/shaders/glsl/forward_plus_light_eval.glsl" 'SurfaceMaterialDecodeCanonical' 'Forward+ material seam missing'
need "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" 'SurfaceMaterialDecodeCanonical' 'WBOIT physical material seam missing'
need "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" 'normalMap' 'WBOIT normal binding missing'
need "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" 'emissiveMap' 'WBOIT emissive binding missing'
need "$ROOT/renderers/vulkan/shaders/glsl/ssr.frag" 'normalTexture' 'SSR GBuffer normal binding missing'
need "$ROOT/renderers/vulkan/shaders/glsl/ssr.frag" 'materialTexture' 'SSR GBuffer material binding missing'
need "$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl" 'SurfaceMaterialDecodeLegacy' 'Hybrid1 material seam missing'
need "$ROOT/renderers/vulkan/shaders/glsl/pt_hit.rchit" 'SurfaceMaterialDecodeLegacy' 'path-trace material seam missing'
need "$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_shadow.rahit" 'SurfaceAlphaTestPass' 'RTX alpha-test semantics are not shared'
need "$ROOT/renderers/vulkan/vk_init_device.c" 'desc.setLayoutCount = 8' 'WBOIT material descriptor layout missing'
need "$ROOT/renderers/vulkan/vk_init_device.c" 'desc.setLayoutCount = 10' 'MBOIT material descriptor layout missing'

echo "Material owner contract passed (base/normal/roughness/metal/AO/emissive/alpha/lightmap/legacy)."
