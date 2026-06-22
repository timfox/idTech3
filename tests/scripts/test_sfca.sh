#!/usr/bin/env bash
# SFCA smoke test — C unit tests.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"

if [[ ! -x "${BUILD}/unit_sfca" ]]; then
  echo "SKIP: unit_sfca not built (USE_RESEARCH_EXTENSIONS / full profile)"
  exit 0
fi

"${BUILD}/unit_sfca"

echo "test_sfca.sh: OK"
