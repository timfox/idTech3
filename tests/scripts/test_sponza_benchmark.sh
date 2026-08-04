#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CFG="$ROOT/examples/demo_game/mod/sponza_benchmark.cfg"
CAPTURE_CFG="$ROOT/examples/demo_game/mod/sponza_benchmark_capture.cfg"
RUNNER="$ROOT/scripts/run_sponza_benchmark.sh"
DOC="$ROOT/docs/renderer_validation/SPONZA_USDA_BENCHMARK.md"
USDA="$ROOT/sponza/main_sponza/NewSponza_Main_USD_Zup_003.usda"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -f "$CFG" ]] || fail "Sponza benchmark config missing"
[[ -f "$CAPTURE_CFG" ]] || fail "single-process Sponza capture config missing"
[[ -x "$RUNNER" ]] || fail "Sponza benchmark runner missing or not executable"
[[ -f "$DOC" ]] || fail "Sponza benchmark documentation missing"
[[ -f "$USDA" ]] || fail "Sponza USDA payload missing"

for value in \
	'set r_mode -1' \
	'set r_customWidth 1280' \
	'set r_customHeight 720' \
	'set r_renderScale 0' \
	'set r_picmip 5' \
	'set r_taa 0' \
	'set r_districtCamera 1' \
	'set r_districtOnly 1' \
	'set r_districtAnchorView 0' \
	'set r_freeusdShaderMap 1' \
	'set r_freeusdMeshBudget 250000' \
	'district_load world/sponza_fixture.usda'; do
	grep -qF "$value" "$CFG" || fail "benchmark missing fixed setting: $value"
done

for command in renderer_status renderer_capture_frame_contract cluster_status deferred_status oit_status; do
	grep -qF "$command" "$CFG" || fail "benchmark missing ownership checkpoint: $command"
done


grep -qF 'screenshotJPEG sponza_benchmark_capture' "$CAPTURE_CFG" ||
	fail "single-process capture JPEG missing"
grep -qF 'set r_freeusdShaderMap 0' "$CAPTURE_CFG" ||
	fail "ownership benchmark must use bounded material fallback"
for capture in forwardplus deferred wboit; do
	grep -qF "${capture}.jpg" "$DOC" ||
		fail "benchmark missing ${capture} JPEG capture"
done

grep -qF 'SPONZA_USDA_BENCHMARK.md' "$DOC" || fail "benchmark documentation is not self-referenced"
grep -qF 'NewSponza_Main_USD_Zup_003.usda' "$DOC" || fail "benchmark documentation lacks source USDA"
grep -qF 'sponza_benchmark_capture' "$CAPTURE_CFG" || fail "benchmark capture name missing"
grep -qF 'fullMesh' "$ROOT/runtime/client/world/cl_district.cpp" || fail "district manifest full-payload ownership is missing"
grep -qF 'string fullMesh' "$ROOT/tests/data/usd/sponza_fixture.usda" || fail "Sponza fixture has no explicit full payload"
grep -qF 'native scene handoff' "$ROOT/docs/FREEUSD.md" || fail "FreeUSD 2027 handoff boundary is undocumented"
grep -qF 'compatibility bridge' "$ROOT/docs/FREEUSD.md" || fail "FreeUSD legacy bridge boundary is undocumented"
grep -qF 'Cmd_AddCommand( "usd_houdini"' "$ROOT/runtime/client/shell/cl_usd.cpp" || fail "Houdini/USD audit command is not wired"
grep -qF 'CL_USD_ListMeshPaths' "$ROOT/runtime/client/shell/cl_usd.cpp" || fail "composed mesh discovery fallback is missing"
grep -qF 'run_case forwardplus 2 0 0' "$RUNNER" || fail "Forward+ isolated benchmark case missing"
grep -qF 'run_case deferred 1 1 0' "$RUNNER" || fail "deferred isolated benchmark case missing"
grep -qF 'run_case wboit 3 0 1' "$RUNNER" || fail "WBOIT isolated benchmark case missing"

echo "Sponza benchmark contract passed."
