#!/usr/bin/env bash
# Package built artifacts and release assets into a versioned archive.
# Usage: tools/package_release.sh [version]
# Env overrides:
#   SRC_DIR - directory to package (default: release/)
#   OUTDIR  - directory for archives (default: dist/)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${SRC_DIR:-$ROOT/release}"
OUTDIR="${OUTDIR:-$ROOT/dist}"
VERSION="${1:-$(git -C "$ROOT" describe --tags --dirty --always 2>/dev/null || date +%Y%m%d)}"
PLATFORM="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

if [[ ! -d "${SRC_DIR}" ]]; then
	echo "[package_release] Source directory not found: ${SRC_DIR}" >&2
	exit 1
fi

mkdir -p "${OUTDIR}"
ARCHIVE="${OUTDIR}/idtech3-${VERSION}-${PLATFORM}-${ARCH}.tar.gz"

echo "[package_release] Packaging '${SRC_DIR}' -> ${ARCHIVE}"
tar -C "${SRC_DIR}" -czf "${ARCHIVE}" .

echo "[package_release] Done."
echo "[package_release] Contents:"
tar -tf "${ARCHIVE}" | head -n 20

