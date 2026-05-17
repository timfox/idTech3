#!/usr/bin/env bash
# Optional sanity check for a TRELLIS.2 checkout (no GPU / no Python inference).
# Usage: ./scripts/trellis_check.sh /path/to/TRELLIS.2
set -euo pipefail
repo="${1:-}"
if [[ -z "$repo" || ! -d "$repo" ]]; then
	echo "usage: $0 /path/to/TRELLIS.2" >&2
	exit 1
fi
for f in setup.sh example.py; do
	if [[ ! -f "$repo/$f" ]]; then
		echo "missing file: $repo/$f" >&2
		exit 1
	fi
done
if [[ ! -d "$repo/trellis2" ]]; then
	echo "missing package directory: $repo/trellis2" >&2
	exit 1
fi
echo "OK: TRELLIS.2 layout looks plausible under $repo"
