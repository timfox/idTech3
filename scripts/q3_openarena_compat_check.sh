#!/usr/bin/env bash
# Static checks that Quake III Arena and OpenArena-style QVM mods remain supported.
# Does not require retail game data.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"

PASS=0
FAIL=0

pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }

bin_path() {
	local bin="$1"
	local base="$RELEASE_DIR/$bin"
	for candidate in "$base" "$base.x64" "$base.x86_64" "$base.aarch64"; do
		if [ -f "$candidate" ]; then
			echo "$candidate"
			return
		fi
	done
	echo ""
}

echo "=== Q3 / OpenArena compatibility checks ==="
echo "Release dir: $RELEASE_DIR"
echo ""

# QVM loader must remain in the engine.
if grep -q 'VM_LoadQVM' "$PROJECT_ROOT/src/qcommon/vm.c"; then
	pass "QVM loader (VM_LoadQVM) present in vm.c"
else
	fail "VM_LoadQVM missing from vm.c"
fi

if grep -q 'vm_native_module' "$PROJECT_ROOT/src/qcommon/vm.c" || \
   [ -f "$PROJECT_ROOT/src/qcommon/vm_native_module.c" ]; then
	pass "native game module path present (native before QVM fallback)"
else
	fail "native module loading path not found"
fi

# Documentation for classic mods.
if grep -qi 'OpenArena' "$PROJECT_ROOT/docs/COMPATIBILITY.md" && \
   grep -qi 'QVM' "$PROJECT_ROOT/docs/COMPATIBILITY.md"; then
	pass "docs/COMPATIBILITY.md documents QVM + OpenArena-style mods"
else
	fail "docs/COMPATIBILITY.md missing QVM/OpenArena guidance"
fi

# Optional generative hooks must default off (no impact on classic play).
_trellis_default_ok=0
for _trellis_src in \
	"$PROJECT_ROOT/src/extensions/generative/cl_trellis.c" \
	"$PROJECT_ROOT/src/client/core/cl_main.c"; do
	if [ -f "$_trellis_src" ] && \
	   grep -q 'cl_trellis_enable' "$_trellis_src" && \
	   grep -q '"cl_trellis_enable", "0"' "$_trellis_src"; then
		_trellis_default_ok=1
		break
	fi
done
if [ "$_trellis_default_ok" -eq 1 ]; then
	pass "cl_trellis_enable defaults to 0"
else
	fail "cl_trellis_enable default not 0"
fi

if [ -f "$PROJECT_ROOT/src/world/fog_biology.cpp" ] && \
   grep -q 'r_fogBiology' "$PROJECT_ROOT/src/world/fog_biology.cpp" && \
   grep -q '"r_fogBiology", "0"' "$PROJECT_ROOT/src/world/fog_biology.cpp"; then
	pass "r_fogBiology defaults to 0 (opt-in bioaerosol ecology)"
else
	fail "r_fogBiology default not 0 in fog_biology.cpp"
fi

if [ -f "$PROJECT_ROOT/src/world/genetic_gan.cpp" ] && \
   grep -q 'cl_geneticGan' "$PROJECT_ROOT/src/world/genetic_gan.cpp" && \
   grep -q '"cl_geneticGan", "0"' "$PROJECT_ROOT/src/world/genetic_gan.cpp"; then
	pass "cl_geneticGan defaults to 0 (opt-in procedural genome API)"
else
	fail "cl_geneticGan default not 0 in genetic_gan.cpp"
fi

if grep -qE 'r_vegWind[[:space:]]*=[[:space:]]*ri\.Cvar_Get\([[:space:]]*"r_vegWind"[[:space:]]*,[[:space:]]*"0"' \
	"$PROJECT_ROOT/src/renderers/vulkan/vk_postfx.c"; then
	pass "r_vegWind defaults to 0 (classic maps unchanged unless enabled)"
else
	fail "r_vegWind default not 0 in Vulkan postfx"
fi

if grep -qE 'r_forwardPlusDistanceSort[[:space:]]*=[[:space:]]*ri\.Cvar_Get\([[:space:]]*"r_forwardPlusDistanceSort"[[:space:]]*,[[:space:]]*"0"' \
	"$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"; then
	pass "r_forwardPlusDistanceSort defaults to 0 (classic Forward+ overload order preserved)"
else
	fail "r_forwardPlusDistanceSort default not 0"
fi

if grep -qE 'r_forwardPlusDepthCull[[:space:]]*=[[:space:]]*ri\.Cvar_Get\([[:space:]]*"r_forwardPlusDepthCull"[[:space:]]*,[[:space:]]*"0"' \
	"$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"; then
	pass "r_forwardPlusDepthCull defaults to 0 (classic Forward+ tile cull timing preserved)"
