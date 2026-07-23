#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
c="$ROOT/renderers/vulkan/vk_renderer_p1_cert.c"
grep -q 'P1_CERT_STAGE_BLOOM_PYRAMID' "$c"
grep -q 'P1_CERT_STAGE_LIFECYCLE' "$c"
grep -q 'IMAGE_QUALITY_CERTIFIED' "$c"
grep -q 'anyManual' "$c"
grep -qi 'cvars alone\|PROFILE_CERTIFIED' "$ROOT/docs/RENDERER_P1_CERTIFICATION.md"
# Must not grant IMAGE_QUALITY from fail==0 gate checklist
if grep -n 'IMAGE_QUALITY_CERTIFIED' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c" | grep -q 'fail == 0'; then
	echo "FAIL: dishonest promotion"; exit 1
fi
echo "All p1_final_promotion checks passed."
