#!/usr/bin/env bash
# Export target source lists from a game-profile CMake configure (Phase 5d scaffold).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-${ROOT}/build-msvc-manifest}"

"${ROOT}/scripts/layout_forwarding_symlinks.sh" >&2

cmake -S "${ROOT}" -B "${BUILD}" \
	-G Ninja \
	-DIDTECH3_PROFILE=game \
	-DIDTECH3_EXPORT_MSVC_MANIFEST=ON \
	-DSKIP_IDPAK_CHECK=ON \
	>/dev/null

MANIFEST="${BUILD}/msvc_source_manifest.json"
[ -f "${MANIFEST}" ] || { echo "missing ${MANIFEST}" >&2; exit 1; }
echo "${MANIFEST}"
python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print('groups:', ', '.join(f'{k}={len(v)}' for k,v in sorted(d.items()))); print('total:', sum(len(v) for v in d.values()))" "${MANIFEST}" >&2
