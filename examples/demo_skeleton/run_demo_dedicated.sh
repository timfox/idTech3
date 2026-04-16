#!/usr/bin/env bash
# Headless: idtech3_server with idtech3_demo (same playfield layout as run_demo_client.sh).
# Usage: ./run_demo_dedicated.sh [PLAYFIELD_DIR] [server args...]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REPO_RELEASE="$(cd "$REPO_ROOT/release" 2>/dev/null && pwd || true)"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	cat <<'EOF'
Run idtech3_server with fs_game idtech3_demo (dedicated 1).

Usage:
  run_demo_dedicated.sh [PLAYFIELD_DIR] [args...]

Default PLAYFIELD_DIR is this folder when it contains idtech3_demo/.
EOF
	exit 0
fi

SAVED_DEMO_ROOT="${IDTECH3_DEMO_ROOT:-}"
ENV_FILE=""
for cand in "$SCRIPT_DIR/local.env" "$SCRIPT_DIR/demo_skeleton.env" "${IDTECH3_DEMO_ENV:-}"; do
	if [[ -n "$cand" && -f "$cand" ]]; then
		ENV_FILE="$cand"
		break
	fi
done
if [[ -n "$ENV_FILE" ]]; then
	set -a
	# shellcheck source=/dev/null
	source "$ENV_FILE"
	set +a
fi
if [[ -n "$SAVED_DEMO_ROOT" && -d "$SAVED_DEMO_ROOT" ]]; then
	IDTECH3_DEMO_ROOT="$SAVED_DEMO_ROOT"
fi
if [[ -n "${IDTECH3_DEMO_ROOT:-}" && ! -d "$IDTECH3_DEMO_ROOT" ]]; then
	echo "Warning: IDTECH3_DEMO_ROOT is not a directory ($IDTECH3_DEMO_ROOT) - fix or remove it in local.env" >&2
	unset IDTECH3_DEMO_ROOT
fi

if [[ -n "${1:-}" && "$1" != -* ]]; then
	IDTECH3_DEMO_ROOT="$1"
	shift
fi

if [[ -z "${IDTECH3_DEMO_ROOT:-}" && -d "$SCRIPT_DIR/idtech3_demo" ]]; then
	IDTECH3_DEMO_ROOT="$SCRIPT_DIR"
fi

if [[ -z "${IDTECH3_DEMO_ROOT:-}" ]]; then
	echo "Set IDTECH3_DEMO_ROOT or run from examples/demo_skeleton with idtech3_demo/ present." >&2
	exit 2
fi

if [[ -f "$IDTECH3_DEMO_ROOT" ]]; then
	case "$(basename "$IDTECH3_DEMO_ROOT")" in
	idtech3|idtech3_server|idtech3.exe|idtech3_server.exe)
		echo "Note: playfield root was a binary path; using its directory as fs_basepath." >&2
		IDTECH3_DEMO_ROOT="$(cd "$(dirname "$IDTECH3_DEMO_ROOT")" && pwd)"
		;;
	esac
fi
if [[ ! -d "$IDTECH3_DEMO_ROOT" ]]; then
	echo "Not a directory: $IDTECH3_DEMO_ROOT" >&2
	exit 2
fi

BASE_ROOT="$(cd "$IDTECH3_DEMO_ROOT" && pwd)"
if [[ -f "$BASE_ROOT/local.env" ]]; then
	set -a
	# shellcheck source=/dev/null
	source "$BASE_ROOT/local.env"
	set +a
	IDTECH3_DEMO_ROOT="$BASE_ROOT"
fi
mkdir -p "$BASE_ROOT/idtech3_demo"
PK3="$BASE_ROOT/idtech3_demo/idtech3_demo.pk3"
if [[ ! -f "$PK3" && -f "$BASE_ROOT/idtech3_demo.pk3" ]]; then
	ln -sf "../idtech3_demo.pk3" "$PK3" 2>/dev/null || true
fi
if [[ ! -f "$PK3" ]]; then
	echo "Missing $PK3 - build ./examples/demo_game/build_demo_pack.sh and copy pk3 into idtech3_demo/ (or place idtech3_demo.pk3 in playfield root)." >&2
	exit 2
fi

BASE_DIR_NAME="${DEMO_BASE_DIR:-base}"
if [[ ! -d "$BASE_ROOT/$BASE_DIR_NAME" ]]; then
	if [[ "$BASE_DIR_NAME" == "base" && -d "$BASE_ROOT/baseq3" ]]; then
		echo "Found baseq3/ but not base/. Set DEMO_BASE_DIR=baseq3 in local.env" >&2
		exit 2
	fi
	echo "Missing $BASE_ROOT/$BASE_DIR_NAME" >&2
	exit 2
fi

shopt -s nullglob
_base_pk3s=( "$BASE_ROOT/$BASE_DIR_NAME"/*.pk3 )
shopt -u nullglob
if [[ ${#_base_pk3s[@]} -eq 0 ]]; then
	echo "Note: no .pk3 in $BASE_ROOT/$BASE_DIR_NAME - +map will need qagame/maps from a full base." >&2
fi

SERVER="${IDTECH3_SERVER:-}"
if [[ -z "$SERVER" ]]; then
	if [[ -x "$BASE_ROOT/idtech3_server" ]]; then
		SERVER="$BASE_ROOT/idtech3_server"
	elif [[ -n "$REPO_RELEASE" && -x "$REPO_RELEASE/idtech3_server" ]]; then
		SERVER="$REPO_RELEASE/idtech3_server"
	else
		echo "No idtech3_server. Build engine or set IDTECH3_SERVER in local.env" >&2
		exit 2
	fi
fi

MAP="${DEMO_MAP:-q3dm1}"
EXTRA=( "$@" )
BASEGAME_ARGS=()
if [[ -n "${DEMO_BASE_DIR:-}" ]]; then
	BASEGAME_ARGS=( +set fs_basegame "$DEMO_BASE_DIR" )
fi

exec "$SERVER" +set dedicated 1 +set fs_basepath "$BASE_ROOT" +set fs_game idtech3_demo "${BASEGAME_ARGS[@]}" +map "$MAP" "${EXTRA[@]}"
