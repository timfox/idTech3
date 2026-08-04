#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

test -f renderers/vulkan/tr_source_vbsp.c
rg -q 'R_LoadSourceVBSPWorld' renderers/vulkan/tr_source_vbsp.c renderers/vulkan/tr_bsp.c renderers/vulkan/tr_local.h
rg -q '0x50534256u' renderers/vulkan/tr_source_vbsp.c renderers/vulkan/tr_bsp.c
rg -q 'SV_LUMP_COUNT 64' renderers/vulkan/tr_source_vbsp.c
rg -q 'clean-room' docs/SOURCE_VBSP_FORMAT_SUPPORT.md
rg -q '!isBsp30World && !isSourceVBSPWorld' renderers/vulkan/tr_bsp.c
rg -q 'R_BspBuildSurfaceLODs\( &s_worldData \)' renderers/vulkan/tr_bsp.c
rg -q 'lodNumIndices' renderers/vulkan/tr_source_vbsp.c renderers/vulkan/tr_surface.c
rg -q 'bsp_lod_status' renderers/vulkan/tr_init.c
rg -q 'SV_LoadTree' renderers/vulkan/tr_source_vbsp.c
rg -q 'SV_LoadVisibility' renderers/vulkan/tr_source_vbsp.c
rg -q 'SV_LUMP_LEAFFACES' renderers/vulkan/tr_source_vbsp.c
rg -q 'R_SourceEntities_LoadEntityString' renderers/vulkan/tr_source_vbsp.c
rg -q 'R_SourceEntities_AddLights' renderers/vulkan/tr_source_vbsp.c renderers/vulkan/tr_scene.c
rg -q 'source_fgd_load' renderers/vulkan/tr_init.c

echo "Source VBSP clean-room reader/render bridge: PASS"
