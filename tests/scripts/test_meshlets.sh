#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

ML="$(idtech3_file renderers/vulkan/vk_meshlets.c src/renderers/vulkan/vk_meshlets.c)"
MESH="$(idtech3_file renderers/vulkan/tr_mesh.c src/renderers/vulkan/tr_mesh.c)"
TR="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
grep -q 'r_meshlets' "$ML"
grep -q 'meshlet_status' "$ML"
grep -q 'R_Meshlets_Bake' "$ML"
grep -q 'R_CullMD3SurfaceMeshlets' "$MESH"
grep -q 'R_Meshlets_Init' "$TR"
test -f "$ROOT/docs/MESHLETS.md"
echo "test_meshlets.sh: ok"
