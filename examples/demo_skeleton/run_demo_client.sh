#!/usr/bin/env bash
# Launch idtech3 with fs_game idtech3_demo over IDTECH3_DEMO_ROOT.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_RELEASE="$(cd "$SCRIPT_DIR/../../release" 2>/dev/null && pwd || true)"

ENV_FILE=""
for cand in "$SCRIPT_DIR/local.env" "$SCRIPT_DIR/demo_skeleton.env" "${IDTECH3_DEMO_ENV:-}"; do
	if [[ -n "$cand" && -f "$cand" ]]; then
		ENV_FILE="$cand"
		break
	fi
done
if [[ -n "$ENV_FILE" ]]; then
	# shellcheck source=/dev/null
	set -a
	source "$ENV_FILE"
	set +a
fi

if [[ -z "${IDTECH3_DEMO_ROOT:-}" ]]; then
	echo "Set IDTECH3_DEMO_ROOT to the folder containing base/ (or baseq3/) and idtech3_demo/" >&2
	echo "Example: cp demo_skeleton.env.example local.env && edit, or export IDTECH3_DEMO_ROOT=..." >&2
	exit 2
fi

BASE_ROOT="$(cd "$IDTECH3_DEMO_ROOT" && pwd)"
PK3="$BASE_ROOT/idtech3_demo/idtech3_demo.pk3"
if [[ ! -f "$PK3" ]]; then
	echo "Missing $PK3 — build the pack: ./examples/demo_game/build_demo_pack.sh then copy idtech3_demo.pk3 here" >&2
	exit 2
fi

ENGINE="${IDTECH3_ENGINE:-}"
if [[ -z "$ENGINE" ]]; then
	if [[ -x "$BASE_ROOT/idtech3" ]]; then
		ENGINE="$BASE_ROOT/idtech3"
	elif [[ -n "$REPO_RELEASE" && -x "$REPO_RELEASE/idtech3" ]]; then
		ENGINE="$REPO_RELEASE/idtech3"
	else
		echo "No engine binary. Set IDTECH3_ENGINE or place idtech3 in \$IDTECH3_DEMO_ROOT or build engine to release/" >&2
		exit 2
	fi
fi

RENDERER="${DEMO_RENDERER:-vulkan}"
EXTRA=( "$@" )
MAP_ARGS=()
if [[ -n "${DEMO_MAP:-}" ]]; then
	MAP_ARGS=( +map "$DEMO_MAP" )
fi

exec "$ENGINE" +set fs_basepath "$BASE_ROOT" +set fs_game idtech3_demo +set cl_renderer "$RENDERER" "${MAP_ARGS[@]}" "${EXTRA[@]}"
