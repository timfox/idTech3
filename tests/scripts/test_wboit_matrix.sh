#!/usr/bin/env bash
# Static gate: B0–B7 WBOIT soak / isolation matrix wiring (device soak is manual).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

DOC="$ROOT/docs/MOMENT_OIT_STOCHASTIC_ALPHA.md"
[[ -f "$DOC" ]] || fail "missing $DOC"

if grep -Eq 'B[0-7]|WBOIT soak|Isolation matrix' "$DOC"; then
	pass "MOMENT doc documents B0–B7 / WBOIT soak matrix"
else
	fail "MOMENT doc must mention B0–B7 or WBOIT soak matrix"
fi

grep -q 'r_oitDebug' "$ROOT/renderers/vulkan/tr_init.c" || fail 'r_oitDebug not registered in tr_init.c'
grep -q 'oit_status' "$ROOT/renderers/vulkan/vk_transparency_route.c" || fail 'oit_status command missing'
[[ -f "$ROOT/config/repro_oit_corruption.cfg" ]] || fail 'repro_oit_corruption.cfg missing'
[[ -f "$ROOT/config/demo_oit_isolation.cfg" ]] || fail 'demo_oit_isolation.cfg missing'

echo ""
echo "WBOIT B0–B7 soak matrix (device; stop at first band/tile failure):"
echo "  B0  r_oit 0                         — clean opaque / weapon / UI"
echo "  B1  WBOIT raw (ForwardPlus 0)       — no rectangular bands"
echo "  B2  r_oitClassify 1                 — no mid-bucket bands"
echo "  B3  r_oitForwardPlus 1              — no magenta tile slabs (OOB)"
echo "  B4  r_oitDebug 1–13                 — stage views coherent"
echo "  B5  r_oitDebug 14                   — magenta×coverage only"
echo "  B5b r_oitDebug 15                   — smooth FragCoord UV"
echo "  B6  r_oitDirectTest 1               — pure opaque after clear"
echo "  B6b r_oitDirectTest 2               — UV gradient composite"
echo "  B7  cg_drawGun 0/1                  — gun clean when world resolve clean"
echo ""
echo "Isolation entry points:"
echo "  exec demo_oit_isolation.cfg"
echo "  exec repro_oit_corruption.cfg"
echo "  ./scripts/oit_corruption_check.sh"
echo ""

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT matrix wiring checks passed."
exit 0
