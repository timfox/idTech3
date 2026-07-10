#!/usr/bin/env bash
# VkSplat 3DGS training checks — Eurographics 2026 / arXiv:2605.00219
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

VKSPLAT_MODEL="$(idtech3_file extensions/research/vksplat/vksplat_model.c src/extensions/research/vksplat/vksplat_model.c)"
VK_VKSPLAT="$(idtech3_file renderers/vulkan/extensions/splats/vk_vksplat.c src/renderers/vulkan/extensions/splats/vk_vksplat.c)"
COMMON="$(idtech3_file engine/core/common.c src/qcommon/common.c)"

grep -q 'VKSplat_ModelBenchmark' "$VKSPLAT_MODEL"
grep -q 'vksplat_train_step' "$VK_VKSPLAT"
grep -q 'vksplat_project_fwd.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'Vksplat_ConsoleInit' "$COMMON"
test -f "$ROOT/docs/VKSPLAT.md"
test -f "$ROOT/tests/unit/test_vksplat_model.c"

echo "test_vksplat.sh: ok"
