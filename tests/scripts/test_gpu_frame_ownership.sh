#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*"; exit 1; }
grep -q 'gpu_frame_ownership_status\|s_visibilityFrame\|s_indirectCommandFrame' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "frame ids"
grep -q 'GPU Frame Ownership\|sceneUpdateFrame\|frameId' "$ROOT/docs/GPU_FRAME_OWNERSHIP.md" || fail "doc"
echo "PASS: test_gpu_frame_ownership.sh"
