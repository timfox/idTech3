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
	test_gbuffer_layout.sh
	test_material_routing.sh
	test_brdf_parity.sh
	test_specular_aa.sh
	test_shadow_consumer_parity.sh
	test_reflection_fallback.sh
	test_indirect_light_parity.sh
	test_hdr_composition.sh
	test_renderer_lab_capture.sh
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
