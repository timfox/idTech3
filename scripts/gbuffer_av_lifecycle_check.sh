#!/usr/bin/env bash
# Static contract: G-buffer + Ambient Visibility resource lifecycle hardening.
# Does not promote AV mode 4. Does not modify modern_vulkan_stable.cfg ownership.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== G-buffer / AV lifecycle check ==="

DGB_H="$ROOT/renderers/vulkan/vk_deferred_gbuffer.h"
DGB_C="$ROOT/renderers/vulkan/vk_deferred_gbuffer.c"
AV_C="$ROOT/renderers/vulkan/vk_ambient_visibility.c"
PRES="$ROOT/renderers/vulkan/vk_presentation.c"
ATT="$ROOT/renderers/vulkan/vk_attachments.c"
BE="$ROOT/renderers/vulkan/tr_backend.c"
VK_H="$ROOT/renderers/vulkan/vk.h"
STABLE="$ROOT/config/modern_vulkan_stable.cfg"
QUALITY="$ROOT/config/modern_vulkan_quality.cfg"

[[ -f "$DGB_H" && -f "$DGB_C" && -f "$AV_C" && -f "$PRES" ]] || fail "missing core lifecycle sources"

# Predicates separated
grep -q 'vk_deferred_gbuffer_resources_wanted' "$DGB_H" || fail "resources_wanted missing from header"
grep -q 'vk_deferred_gbuffer_fill_wanted' "$DGB_H" || fail "fill_wanted missing from header"
grep -q 'vk_classify_current_view' "$DGB_H" || fail "view classification missing"
grep -q 'vk_deferred_gbuffer_generation' "$DGB_H" || fail "generation API missing"
grep -q 'vk_deferred_gbuffer_invalidate_runtime' "$DGB_H" || fail "invalidate_runtime missing"
pass "resource vs fill predicates + view class + generation API"

# fill_wanted must NOT require doneWorldScene (set after capture)
if grep -n 'doneWorldScene' "$DGB_C" | grep -v 'Do NOT require doneWorldScene' | grep -q .; then
  fail "fill_wanted / deferred gbuffer must not gate on doneWorldScene (flag is set after geometry capture)"
fi
pass "fill_wanted independent of doneWorldScene"

# Generation fields
grep -q 'deferredGbufferGeneration' "$VK_H" || fail "deferredGbufferGeneration missing from vk.h"
grep -q 'deferredGbufferExtentW' "$VK_H" || fail "deferredGbufferExtentW missing"
grep -q 'deferredGbufferFallbackReason' "$VK_H" || fail "fallback reason missing"
grep -q 'deferredGbufferGeneration++' "$ATT" || fail "scaffold finalize must bump generation"
grep -q 'create_color_attachment_soft' "$ATT" || fail "G-buffer scaffold must soft-fail CreateImage"
grep -q 'vk_create_fullres_color_attachment_soft' "$ATT" || fail "fullres soft CreateImage helper missing"
pass "generation tracking in vk state + scaffold"

# Presentation teardown order
grep -q 'vk_deferred_gbuffer_invalidate_runtime' "$PRES" || fail "presentation teardown must invalidate deferred runtime"
grep -q 'vk_visibility_buffer_shutdown' "$PRES" || fail "presentation teardown must shutdown visibility buffer"
grep -q 'vk_ambient_visibility_shutdown' "$PRES" || fail "presentation teardown must shutdown AV"
# invalidate before destroy_attachments
python3 - <<'PY' || exit 1
from pathlib import Path
text = Path("renderers/vulkan/vk_presentation.c").read_text()
fn = text.split("void vk_teardown_presentation_targets", 1)[1].split("void vk_restore_presentation_targets", 1)[0]
for name in ("vk_deferred_gbuffer_invalidate_runtime", "vk_ambient_visibility_shutdown", "vk_destroy_attachments"):
    if name not in fn:
        raise SystemExit(f"FAIL: {name} missing from teardown")
if fn.index("vk_deferred_gbuffer_invalidate_runtime") > fn.index("vk_destroy_attachments"):
    raise SystemExit("FAIL: invalidate_runtime must run before destroy_attachments")
if fn.index("vk_ambient_visibility_shutdown") > fn.index("vk_destroy_attachments"):
    raise SystemExit("FAIL: AV shutdown must run before destroy_attachments")
