#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'bloomSourceContract_t' "$ROOT/renderers/vulkan/vk_bloom_source_contract.h"
grep -q 'BLOOM_CONTRIB_WEAPON_OPAQUE' "$ROOT/renderers/vulkan/vk_bloom_source_contract.h"
grep -q 'bloom_source_validate' "$ROOT/renderers/vulkan/vk_bloom_source_contract.c"
echo "test_bloom_source_contract.sh OK"
