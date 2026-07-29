#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CHECK="${1:?check name required}"
CERT="$ROOT/renderers/vulkan/vk_deferred_certification.c"
CERTH="$ROOT/renderers/vulkan/vk_deferred_certification.h"
HONEST="$ROOT/renderers/vulkan/vk_deferred_honesty.c"
HONESTH="$ROOT/renderers/vulkan/vk_deferred_honesty.h"
GLSL="$ROOT/renderers/vulkan/shaders/glsl"
fail() { echo "FAIL [$CHECK]: $*" >&2; exit 1; }
has() { grep -q "$2" "$1" || fail "$3"; }

case "$CHECK" in
gbuffer_full_fidelity)
	has "$ROOT/config/modern_raster_iq_reference.cfg" 'r_gbufferQuality 2' "IQ quality"
	has "$GLSL/deferred_lighting_common.glsl" 'surface_material_decode.glsl' "canonical decode"
	;;
gbuffer_metadata_explicit)
	has "$ROOT/docs/GBUFFER_FULL_FIDELITY.md" 'SurfaceData.a' "explicit ownership"
	! grep -q 'DEFERRED_OWNER_BIAS' "$GLSL/deferred_lighting_common.glsl" ||
		fail "shader still uses bias ownership"
	;;
opaque_lighting_ownership)
	has "$HONESTH" 'OPAQUE_OWNER_INVALID' "owner enum"
	has "$CERT" 'invalidOwnerPixels == 0u' "invalid gate"
	;;
material_decode_parity)
	has "$GLSL/surface_material_decode.glsl" 'struct SurfaceMaterial' "SurfaceMaterial"
	has "$GLSL/forward_plus_light_eval.glsl" 'SurfaceMaterialDecodeCanonical' "Forward decode"
	has "$GLSL/deferred_lighting_common.glsl" 'SurfaceMaterialDecodeCanonical' "Deferred decode"
	;;
brdf_shared)
	has "$GLSL/forward_plus_light_eval.glsl" 'pbr_brdf_core.glsl' "Forward BRDF"
	has "$GLSL/deferred_lighting_common.glsl" 'pbr_brdf_core.glsl' "Deferred BRDF"
	;;
normal_tangent_parity)
	has "$GLSL/surface_material_decode.glsl" 'normalWS' "normal boundary"
	has "$GLSL/deferred_lighting_common.glsl" 'SampleDeferredNormal' "Deferred normal decode"
	;;
cluster_authority)
	has "$CERTH" 'clusterLightingFrame_s' "cluster frame record"
	has "$CERT" 'vk_cluster_list_generation' "shared generation"
	;;
deferred_direct_light_parity)
	has "$GLSL/deferred_lighting_common.glsl" 'attenPointLight' "point light"
	has "$GLSL/deferred_lighting_common.glsl" 'attenSpotLight' "spot light"
	has "$GLSL/deferred_lighting_common.glsl" 'sunDiffuse' "sun light"
	;;
deferred_shadow_parity)
	has "$GLSL/deferred_lighting_common.glsl" 'DEFERRED_HAS_SHADOW_CONTRACT' "shadow contract"
	has "$ROOT/docs/DEFERRED_SHADOWS.md" 'scene-linear' "shadow evidence"
	;;
deferred_lightmap_parity|deferred_deluxe_parity)
	has "$GLSL/deferred_lighting_common.glsl" 'lightmapIrr' "lightmap term"
	has "$GLSL/lightmap_decode.glsl" 'DeferredStaticDiffuseFromDeluxeApprox' "deluxe approximation"
	has "$GLSL/deferred_lighting_common.glsl" 'lightmapMode' "lightmap mode switch"
	has "$ROOT/docs/DEFERRED_LIGHTMAPS.md" 'Directional deluxe' "deluxe policy"
	;;
deferred_ibl_parity)
	has "$GLSL/deferred_lighting_common.glsl" 'DeferredEvalSkyIBL' "shared IBL inputs"
	;;
deferred_ao_ownership)
	has "$ROOT/docs/DEFERRED_AO_OWNERSHIP.md" 'do not darken' "AO policy"
	has "$GLSL/deferred_lighting_common.glsl" 'materialAO' "AO input"
	;;
deferred_emissive_ownership)
	has "$HONEST" 'No explicit full-fidelity emissive MRT yet' "emissive routing"
	has "$ROOT/docs/DEFERRED_EMISSIVE_OWNERSHIP.md" 'exactly once' "emissive policy"
	;;
deferred_owner_composite)
	has "$GLSL/deferred_lighting_composite.frag" 'mixedMaterial' "owner composite"
	has "$GLSL/deferred_lighting_composite.frag" 'owned' "replacement mask"
	;;
deferred_material_routing)
	has "$HONEST" 'DEFERRED_REASON_TRANSMISSION_OR_REFRACTION' "unsupported routing"
	has "$HONEST" 'DEFERRED_REASON_FORWARD_ONLY_POLICY' "Forward policy"
	;;
deferred_depth_parity)
	has "$CERT" 'vk_hdr_resolve_depth_generation' "depth generation"
	;;
deferred_no_fullbright_escape)
	has "$CERT" 'fullbrightEscapeCount == 0u' "fullbright hard gate"
	has "$ROOT/docs/OPAQUE_LIGHTING_OWNERSHIP.md" 'Raw albedo' "raw albedo policy"
	;;
deferred_forward_metrics)
	has "$CERT" 'PENDING_GPU_EVIDENCE' "honest GPU evidence gate"
	has "$ROOT/renderers/vulkan/vk_shading_compare.c" 'mean error (linear HDR)' "linear HDR metric"
	;;
deferred_contract)
	has "$CERTH" 'deferredRenderingContract_s' "versioned contract"
	has "$CERT" 'DeferredContractHash' "contract hash"
	;;
deferred_lifecycle)
	has "$CERT" 's_contractHashAtEvidence != s_contract.hash' "contract invalidation"
	has "$CERT" 's_gbufferGenerationAtEvidence' "G-buffer invalidation"
	has "$CERT" 's_clusterGenerationAtEvidence' "cluster invalidation"
	;;
deferred_production_certification)
	has "$CERTH" 'DEFERRED_PRODUCTION_CERTIFIED' "production level"
	has "$CERT" 'GPU HDR parity evidence required' "no static promotion"
	;;
*) fail "unknown check" ;;
esac
echo "PASS: $CHECK"
