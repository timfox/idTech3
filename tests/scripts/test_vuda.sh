#!/usr/bin/env bash
# VUDA CUDA-Vulkan multiplexing checks — arXiv:2605.01352
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'VUDA_ModelDataGen' "$ROOT/src/extensions/research/vuda/vuda_model.c"
grep -q 'vuda_maniskill' "$ROOT/src/extensions/research/vuda/vuda_console.c"
grep -q 'VudaCuda_BindStream' "$ROOT/src/extensions/research/vuda/vuda_cuda.c"
grep -q 'Vuda_ConsoleInit' "$ROOT/src/qcommon/common.c"
grep -q 'CUstream_bind' "$ROOT/docs/VUDA.md"
test -f "$ROOT/tests/unit/test_vuda_model.c"
test -f "$ROOT/src/renderers/vulkan/vk_vuda.c"

echo "test_vuda.sh: ok"
