#!/usr/bin/env bash
# Static gate: IQ live cert lab/readback/snapshot API contract.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "PASS: $*"; }

need() { [[ -f "$1" ]] || fail "missing $1"; }

need "$ROOT/renderers/vulkan/vk_iq_lab.c"
need "$ROOT/renderers/vulkan/vk_iq_cert_geometry.c"
need "$ROOT/renderers/vulkan/vk_renderer_p1_cert.c"
need "$ROOT/config/demo_iq_certify_core.cfg"
need "$ROOT/docs/RENDERER_IQ_LIVE_CERTIFICATION.md"

grep -q 'iq_certify_core' "$ROOT/renderers/vulkan/vk_iq_lab.c" || fail 'iq_certify_core command'
grep -q 'iq_lab_run' "$ROOT/renderers/vulkan/vk_iq_lab.c" || fail 'iq_lab_run'
grep -q 'vk_cert_readback_record_iq_snapshot' "$ROOT/renderers/vulkan/vk_cert_readback.h" || fail 'record_iq_snapshot API'
grep -q 'vk_cert_readback_finalize_iq_snapshot' "$ROOT/renderers/vulkan/vk_cert_readback.h" || fail 'finalize_iq_snapshot API'
grep -q 'CERT_RB_BLOOM_EXTRACT' "$ROOT/renderers/vulkan/vk_cert_readback.h" || fail 'BLOOM_EXTRACT resource'
grep -q 'vk_iq_lab_finalize_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || fail 'frame_submit IQ finalize'
grep -q 'vk_iq_lab_on_bloom_extract' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail 'bloom extract hook'
grep -q 'r_iqCertIsolate' "$ROOT/renderers/vulkan/vk_iq_lab.c" || fail 'r_iqCertIsolate'
grep -qi 'STATIC.*IMAGE_QUALITY\|PROFILE_CERTIFIED' "$ROOT/docs/RENDERER_IQ_LIVE_CERTIFICATION.md" || \
	fail 'live cert doc honesty'
pass 'IQ live cert contract'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All iq_live_cert_contract checks passed."
