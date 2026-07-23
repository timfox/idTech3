#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 'Com_Memset( s_indirect' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "zero cmds"
grep -q 'r_gpuDrawForceStaleCommand\|s_drawCountPublished' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "stale/fault"
echo "PASS: test_indirect_stale_prevention.sh"
