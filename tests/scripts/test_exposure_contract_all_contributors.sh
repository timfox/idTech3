#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'exposure_contract_validate' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'Unexposed SceneHDR' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
echo "PASS: exposure contract commands"
