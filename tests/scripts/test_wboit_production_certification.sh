#!/usr/bin/env bash
# Color Pipeline Phase 2.6 — live cert controller + specialized transparency scaffolds.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

CERT_H="$ROOT/renderers/vulkan/vk_wboit_production_cert.h"
CERT_C="$ROOT/renderers/vulkan/vk_wboit_production_cert.c"
LAB_H="$ROOT/renderers/vulkan/vk_transparency_lab.h"
LAB_C="$ROOT/renderers/vulkan/vk_transparency_lab.c"
SPEC_H="$ROOT/renderers/vulkan/vk_specialized_transparency.h"
SPEC_C="$ROOT/renderers/vulkan/vk_specialized_transparency.c"
ROUTE="$ROOT/renderers/vulkan/vk_transparency_route.c"
BACK="$ROOT/renderers/vulkan/tr_backend.c"
DOC_LIVE="$ROOT/docs/WBOIT_LIVE_CERTIFICATION.md"
DOC_ROUTE="$ROOT/docs/TRANSPARENCY_ROUTING.md"
DOC_REF="$ROOT/docs/REFRACTION.md"
DOC_COLOR="$ROOT/docs/COLOR_PIPELINE.md"

for f in "$CERT_H" "$CERT_C" "$LAB_H" "$LAB_C" "$SPEC_H" "$SPEC_C" \
	"$DOC_LIVE" "$DOC_ROUTE" "$DOC_REF" \
	"$ROOT/docs/CLASSIC_BLEND_COMPATIBILITY.md" \
	"$ROOT/docs/PORTAL_TRANSPARENCY.md" \
	"$ROOT/docs/WEAPON_TRANSPARENCY.md" \
	"$ROOT/docs/SOFT_PARTICLES.md" \
	"$ROOT/docs/TRANSPARENT_SHADOWS.md" \
	"$ROOT/docs/TRANSPARENCY_RESOURCE_CHAIN.md" \
	"$ROOT/docs/MBOIT_EXPERIMENTAL.md" \
	"$ROOT/docs/WBOIT_CERTIFICATION_THRESHOLDS.md"; do
	[[ -f "$f" ]] || fail "missing $f"
done
pass "Phase 2.6 sources + docs present"

grep -q 'WBOIT_PRODUCTION_CERTIFIED' "$CERT_H" || fail "PRODUCTION level missing"
grep -q 'WBOIT_CERT_STAGE_EMPTY_PIXEL\|WBOIT_CERT_EMPTY_PIXEL' "$CERT_H" || fail "EMPTY_PIXEL stage missing"
grep -q 'WBOIT_CERT_STAGE_SOAK\|WBOIT_CERT_SOAK' "$CERT_H" || fail "SOAK stage missing"
grep -q 'oit_certification_capture' "$CERT_C" || fail "capture command missing"
grep -q 'oit_certification_abort' "$CERT_C" || fail "abort command missing"
grep -q 'oit_cert_stage' "$CERT_C" || fail "oit_cert_stage missing"
grep -q 'wboit_production_status' "$CERT_C" || fail "wboit_production_status missing"
grep -q 'STATIC alone is NOT WBOIT_PRODUCTION_CERTIFIED\|not WBOIT_PRODUCTION_CERTIFIED\|never grants PRODUCTION\|MANUAL_OVERRIDE never' "$CERT_C" || fail "static≠production note"
grep -q 'WBOIT_EVIDENCE_MANUAL_OVERRIDE' "$CERT_H" || fail "MANUAL_OVERRIDE evidence missing"
grep -q 'r_oitAllowManualCertification' "$CERT_C" || fail "manual cert cvar missing"
grep -q 'oit_lab_run' "$ROOT/renderers/vulkan/vk_oit_lab.c" || fail "oit_lab_run missing"
pass "live certification controller"

grep -q 'r_transparencyReference' "$LAB_C" || fail "r_transparencyReference missing"
grep -q 'r_transparencyFreeze' "$LAB_C" || fail "r_transparencyFreeze missing"
grep -q 'r_transparencyCompare' "$LAB_C" || fail "r_transparencyCompare missing"
grep -q 'TRANSPARENCY_REF_SORTED_ALPHA' "$LAB_H" || fail "sorted reference mode missing"
pass "transparency laboratory"

grep -q 'refractiveMaterial_t' "$SPEC_H" || fail "refractiveMaterial_t missing"
grep -q 'SPECIAL_BLEND_MULTIPLY' "$SPEC_H" || fail "special blend routes missing"
grep -q 'TRANSPARENT_SHADOW_RECEIVE' "$SPEC_H" || fail "shadow flags missing"
grep -q 'WEAPON_EMISSIVE_RETICLE' "$SPEC_H" || fail "weapon classes missing"
grep -q 'r_softParticleRange' "$SPEC_C" || fail "soft particle cvars missing"
grep -q 'mboit_certification_status' "$SPEC_C" || fail "mboit_certification_status missing"
grep -q 'r_mboitCompare' "$SPEC_C" || fail "r_mboitCompare missing"
grep -q 'transparency_resource_validate' "$SPEC_C" || fail "resource validate missing"
pass "specialized transparency"

grep -q 'vk_wboit_production_cert_register' "$ROUTE" || fail "production cert not registered"
grep -q 'vk_transparency_lab_register' "$ROUTE" || fail "lab not registered"
grep -q 'vk_specialized_transparency_register' "$ROUTE" || fail "specialized not registered"
grep -q 'vk_transparency_resource_bump' "$BACK" || fail "backend must bump refractive gens"
grep -q 'Phase 2.6' "$ROUTE" || fail "route status must mention Phase 2.6"
pass "wiring"

grep -q 'WBOIT_PRODUCTION_CERTIFIED' "$DOC_LIVE" || fail "live cert doc"
grep -q 'Phase 2.6' "$DOC_COLOR" || fail "COLOR_PIPELINE Phase 2.6"
grep -q 'WBOIT_PRODUCTION_CERTIFIED' "$DOC_COLOR" || fail "COLOR_PIPELINE production gate"
pass "documentation"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All Phase 2.6 WBOIT live / specialized transparency gates passed."
