#!/usr/bin/env bash
# Dedicated server for OpenArena / Q3-style bases (QVM qagame).
# Set OA_BASE to the folder containing pak*.pk3 (or +set fs_game on command line).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${RELEASE_DIR:-$PROJECT_ROOT/release}"

SERVER="$RELEASE_DIR/idtech3_server"
for candidate in "$SERVER" "$SERVER.x64" "$SERVER.x86_64"; do
	if [ -x "$candidate" ]; then
		SERVER="$candidate"
		break
	fi
done

if [ ! -x "$SERVER" ]; then
	echo "Error: idtech3_server not found under $RELEASE_DIR" >&2
	exit 1
fi

OA_BASE="${OA_BASE:-}"
FS_GAME_ARGS=()
if [ -n "$OA_BASE" ]; then
	if [ ! -d "$OA_BASE" ]; then
		echo "Error: OA_BASE is not a directory: $OA_BASE" >&2
		exit 1
	fi
	FS_GAME_ARGS=( "+set" "fs_game" "$OA_BASE" )
fi

exec "$SERVER" \
	+set dedicated 1 \
	+set fs_basegame base \
	+set com_hunkMegs "${COM_HUNKMEGS:-128}" \
	"${FS_GAME_ARGS[@]}" \
	"$@"
