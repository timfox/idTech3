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
  ./examples/demo_skeleton/run_demo.sh
  ./examples/demo_skeleton/run_demo_client.sh
  ./run_demo_client.sh /opt/my-playfield +set r_fullscreen 0
EOF
	exit 0
fi

# Skeleton local.env must not override IDTECH3_DEMO_ROOT if the parent launcher already set it.
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
if [[ -n "$SAVED_DEMO_ROOT" ]]; then
	IDTECH3_DEMO_ROOT="$SAVED_DEMO_ROOT"
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

# Common mistake: IDTECH3_DEMO_ROOT points at the client binary (…/release/idtech3)
if [[ -n "${IDTECH3_DEMO_ROOT:-}" ]]; then
	if [[ -f "$IDTECH3_DEMO_ROOT" ]]; then
		case "$(basename "$IDTECH3_DEMO_ROOT")" in
		idtech3|idtech3.exe)
			echo "Note: playfield root was a file ($(basename "$IDTECH3_DEMO_ROOT")); using its directory as fs_basepath." >&2
			IDTECH3_DEMO_ROOT="$(cd "$(dirname "$IDTECH3_DEMO_ROOT")" && pwd)"
			;;
		esac
	fi
	if [[ ! -d "$IDTECH3_DEMO_ROOT" ]]; then
		echo "Not a directory: $IDTECH3_DEMO_ROOT" >&2
		echo "Set IDTECH3_DEMO_ROOT to the folder that will contain base/ and idtech3_demo/ (often …/release or examples/demo_skeleton)." >&2
		exit 2
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
# Playfield-specific env (e.g. release/local.env after setup_demo_layout)
if [[ -f "$BASE_ROOT/local.env" ]]; then
	set -a
	# shellcheck source=/dev/null
	source "$BASE_ROOT/local.env"
	set +a
	BASE_ROOT="$(cd "$IDTECH3_DEMO_ROOT" && pwd)"
fi
# Engine loads mods from fs_game/<name>/; pk3 must live as idtech3_demo/idtech3_demo.pk3.
# If the user dropped idtech3_demo.pk3 next to base/, link it into place.
mkdir -p "$BASE_ROOT/idtech3_demo"
PK3="$BASE_ROOT/idtech3_demo/idtech3_demo.pk3"
if [[ ! -f "$PK3" && -f "$BASE_ROOT/idtech3_demo.pk3" ]]; then
	if ln -sf "../idtech3_demo.pk3" "$PK3" 2>/dev/null; then
		echo "Linked $PK3 -> ../idtech3_demo.pk3 (flat layout)." >&2
	fi
fi
if [[ ! -f "$PK3" ]]; then
	echo "Missing demo mod: $PK3" >&2
	echo "Build: ./examples/demo_game/build_demo_pack.sh" >&2
	echo "Then either:" >&2
	echo "  cp build-vk-Release/idtech3_demo.pk3 \"$BASE_ROOT/idtech3_demo/\"" >&2
	echo "  or run: ./examples/demo_skeleton/setup_demo_layout.sh \"$BASE_ROOT\"" >&2
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
