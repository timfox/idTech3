#!/usr/bin/env bash
# Install the local 0 A.D. javelineer fixture into the active engine base path.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="${1:-/home/tim/Desktop/rts/binaries/data/mods/_test.dae/art/meshes/jav2.dae}"
DESTINATION="${ROOT}/release/base/models/rts/0ad_jav2.dae"

if [ ! -f "${SOURCE}" ]; then
	echo "missing 0 A.D. Collada fixture: ${SOURCE}" >&2
	exit 1
fi

mkdir -p "$(dirname "${DESTINATION}")"
cp -f "${SOURCE}" "${DESTINATION}"
echo "installed RTS fixture: ${DESTINATION}"
