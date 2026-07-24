#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'gray_veil' "$ROOT/docs/GRAY_VEIL.md"
echo "PASS: volumetric depth upsample doc gate"
