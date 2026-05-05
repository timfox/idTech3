#!/usr/bin/env bash
# One-command launcher: run idtech3 with the idtech3_demo mod using examples/demo_skeleton layout.
# Prerequisite: idtech3_demo.pk3 under idtech3_demo/ (rebuilt pack includes fonts + HUD bootstrap media). Retail .pk3 under base/ optional for maps/menus.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REL_ENV="$ROOT/release/local.env"
# setup_demo_layout.sh writes release/local.env with IDTECH3_DEMO_ROOT; pick it up if the user did not export
if [[ -z "${IDTECH3_DEMO_ROOT:-}" && -f "$REL_ENV" ]]; then
	set -a
	# shellcheck source=/dev/null
	source "$REL_ENV"
	set +a
fi
if [[ -n "${IDTECH3_DEMO_ROOT:-}" && ! -d "$IDTECH3_DEMO_ROOT" ]]; then
	echo "Warning: release/local.env sets invalid IDTECH3_DEMO_ROOT ($IDTECH3_DEMO_ROOT); ignoring" >&2
	unset IDTECH3_DEMO_ROOT
fi
exec "$ROOT/examples/demo_skeleton/run_demo_client.sh" "$@"
