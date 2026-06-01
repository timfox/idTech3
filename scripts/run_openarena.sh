#!/usr/bin/env bash
# Launch idtech3 client for OpenArena-style base paths (QVM mods).
# Does not ship game data; set OA_BASE or pass +set fs_game.
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
FS_GAME_ARGS=()
if [ -n "$OA_BASE" ]; then
	FS_GAME_ARGS=( "+set" "fs_game" "$OA_BASE" )
fi

CLASSIC_ARGS=()
if [ "${CLASSIC_MOD:-0}" = "1" ]; then
	CLASSIC_ARGS=( "+set" "r_classicMod" "1" )
fi

exec "$CLIENT" \
	+set fs_basegame base \
	"${FS_GAME_ARGS[@]}" \
	"${CLASSIC_ARGS[@]}" \
	+exec q3_vulkan_compat \
	"$@"
