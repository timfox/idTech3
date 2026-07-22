#!/usr/bin/env bash
# Foundation Consolidation: G-buffer compact layout + bandwidth reporting.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/GBUFFER_2.md"
DOC0="$ROOT/docs/GBUFFER_2_0.md"
BF="$ROOT/renderers/vulkan/vk_black_frame.c"

[[ -f "$DOC" ]] || fail "missing GBUFFER_2.md"
for sec in Ownership "Data flow" "Buffer formats" Lifecycle "Fallback behavior" "Debug commands" "Performance cost" "Known limitations" "Next milestone hooks"; do
	grep -q "## $sec" "$DOC" || fail "GBUFFER_2.md missing section: $sec"
done
pass "GBUFFER_2.md sections present"

grep -q 'GBUFFER_2.md' "$DOC0" || fail "GBUFFER_2_0.md must link to GBUFFER_2.md"
grep -q 'gbuffer_bandwidth' "$DOC" || fail "doc must mention gbuffer_bandwidth"
grep -q 'R_SelectSurfaceRenderPath\|R_SelectMaterialRenderPath' "$DOC" || fail "doc must mention material routing"
pass "G-buffer doc cross-links and symbols"

grep -q 'gbuffer_bandwidth' "$BF" || fail "gbuffer_bandwidth command missing in vk_black_frame.c"
grep -q 'r_gbufferBandwidth\|r_gbufferCompact' "$BF" || fail "gbuffer cvars missing in vk_black_frame.c"
pass "gbuffer bandwidth wiring present"

if [[ -f "$ROOT/renderers/vulkan/shaders/glsl/gbuffer_octahedral.glsl" ]]; then
	grep -q 'GbufEncodeOctahedral' "$ROOT/renderers/vulkan/shaders/glsl/gbuffer_octahedral.glsl" || fail "octahedral encode missing"
	grep -q 'gbuffer_compact\|GbufEncodeOctahedral' "$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl" || fail "gen_frag must dual-write octahedral when compact"
	pass "gbuffer_octahedral.glsl present"
else
	grep -q 'gbuffer_octahedral' "$DOC0" || fail "GBUFFER_2_0 must mention gbuffer_octahedral.glsl"
	pass "octahedral helpers documented (source optional)"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All G-buffer layout checks passed."
