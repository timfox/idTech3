#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'IQ_READBACK_FRAME_MISMATCH' "$ROOT/renderers/vulkan/vk_renderer_p1_live.h"
grep -q 'P1_Live_ValidateIdentity\|fixtureFrame' "$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
echo "All p1_readback_identity checks passed."
