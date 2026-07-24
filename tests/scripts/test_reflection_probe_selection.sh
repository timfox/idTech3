#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'reflection_probe_status' "$ROOT/renderers/vulkan/vk_environment_probes.c"
grep -q 'environmentProbe_t' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: reflection probe selection scaffold"
