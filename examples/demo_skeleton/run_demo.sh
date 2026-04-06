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
exec "$ROOT/examples/demo_skeleton/run_demo_client.sh" "$@"
