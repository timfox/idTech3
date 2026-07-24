#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'Do not clamp SceneHDR' "$ROOT/docs/HDR_SUN_EXPOSURE.md"
# bloom must not force 0-1 scene clamp
! grep -q 'clamp(.*0.0.*1.0)' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag" || \
  grep -q 'bloomMeterLuma' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag"
echo "PASS: no SceneHDR 0-1 pre-exposure clamp in bloom path"
