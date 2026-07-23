#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'P1_CERT_STAGE_VELOCITY' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
grep -q 'VELOCITY_GPU' "$ROOT/docs/VELOCITY_GPU_CERTIFICATION.md"
echo "All p1_velocity_live checks passed."
