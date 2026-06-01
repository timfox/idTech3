#!/usr/bin/env bash
# Launch idtech3 client for OpenArena-style base paths (QVM mods).
# Does not ship game data; set OA_BASE or pass +set fs_game.
#
# Environment:
#   OA_BASE       - path to OpenArena (or Q3) pk3 folder (+set fs_game)
#   CLASSIC_MOD=1 - enable r_classicMod + conservative Vulkan cvars at launch
#   AUTO_CLASSIC=1 - when OA_BASE path looks like OpenArena/baseoa, set CLASSIC_MOD=1
#   RELEASE_DIR   - default: repo release/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${RELEASE_DIR:-$PROJECT_ROOT/release}"

CLIENT="$RELEASE_DIR/idtech3"
for candidate in "$CLIENT" "$CLIENT.x64" "$CLIENT.x86_64"; do
	if [ -x "$candidate" ]; then
		CLIENT="$candidate"
		break
	fi
done

if [ ! -x "$CLIENT" ]; then
	echo "Error: idtech3 client not found under $RELEASE_DIR (run ./scripts/compile_engine.sh vulkan)" >&2
	exit 1
fi

OA_BASE="${OA_BASE:-}"

if [ -z "${CLASSIC_MOD:-}" ] && [ "${AUTO_CLASSIC:-1}" = "1" ] && [ -n "$OA_BASE" ]; then
	case "$OA_BASE" in
		*[Oo]pen[Aa]rena*|*openarena*|*baseoa*|*BaseOA*)
			CLASSIC_MOD=1
			echo "run_openarena: AUTO_CLASSIC enabled conservative Vulkan (CLASSIC_MOD=1)"
			;;
	esac
fi

FS_GAME_ARGS=()
if [ -n "$OA_BASE" ]; then
	if [ ! -d "$OA_BASE" ]; then
		echo "Error: OA_BASE is not a directory: $OA_BASE" >&2
		exit 1
	fi
	FS_GAME_ARGS=( "+set" "fs_game" "$OA_BASE" )
fi

BOOT_CVARS=(
	"+set" "fs_basegame" "base"
	"+set" "cl_renderer" "vulkan"
	"+set" "r_fbo" "1"
)

if [ "${CLASSIC_MOD:-0}" = "1" ]; then
	BOOT_CVARS+=(
		"+set" "r_classicMod" "1"
		"+set" "r_volumetricFog" "0"
		"+set" "r_ssao" "0"
		"+set" "r_bloom" "0"
		"+set" "r_ext_smaa" "0"
		"+set" "r_ssr" "0"
		"+set" "r_sharpen" "0.0"
		"+set" "r_exposure_auto" "0"
		"+set" "r_fogFluid" "0"
		"+set" "r_forwardPlus" "0"
		"+set" "r_rtx" "0"
		"+set" "r_rtxDemo" "0"
		"+set" "r_rtxEntities" "0"
		"+set" "r_vdbFog" "0"
		"+set" "r_vdb" "0"
		"+set" "r_vegWind" "0"
		"+set" "cl_flux_enable" "0"
		"+set" "cl_trellis_enable" "0"
		"+set" "cl_spec_energy_enable" "0"
	)
fi

# Optional: copy example cfgs into OA_BASE when present (so +exec works in-game).
if [ -n "$OA_BASE" ] && [ -d "$PROJECT_ROOT/examples" ]; then
	for cfg in q3_vulkan_compat.cfg q3_classic_mod.cfg q3_fbo_safe.cfg; do
		if [ -f "$PROJECT_ROOT/examples/$cfg" ] && [ ! -f "$OA_BASE/$cfg" ]; then
			cp -n "$PROJECT_ROOT/examples/$cfg" "$OA_BASE/$cfg" 2>/dev/null || cp "$PROJECT_ROOT/examples/$cfg" "$OA_BASE/$cfg"
		fi
	done
	BOOT_CVARS+=( "+exec" "q3_vulkan_compat" )
fi

exec "$CLIENT" \
	"${BOOT_CVARS[@]}" \
	"${FS_GAME_ARGS[@]}" \
	"$@"
