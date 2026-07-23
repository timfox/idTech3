#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
bash "$ROOT/tests/scripts/test_renderer_iq_p1.sh"
grep -q 'modern_raster_iq_reference' "$ROOT/config/modern_raster_iq_reference.cfg"
grep -q 'renderer_iq_profile' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c"
echo "test_renderer_iq_profile.sh OK"
