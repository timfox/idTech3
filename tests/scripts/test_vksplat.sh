#!/usr/bin/env bash
# VkSplat 3DGS training checks — Eurographics 2026 / arXiv:2605.00219
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'VKSplat_ModelBenchmark' "$ROOT/src/extensions/research/vksplat/vksplat_model.c"
grep -q 'vksplat_train_step' "$ROOT/src/renderers/vulkan/vk_vksplat.c"
grep -q 'vksplat_project_fwd.comp' "$ROOT/scripts/compile_shaders.sh"
grep -q 'Vksplat_ConsoleInit' "$ROOT/src/qcommon/common.c"
test -f "$ROOT/docs/VKSPLAT.md"
test -f "$ROOT/tests/unit/test_vksplat_model.c"

echo "test_vksplat.sh: ok"