print("PASS: teardown destroys descriptors/AV before attachments")
PY

# Presentation restore must rebind deferred/AV and sticky-invalidate temporal history
python3 - <<'PY' || exit 1
from pathlib import Path
text = Path("renderers/vulkan/vk_presentation.c").read_text()
td = text.split("void vk_teardown_presentation_targets", 1)[1].split("void vk_restore_presentation_targets", 1)[0]
rs = text.split("void vk_restore_presentation_targets", 1)[1].split("void vk_restart_swapchain", 1)[0]
for name in ("vk_temporal_request_sticky_reset", "VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE"):
    if name not in td:
        raise SystemExit(f"FAIL: teardown must sticky-reset temporal ({name})")
    if name not in rs:
        raise SystemExit(f"FAIL: restore must sticky-reset temporal ({name})")
for name in ("vk_deferred_gbuffer_ensure_runtime", "vk_visibility_buffer_ensure_runtime",
             "vk_ambient_visibility_init", "vk_ambient_visibility_reset_history"):
    if name not in rs:
        raise SystemExit(f"FAIL: restore missing {name}")
# Order: ensure deferred before AV init; AV reset after init; temporal sticky present
if rs.index("vk_deferred_gbuffer_ensure_runtime") > rs.index("vk_ambient_visibility_init"):
    raise SystemExit("FAIL: deferred ensure_runtime must precede AV init on restore")
if rs.index("vk_ambient_visibility_init") > rs.index("vk_ambient_visibility_reset_history"):
    raise SystemExit("FAIL: AV init must precede AV history reset on restore")
print("PASS: restore rebinds deferred/AV + temporal sticky on teardown/restore")
PY

# Visibility scaffold soft-fail (sibling of G-buffer scaffold soft CreateImage)
grep -q 'visibility IDs/bary soft-fail' "$ATT" || fail "visibility scaffold IDs soft-fail missing"
grep -q 'visibility class soft-fail' "$ATT" || fail "visibility scaffold class soft-fail missing"

# Non-split weapon guard
grep -A8 'if ( !vk_deferred_opaque_transparent_split()' "$BE" | grep -q 'RDF_NOWORLDMODEL' || \
  fail "non-split G-buffer path must guard RDF_NOWORLDMODEL"
pass "weapon/NOWORLDMODEL guard on non-split path"

# Failure injection + diagnostics
grep -q 'r_dgbFailInject' "$DGB_C" "$ATT" || fail "r_dgbFailInject missing"
grep -q 'r_avFailInject' "$AV_C" || fail "r_avFailInject missing"
grep -q 'deferred_gbuffer_status' "$ROOT/renderers/vulkan/tr_init.c" || fail "deferred_gbuffer_status command missing"
grep -q 'gbuffer   :' "$ROOT/renderers/vulkan/tr_init_diagnostics.inc" || fail "havenrp_renderer_status gbuffer block missing"
pass "failure injection + diagnostics"

# Forward+ AV composite (mode 2)
grep -q 'r_renderMode->integer == 2' "$AV_C" || fail "Forward+ mode-2 AV composite gate missing"
grep -q 'descriptor_generation' "$VK_H" || fail "descriptor_generation missing from vk.h"
grep -q 'descriptor_generation = vk.deferredGbufferGeneration' "$DGB_C" || fail "fill descriptors must stamp generation"
grep -q 'lighting_descriptor_generation = vk.deferredGbufferGeneration' "$DGB_C" || fail "lighting descriptors must stamp generation"
grep -q 'vk_deferred_gbuffer_invalidate_runtime' "$ROOT/renderers/vulkan/vk_shutdown.c" || fail "shutdown must invalidate deferred before attachment destroy"
grep -q 'demo_gbuffer_av_lifecycle.cfg' "$ROOT/scripts/compile_engine.sh" || fail "lifecycle demo cfg must be packaged"
[[ -f "$ROOT/config/demo_gbuffer_av_lifecycle.cfg" ]] || fail "demo_gbuffer_av_lifecycle.cfg missing"
pass "descriptor generation + shutdown order + demo cfg"

