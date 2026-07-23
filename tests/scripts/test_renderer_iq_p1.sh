#!/usr/bin/env bash
# Static gate: Renderer IQ P1 remediation foundation.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

need() { [[ -f "$1" ]] || fail "missing $1"; }

need "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c"
need "$ROOT/renderers/vulkan/vk_bloom_source_contract.c"
need "$ROOT/config/modern_raster_iq_reference.cfg"
need "$ROOT/docs/RENDERER_P1_CERTIFICATION.md"
need "$ROOT/docs/RENDERER_IQ_REFERENCE_PROFILE.md"
need "$ROOT/docs/BLOOM_SOURCE_INTEGRITY.md"
need "$ROOT/docs/BLOOM_FIREFLY_CONTROL.md"

grep -q 'modern_raster_iq_reference' "$ROOT/config/modern_raster_iq_reference.cfg" || fail 'profile name missing'
grep -q 'r_bloomFireflyClamp' "$ROOT/config/modern_raster_iq_reference.cfg" || fail 'firefly clamp in profile'
grep -q 'r_taa 0' "$ROOT/config/modern_raster_iq_reference.cfg" || fail 'profile must disable TAA'
grep -q 'r_gbufferCompact 0' "$ROOT/config/modern_raster_iq_reference.cfg" || fail 'profile full G-buffer'
pass 'IQ reference profile'

grep -q 'BLOOM_CONTRIB_WBOIT' "$ROOT/renderers/vulkan/vk_bloom_source_contract.h" || fail 'bloom contrib mask'
grep -q 'bloom_source_status' "$ROOT/renderers/vulkan/vk_bloom_source_contract.c" || fail 'bloom_source_status'
pass 'BloomSourceHDR contract'

grep -q 'firefly_clamp' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag" || fail 'firefly in bloom.frag'
grep -q 'robustNeighborhoodLuma' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag" || fail 'robust luma missing'
grep -q 'bloom_firefly_clamp' "$ROOT/renderers/vulkan/vk_post_process_pipeline.c" || fail 'firefly spec constants'
pass 'Bloom firefly control'

grep -q 'HISTORY_TAA' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.h" || fail 'history registry enum'
grep -q 'r_ghostIsolation' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c" || fail 'ghost isolation'
grep -q 'RENDERER_P1_IMAGE_QUALITY_CERTIFIED' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c" || fail 'P1 cert level'
pass 'History + ghost isolation + P1 cert hub'

grep -q 'roughMod = 1.0' "$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl" || \
	fail 'deferred roughMod seam not removed'
pass 'Deferred/Forward+ roughMod parity fix'

grep -q 'vk_renderer_iq_p1_register' "$ROOT/renderers/vulkan/tr_init.c" || fail 'P1 hub not registered'
grep -q 'vk_bloom_source_note_extract' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || fail 'extract not noting bloom source'
pass 'Runtime wiring'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All Renderer IQ P1 foundation checks passed."
