#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
test -f "$ROOT/docs/CLEAN_ROOM_WORLD_PRESENTATION.md"
test -f "$ROOT/docs/WORLD_PRESENTATION_PROVENANCE.md"
test -f "$ROOT/docs/CLEAN_ROOM_SOURCE_STYLE_FEATURES.md"
grep -q 'Independently implemented' "$ROOT/renderers/vulkan/vk_world_presentation.c"
grep -q 'vk_world_presentation_register' "$ROOT/renderers/vulkan/tr_init.c"
# Commands/comments must not advertise third-party product branding
! grep -E 'Source-style|GoldSrc|source_style_exposure' "$ROOT/renderers/vulkan/vk_world_presentation.c"
! grep -E 'Source-style|source_style_exposure' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
echo "PASS: clean-room provenance framework"