# Soft-fail pipeline create (no VK_CHECK fatal on G-buffer fill path)
grep -q 'gbuffer_pipeline_' "$DGB_C" || fail "create_pipeline must soft-fail with fallback reason"
grep -q 'legacy_ssao' "$DGB_C" || fail "set_fallback must restore legacy_ssao owner"
grep -q 'lighting_create_failed' "$DGB_C" "$VK_H" || fail "lighting soft-fail sticky missing"
grep -q 'composite_create_failed' "$DGB_C" "$VK_H" || fail "composite soft-fail sticky missing"
grep -q 'debug_create_failed' "$DGB_C" "$VK_H" || fail "debug soft-fail sticky missing"
grep -q 'fail-inject cleared' "$DGB_C" || fail "fail-inject recovery path missing"
# AV fail-inject must not kill G-buffer fill (sticky until scaffold recreate)
if grep -nE '^\s*vk_deferred_gbuffer_set_fallback\s*\(' "$AV_C" | grep -q .; then
  fail "AV must not call vk_deferred_gbuffer_set_fallback (decouple AO demote from G-buffer)"
fi
grep -q 'AV_FailInject' "$AV_C" || fail "AV_FailInject helper missing"
grep -q 'AV_DemoteToLegacySsao' "$AV_C" || fail "AV demote-to-ssao helper missing"
grep -q 'forceHistory' "$AV_C" || fail "r_avFailInject=history thrash path missing"
# Visibility fill shares main-world view class
grep -q 'VK_VIEW_CLASS_MAIN_WORLD' "$ROOT/renderers/vulkan/vk_visibility_buffer.c" || \
  fail "visibility fill must gate on MAIN_WORLD view class"
# TLAS rebuild must invalidate AV temporal history (RTAO)
grep -q 'vk_rtx_tlas_revision' "$AV_C" "$ROOT/renderers/vulkan/extensions/rtx/vk_rtx.h" || \
  fail "TLAS revision API / AV consumption missing"
grep -q 'lastTlasRevision' "$AV_C" || fail "AV must track lastTlasRevision"
# No remaining VK_CHECK fatals on deferred G-buffer create paths
if grep -n 'VK_CHECK' "$DGB_C" | grep -q .; then
  fail "deferred G-buffer create paths must soft-fail (no VK_CHECK left in vk_deferred_gbuffer.c)"
fi
# AV soft-fail (no VK_CHECK fatal on image/pipeline create)
if grep -nE '^\s*VK_CHECK' "$AV_C" | grep -q .; then
  fail "AV create paths must soft-fail (no VK_CHECK left in vk_ambient_visibility.c)"
fi
grep -q 'AV image create failed' "$AV_C" || fail "AV EnsureImages soft-fail demote missing"
# Visibility soft-fail stickies
VIS="$ROOT/renderers/vulkan/vk_visibility_buffer.c"
grep -q 'fill_create_failed' "$VIS" "$VK_H" || fail "visbuf fill soft-fail sticky missing"
grep -q 'classify_create_failed' "$VIS" "$VK_H" || fail "visbuf classify soft-fail sticky missing"
if grep -nE '^\s*VK_CHECK' "$VIS" | grep -q .; then
  fail "visibility buffer create paths must soft-fail (no VK_CHECK left)"
fi
pass "soft-fail pipeline + visibility view-class gate + TLAS→AV history"


# Stable ownership frozen; quality must NOT silently enable mode 4 yet
grep -q 'seta r_ambientVisibilityMode 2' "$STABLE" || fail "stable AV owner changed (expected mode 2 GTAO)"
grep -q 'seta r_ssao 0' "$STABLE" || fail "stable must keep r_ssao 0"
if grep -qE 'seta r_ambientVisibilityMode 4' "$QUALITY"; then
  fail "quality must not promote AV mode 4 until GPU lifecycle matrix passes"
fi
pass "stable ownership unchanged; quality has not promoted AV mode 4"

echo "=== G-buffer / AV lifecycle check PASSED ==="
echo "Manual GPU matrix still required before promoting AV mode 4 to quality:"
echo "  menu, maps, map restart/transition, 10x vid_restart, resize, fullscreen,"
echo "  profile stable↔quality, AO 1→2→4→1, weapon, RDF_NOWORLDMODEL,"
echo "  r_dgbFailInject / r_avFailInject, RT off, clean shutdown."
echo "  Commands: deferred_gbuffer_status ; ambient_visibility_status ; havenrp_renderer_status"
