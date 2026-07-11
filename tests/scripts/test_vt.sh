#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

VT="$(idtech3_file renderers/vulkan/vk_vt.c src/renderers/vulkan/vk_vt.c)"
TR="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
grep -q 'r_vt' "$VT"
grep -q 'vt_status' "$VT"
grep -q 'vt_load' "$VT"
grep -q 'R_VT_Init' "$TR"
test -f "$ROOT/docs/VIRTUAL_TEXTURE.md"
echo "test_vt.sh: ok"
