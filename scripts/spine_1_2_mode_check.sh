#!/usr/bin/env bash
# Spine 1.2 mode model static contracts (Selective Hybrid + PT Reference).
# Keeps boot on clustered raster; forbids FG / presentation latency policies.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Spine 1.2 mode check ==="

STABLE="$ROOT/config/modern_vulkan_stable.cfg"
MODE_C="$ROOT/renderers/vulkan/tr_render_mode_vk.c"
MODE_H="$ROOT/renderers/vulkan/tr_render_mode_vk.h"
INIT="$ROOT/renderers/vulkan/tr_init.c"
PASS_C="$ROOT/renderers/vulkan/vk_pass_registry.c"
RP="$ROOT/renderers/vulkan/vk_render_pass.c"
HYB="$ROOT/config/vulkan_overlay_selective_hybrid.cfg"
PT="$ROOT/config/vulkan_overlay_pt_reference.cfg"
DOC="$ROOT/docs/RENDERER_SPINE_1.2.md"

[[ -f "$STABLE" ]] || fail "missing modern_vulkan_stable.cfg"
grep -qE 'seta r_renderMode 3' "$STABLE" || fail "stable must remain mode 3 (Unified Clustered Tier A)"
grep -qE 'seta r_hybrid1 0' "$STABLE" || fail "stable must keep Hybrid1 off"
grep -qE 'seta r_rtx 0' "$STABLE" || fail "stable must keep RTX off"
pass "boot Tier A: mode 3 clustered raster, RT/Hybrid1 off"

grep -qE 'Cvar_CheckRange\( r_renderMode, "0", "5"' "$INIT" || fail "r_renderMode range must be 0-5"
grep -q 'case 4:' "$MODE_C" || fail "mode 4 latch missing"
grep -q 'case 5:' "$MODE_C" || fail "mode 5 latch missing"
grep -q 'R_RenderMode_IsSelectiveHybrid' "$MODE_H" "$MODE_C" || fail "selective hybrid helper missing"
grep -q 'R_RenderMode_IsPathTracedReference' "$MODE_H" "$MODE_C" || fail "PT reference helper missing"
grep -q 'r_presentAdaptiveRecon' "$MODE_C" || fail "presentAdaptiveRecon cvar missing"
grep -q 'Forbidden: frame generation' "$MODE_C" || fail "FG forbid description missing"
pass "mode 4/5 helpers + presentAdaptiveRecon policy"

[[ -f "$HYB" && -f "$PT" && -f "$DOC" ]] || fail "missing Spine 1.2 overlay/docs"
grep -qE 'seta r_renderMode 4' "$HYB" || fail "selective hybrid overlay must set mode 4"
grep -qE 'seta r_pathtrace 0' "$HYB" || fail "selective hybrid must keep pathtrace off"
grep -qE 'seta r_renderMode 5' "$PT" || fail "PT reference overlay must set mode 5"
grep -qE 'seta r_pathtrace_composite 1' "$PT" || fail "PT reference must full-replace composite"
grep -qE 'seta r_hybrid1 0' "$PT" || fail "PT reference must disable Hybrid1"
grep -q 'No frame generation' "$DOC" || fail "Spine 1.2 doc must forbid frame generation"
pass "Tier B/C overlays + docs"

grep -q 'hybrid1_x_pathtrace' "$PASS_C" || fail "Hybrid1×PT combo validator missing"
grep -q 'spine_1_2_pt_reference\|spine_1_2_selective_hybrid' "$PASS_C" || fail "Spine 1.2 combo ids missing"
grep -q 'R_RenderMode_IsPathTracedReference' "$RP" || fail "render_pass must prioritize mode 5 PT"
pass "ownership: exclusive PT vs Hybrid1 demotion"

# Must not enable frame generation in overlays
! grep -qiE 'seta r_frameGen |seta r_dlssg |seta r_fsr3FrameGen |enable frame.?gen' "$HYB" "$PT" || \
  fail "overlays must not enable frame generation"
pass "no frame-generation enable in Spine 1.2 overlays"

echo "=== Spine 1.2 mode check PASSED ==="
echo "GPU (RTX): exec vulkan_overlay_selective_hybrid.cfg | vulkan_overlay_pt_reference.cfg; vid_restart"
echo "Recovery: exec modern_vulkan.cfg"
