#!/usr/bin/env bash
# Headless-friendly: run idtech3_server with idtech3_demo over IDTECH3_DEMO_ROOT.
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
	set -a
	# shellcheck source=/dev/null
	source "$ENV_FILE"
	set +a
fi

if [[ -z "${IDTECH3_DEMO_ROOT:-}" ]]; then
	echo "Set IDTECH3_DEMO_ROOT (see demo_skeleton.env.example)" >&2
	exit 2
fi

BASE_ROOT="$(cd "$IDTECH3_DEMO_ROOT" && pwd)"
PK3="$BASE_ROOT/idtech3_demo/idtech3_demo.pk3"
if [[ ! -f "$PK3" ]]; then
	echo "Missing $PK3" >&2
	exit 2
fi

SERVER="${IDTECH3_SERVER:-}"
if [[ -z "$SERVER" ]]; then
	if [[ -x "$BASE_ROOT/idtech3_server" ]]; then
		SERVER="$BASE_ROOT/idtech3_server"
	elif [[ -n "$REPO_RELEASE" && -x "$REPO_RELEASE/idtech3_server" ]]; then
		SERVER="$REPO_RELEASE/idtech3_server"
	else
		echo "No idtech3_server. Set IDTECH3_SERVER or copy idtech3_server next to data or use release/" >&2
		exit 2
	fi
fi

MAP="${DEMO_MAP:-q3dm1}"
EXTRA=( "$@" )

exec "$SERVER" +set dedicated 1 +set fs_basepath "$BASE_ROOT" +set fs_game idtech3_demo +map "$MAP" "${EXTRA[@]}"
