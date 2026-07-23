#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'HISTORY_OWNER_COUNT' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.h"
grep -q 'temporal_history_status' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c"
echo "test_temporal_history_registry.sh OK"
