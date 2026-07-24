#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'world_features_status' "$ROOT/renderers/vulkan/vk_world_presentation.c"
grep -q 'vk_world_presentation_register' "$ROOT/renderers/vulkan/tr_init.c"
bash "$ROOT/tests/scripts/test_clean_room_feature_provenance.sh"
bash "$ROOT/tests/scripts/test_hdr_exposure_volumes.sh"
echo "PASS: world presentation feature integration"
