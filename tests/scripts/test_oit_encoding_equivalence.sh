#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_alphaEncodingCompare' "$ROOT/renderers/vulkan/vk_oit_alpha.c"
grep -q 'source-over encoding equivalence\|encoding equivalence' "$ROOT/tests/unit/test_oit_alpha_normalize.c"
echo "OK: encoding equivalence hooks"
