#!/usr/bin/env bash
# Optional sanity check for a Spectral-Energy upstream checkout (no GPU / no inference).
# Usage: ./scripts/spec_energy_check.sh /path/to/flux_spec_energy
set -euo pipefail
repo="${1:-}"
if [[ -z "$repo" || ! -d "$repo" ]]; then
	echo "usage: $0 /path/to/upstream-checkout" >&2
	exit 1
fi
for d in flux_sega qwen_sega; do
	if [[ ! -d "$repo/$d" ]]; then
		echo "missing directory: $repo/$d" >&2
		exit 1
	fi
done
if [[ ! -f "$repo/requirements.txt" ]]; then
	echo "missing file: $repo/requirements.txt" >&2
	exit 1
fi
if [[ ! -f "$repo/flux_sega/run_flux.py" ]]; then
	echo "missing file: $repo/flux_sega/run_flux.py" >&2
	exit 1
fi
echo "OK: Spectral-Energy upstream layout looks plausible under $repo"
