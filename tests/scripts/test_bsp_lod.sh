#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

rg -q 'r_bspLod' renderers/vulkan/tr_init.c renderers/vulkan/tr_local.h renderers/vulkan/tr_surface.c
rg -q 'R_BspBuildSurfaceLODs' renderers/vulkan/tr_bsp.c renderers/vulkan/tr_local.h
rg -q 'lodNumIndices\[2\]' renderers/vulkan/tr_local.h
rg -q 'RB_QueueSurfaceVBO' renderers/vulkan/tr_surface.c
rg -q 'r_bspLodDistance' docs/BSP_LOD.md

echo "Planar BSP LOD contract: PASS"