else
	fail "r_forwardPlusDepthCull default not 0"
fi

if grep -qE 'r_rtxEntities[[:space:]]*=[[:space:]]*ri\.Cvar_Get\([[:space:]]*"r_rtxEntities"[[:space:]]*,[[:space:]]*"0"' \
	"$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"; then
	pass "r_rtxEntities defaults to 0 (RTX demo unchanged unless enabled)"
else
	fail "r_rtxEntities default not 0"
fi

if grep -qE 'r_vdbFog[[:space:]]*=[[:space:]]*ri\.Cvar_Get\([[:space:]]*"r_vdbFog"[[:space:]]*,[[:space:]]*"0"' \
	"$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"; then
	pass "r_vdbFog defaults to 0 (VDB volumetric blend off unless enabled)"
else
	fail "r_vdbFog default not 0"
fi

if grep -qE 'cs_autoInit[[:space:]]*=[[:space:]]*Cvar_Get\([[:space:]]*"cs_autoInit"[[:space:]]*,[[:space:]]*"0"' \
	"$PROJECT_ROOT/src/qcommon/csharp_debug.c"; then
	pass "cs_autoInit defaults to 0 (C# runtime manual until cs_reload)"
else
	fail "cs_autoInit default not 0 in csharp_debug.c"
fi

if grep -q 'ri\.Cmd_AddCommand( "vdb_load"' "$PROJECT_ROOT/src/renderers/vulkan/vk_vdb.c" && \
   grep -q 'ri\.Cmd_AddCommand( "vdb_bind_fog"' "$PROJECT_ROOT/src/renderers/vulkan/vk_vdb.c"; then
	pass "VDB console commands vdb_load / vdb_bind_fog registered"
else
	fail "VDB console commands missing from vk_vdb.c"
fi

if grep -q 'CL_GetLegacyGameState' "$PROJECT_ROOT/runtime/client/core/cl_cgame.c" && \
   grep -q 'legacyGameState_t' "$PROJECT_ROOT/runtime/client/core/cl_cgame.c"; then
	pass "stock cgame.qvm uses legacy gameState_t copy path"
else
	fail "legacy cgame gamestate path missing from cl_cgame.c"
fi

if grep -q 'CL_GetLegacySnapshot' "$PROJECT_ROOT/runtime/client/core/cl_cgame.c" && \
   grep -q 'legacySnapshot_t' "$PROJECT_ROOT/runtime/client/core/cl_cgame.c"; then
	pass "stock cgame.qvm uses legacy snapshot copy path"
else
	fail "legacy cgame snapshot path missing from cl_cgame.c"
fi

if grep -q 'SV_EnsureGameVersionConfigstring' "$PROJECT_ROOT/runtime/server/sv_init.c"; then
	pass "server CS_GAME_VERSION retail fallback wired"
else
	fail "SV_EnsureGameVersionConfigstring missing from sv_init.c"
fi

if grep -q 'CM_Stream_OnBaseMapLoad' "$PROJECT_ROOT/engine/core/cm_stream.c" && \
   grep -q 'SV_OpenWorld_OnMapLoad' "$PROJECT_ROOT/runtime/server/sv_openworld.c"; then
	pass "classic map guards clear sector collision overlays"
else
	fail "classic map sector overlay guards missing"
fi

SERVER="$(bin_path idtech3_server)"
CLIENT="$(bin_path idtech3)"
if [ -n "$SERVER" ]; then
	if strings "$SERVER" 2>/dev/null | grep -E 'qvm|VM_LoadQVM' | head -1 | grep -q .; then
		pass "dedicated server binary references QVM modules"
	else
		fail "server binary missing QVM string references"
	fi
else
	fail "idtech3_server not found under $RELEASE_DIR"
fi

if [ -n "$CLIENT" ]; then
	if strings "$CLIENT" 2>/dev/null | grep -E 'fs_basegame|FS_GetBaseGameDir' | head -1 | grep -q .; then
		pass "client supports fs_basegame (e.g. baseq3 for Q3A)"
	else
		fail "client missing fs_basegame support strings"
	fi
else
	fail "idtech3 client not found under $RELEASE_DIR"
fi

echo ""
echo "=== Results ==="
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
if [ "$FAIL" -gt 0 ]; then
	echo "Q3/OA COMPAT CHECK FAILED"
	exit 1
fi
echo "Q3/OA COMPAT CHECK PASSED"
exit 0
