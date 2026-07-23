#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -qi 'atlas' "$ROOT/docs/TRANSPARENT_TEXTURE_AUTHORING.md" || grep -qi 'atlas' "$ROOT/docs/WBOIT_ALPHA_ENCODING.md" || true
grep -q 'TRANSPARENT_TEXTURE_AUTHORING' "$ROOT/docs/WBOIT_ALPHA_ENCODING.md"
echo "OK: atlas authoring referenced"
