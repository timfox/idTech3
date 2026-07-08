#!/usr/bin/env bash
# Stage CI build artifacts into release/ for smoke_test.sh (Linux/macOS/Windows layouts).
set -euo pipefail

RELEASE_DIR="${1:-release}"
BIN_DIR="${2:-bin}"

if [ ! -d "$BIN_DIR" ]; then
	echo "stage_ci_release: bin dir missing: $BIN_DIR" >&2
	exit 1
fi

mkdir -p "$RELEASE_DIR"

shopt -s nullglob
for artifact in "$BIN_DIR"/idtech3* "$BIN_DIR"/*.so "$BIN_DIR"/*.dll; do
	[ -e "$artifact" ] || continue
	cp -f "$artifact" "$RELEASE_DIR/"
done

echo "stage_ci_release: staged $(ls -1 "$RELEASE_DIR" | wc -l | tr -d ' ') file(s) in $RELEASE_DIR"
