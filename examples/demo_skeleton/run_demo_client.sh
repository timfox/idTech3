#!/usr/bin/env bash
# Launch idtech3 with fs_game idtech3_demo. Defaults to this folder as the playfield.
#
# Usage:
#   ./run_demo_client.sh                    # use examples/demo_skeleton/ as root (when run from repo)
#   ./run_demo_client.sh /path/to/playfield # custom root (must contain base/ or baseq3/ and idtech3_demo/)
#   ./run_demo_client.sh --help
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REPO_RELEASE="$(cd "$REPO_ROOT/release" 2>/dev/null && pwd || true)"
RUN_VULKAN="$REPO_ROOT/scripts/run_vulkan.sh"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	cat <<'EOF'
Launch idtech3 with fs_game idtech3_demo.

Usage:
  run_demo_client.sh [PLAYFIELD_DIR] [engine args...]

PLAYFIELD_DIR defaults to this script's folder when it contains idtech3_demo/.
Otherwise set IDTECH3_DEMO_ROOT in local.env (see demo_skeleton.env.example).

Examples:
  ./scripts/run_demo.sh
  ./examples/demo_skeleton/run_demo_client.sh
  ./run_demo_client.sh /opt/my-playfield +set r_fullscreen 0
EOF
	exit 0
fi

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

# First argument: explicit playfield root
if [[ -n "${1:-}" && "$1" != -* ]]; then
	IDTECH3_DEMO_ROOT="$1"
	shift
fi

# Default: this directory if it looks like the skeleton (has idtech3_demo subdir)
if [[ -z "${IDTECH3_DEMO_ROOT:-}" ]]; then
	if [[ -d "$SCRIPT_DIR/idtech3_demo" ]]; then
		IDTECH3_DEMO_ROOT="$SCRIPT_DIR"
	fi
fi

if [[ -z "${IDTECH3_DEMO_ROOT:-}" ]]; then
	echo "Could not find a playfield directory." >&2
	echo "" >&2
	echo "Easiest: put game data in examples/demo_skeleton/base/ and run from repo:" >&2
	echo "  ./scripts/run_demo.sh" >&2
	echo "" >&2
	echo "Or set IDTECH3_DEMO_ROOT in local.env (copy demo_skeleton.env.example)." >&2
	exit 2
fi

BASE_ROOT="$(cd "$IDTECH3_DEMO_ROOT" && pwd)"
PK3="$BASE_ROOT/idtech3_demo/idtech3_demo.pk3"
if [[ ! -f "$PK3" ]]; then
	echo "Missing demo mod: $PK3" >&2
	echo "Build it: ./examples/demo_game/build_demo_pack.sh" >&2
	echo "Then copy idtech3_demo.pk3 into idtech3_demo/ under your playfield." >&2
	exit 2
fi

BASE_DIR_NAME="${DEMO_BASE_DIR:-base}"
if [[ ! -d "$BASE_ROOT/$BASE_DIR_NAME" ]]; then
	if [[ "$BASE_DIR_NAME" == "base" && -d "$BASE_ROOT/baseq3" ]]; then
		echo "Found baseq3/ but not base/. Set in local.env: DEMO_BASE_DIR=baseq3" >&2
		echo "Or symlink: ln -s baseq3 \"$BASE_ROOT/base\"" >&2
		exit 2
	fi
	echo "Missing game data folder: $BASE_ROOT/$BASE_DIR_NAME" >&2
	echo "Add your licensed .pk3 files there. See base/README.txt" >&2
	exit 2
fi

ENGINE="${IDTECH3_ENGINE:-}"
LAUNCH_WRAPPER=""
if [[ -z "$ENGINE" ]]; then
	if [[ -x "$BASE_ROOT/idtech3" ]]; then
		ENGINE="$BASE_ROOT/idtech3"
	elif [[ -n "$REPO_RELEASE" && -x "$REPO_RELEASE/idtech3" ]]; then
		ENGINE="$REPO_RELEASE/idtech3"
	else
		echo "No idtech3 binary found." >&2
		echo "Build: ./scripts/compile_engine.sh vulkan" >&2
		echo "Or set IDTECH3_ENGINE in local.env to your client path." >&2
		exit 2
	fi
fi

RENDERER="${DEMO_RENDERER:-vulkan}"
USE_RUN_VK="${IDTECH3_USE_RUN_VULKAN:-1}"
EXTRA=( "$@" )
MAP_ARGS=()
if [[ -n "${DEMO_MAP:-}" ]]; then
	MAP_ARGS=( +map "$DEMO_MAP" )
fi

BASEGAME_ARGS=()
if [[ -n "${DEMO_BASE_DIR:-}" ]]; then
	BASEGAME_ARGS=( +set fs_basegame "$DEMO_BASE_DIR" )
fi

ARGS=( +set fs_basepath "$BASE_ROOT" +set fs_game idtech3_demo "${BASEGAME_ARGS[@]}" +set cl_renderer "$RENDERER" "${MAP_ARGS[@]}" "${EXTRA[@]}" )

# run_vulkan.sh picks release/idtech3 and sets LD_LIBRARY_PATH for custom SDL; only for default engine path
if [[ "$RENDERER" == "vulkan" && "$USE_RUN_VK" != "0" && -x "$RUN_VULKAN" && "$ENGINE" == "$REPO_RELEASE/idtech3" ]]; then
	exec "$RUN_VULKAN" "${ARGS[@]}"
fi

exec "$ENGINE" "${ARGS[@]}"
