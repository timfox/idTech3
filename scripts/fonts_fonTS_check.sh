#!/usr/bin/env bash
# Optional sanity check for a FonTS checkout (no GPU / no Python run).
# Usage: ./scripts/fonts_fonTS_check.sh /path/to/FonTS
set -euo pipefail
repo="${1:-}"
if [[ -z "$repo" || ! -d "$repo" ]]; then
	echo "usage: $0 /path/to/FonTS" >&2
	exit 1
fi
for d in "flux+SCA-only" "flux+SCA-both"; do
	if [[ ! -d "$repo/$d" ]]; then
		echo "missing directory: $repo/$d" >&2
		exit 1
	fi
done
echo "OK: FonTS layout looks plausible under $repo"
