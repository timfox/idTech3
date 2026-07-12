#!/usr/bin/env bash
# VUDA CUDA-Vulkan multiplexing checks — arXiv:2605.01352
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

VUDA_MODEL="$(idtech3_file extensions/research/vuda/vuda_model.c src/extensions/research/vuda/vuda_model.c)"
VUDA_CON="$(idtech3_file extensions/research/vuda/vuda_console.c src/extensions/research/vuda/vuda_console.c)"
VUDA_CUDA="$(idtech3_file extensions/research/vuda/vuda_cuda.c src/extensions/research/vuda/vuda_cuda.c)"
VUDA_H="$(idtech3_file extensions/research/vuda/vuda_cuda.h src/extensions/research/vuda/vuda_cuda.h)"
COMMON="$(idtech3_file engine/core/common.c src/qcommon/common.c)"
CL_VUDA="$(idtech3_file runtime/client/shell/cl_vuda.c src/client/cl_vuda.c)"
VK_VUDA="$(idtech3_file renderers/vulkan/extensions/scaffold/vk_vuda.c src/renderers/vulkan/extensions/scaffold/vk_vuda.c)"

grep -q 'VUDA_ModelDataGen' "$VUDA_MODEL"
grep -q 'vuda_maniskill' "$VUDA_CON"
grep -q 'VudaCuda_BindStream' "$VUDA_CUDA"
grep -q 'VudaCuda_WaitRenderTimeline' "$VUDA_H"
grep -q 'Vuda_ConsoleInit' "$COMMON"
grep -q 'CUstream_bind' "$ROOT/docs/VUDA.md"
grep -q 'vuda_step_async' "$ROOT/docs/VUDA.md"
grep -q 'vuda_wait_render' "$ROOT/docs/VUDA.md"
grep -q 'vuda_step_async' "$CL_VUDA"
grep -q 'vuda_wait_step' "$CL_VUDA"
test -f "$ROOT/tests/unit/test_vuda_model.c"
test -f "$VK_VUDA"

echo "test_vuda.sh: ok"
