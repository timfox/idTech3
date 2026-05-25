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
if grep -q 'cl_trellis_enable' "$PROJECT_ROOT/src/client/cl_main.c" && \
   grep -q '"cl_trellis_enable", "0"' "$PROJECT_ROOT/src/client/cl_main.c"; then
	pass "cl_trellis_enable defaults to 0"
else
	fail "cl_trellis_enable default not 0"
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

if grep -qE 'cs_autoInit[[:space:]]*=[[:space:]]*(ri\.)?Cvar_Get\([[:space:]]*"cs_autoInit"[[:space:]]*,[[:space:]]*"0"' \
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
