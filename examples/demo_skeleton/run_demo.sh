#!/usr/bin/env bash
# Same as scripts/run_demo.sh — lives here so a minimal copy of demo_skeleton still has a launcher.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
REL_ENV="$ROOT/release/local.env"
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
