#!/usr/bin/env bash
# Foundation Consolidation master runner — static grep gates (no GPU required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT_DIR="$(dirname "$0")"

TESTS=(
	test_renderer_frame_contract.sh
	test_gpu_scene_layout.sh
	test_gpu_draw_parity.sh
	test_hiz_reversed_z.sh
	test_gpu_scene_generation.sh
	test_gpu_object_identity.sh
	test_gpu_frustum_culling.sh
	test_gpu_occlusion_culling.sh
	test_indirect_command_bounds.sh
	test_indirect_stale_prevention.sh
	test_gpu_render_path_buckets.sh
	test_gpu_dynamic_object_lifecycle.sh
	test_gpu_capacity_fallback.sh
	test_gpu_frame_ownership.sh
	test_gbuffer_layout.sh
	test_material_routing.sh
	test_brdf_parity.sh
	test_specular_aa.sh
	test_shadow_consumer_parity.sh
	test_reflection_fallback.sh
	test_indirect_light_parity.sh
	test_hdr_composition.sh
	test_color_pipeline_contract.sh
	test_oit_contract.sh
	test_oit_alpha_contract.sh
	test_depth_contract.sh
	test_oit_view_depth.sh
	test_hdr_resolve_integrity.sh
	test_oit_weight_contract.sh
	test_renderer_iq_p0.sh
	test_renderer_iq_p1.sh
	test_renderer_p1_certification.sh
	test_renderer_p1_cert_levels.sh
	test_iq_live_cert_contract.sh
	test_iq_firefly_metrics.sh
	test_iq_edge_metrics.sh
	test_iq_temporal_history_live.sh
	test_p1_live_state_machine.sh
	test_p1_live_preflight.sh
	test_p1_fixture_visibility.sh
	test_p1_readback_identity.sh
	test_p1_bloom_source_live.sh
	test_p1_firefly_live.sh
	test_p1_bloom_pyramid_live.sh
	test_p1_velocity_live.sh
	test_p1_temporal_reset_live.sh
	test_p1_ghosting_attribution_live.sh
	test_p1_specular_stability_live.sh
	test_p1_normal_mip_live.sh
	test_p1_gbuffer_quantization_live.sh
	test_p1_material_decode_live.sh
	test_p1_lighting_parity_live.sh
	test_p1_lighting_ownership_live.sh
	test_p1_cluster_parity_live.sh
	test_p1_edge_quality_live.sh
	test_p1_smaa_live.sh
	test_p1_msaa_oit_live.sh
	test_p1_texture_lod_live.sh
	test_p1_lifecycle_live.sh
	test_p1_evidence_invalidation.sh
	test_p1_failure_bundles.sh
	test_p1_final_promotion.sh
	test_wboit_production_certification.sh
	test_wboit_cert_evidence.sh
	test_renderer_lab_capture.sh
	test_deferred_honesty.sh
	test_deferred_eligibility.sh
	test_deferred_mixed_material.sh
	test_deferred_architecture_modes.sh
	test_gbuffer_true_base_color.sh
	test_deferred_lightmap_term.sh
	test_deferred_no_double_shading.sh
	test_deferred_mixed_composite.sh
	test_deferred_lighting_parity.sh
)

failures=0
for t in "${TESTS[@]}"; do
	path="$SCRIPT_DIR/$t"
	if [[ ! -x "$path" ]]; then
		echo "FAIL: $t not executable or missing"
		failures=$((failures + 1))
		continue
	fi
	echo "==> running $t"
	if ! "$path"; then
		failures=$((failures + 1))
	fi
done

echo ""
if [[ $failures -ne 0 ]]; then
	echo "Foundation Consolidation: $failures test script(s) failed"
	exit 1
fi
echo "Foundation Consolidation: all ${#TESTS[@]} test scripts passed."
