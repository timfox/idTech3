#!/usr/bin/env bash
# Smoke checks for MagicaVoxel voxel sprite props.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/renderers/common/tr_vox_parse.c" || fail "tr_vox_parse.c missing"
test -f "$ROOT/renderers/vulkan/tr_model_vox.c" || fail "tr_model_vox.c missing"
test -f "$ROOT/docs/VOXEL_SPRITES.md" || fail "docs/VOXEL_SPRITES.md missing"
test -f "$ROOT/examples/demo_game/mod/demo_voxel_sprites.cfg" || fail "demo_voxel_sprites.cfg missing"
test -f "$ROOT/examples/demo_game/mod/models/vox/demo_crate.vox" || fail "demo_crate.vox missing"

rg -q 'ENGINE_SPRITE_VOXEL' "$ROOT/engine/core/engine_sprite_map.h" || fail "ENGINE_SPRITE_VOXEL missing"
rg -q 'misc_voxel' "$ROOT/engine/core/engine_sprite_map.c" || fail "misc_voxel parse missing"
rg -q 'R_RegisterVOX' "$ROOT/renderers/vulkan/tr_model.c" || fail "vox model loader not registered"
rg -q 'voxel_spawn' "$ROOT/runtime/client/shell/cl_engine_sprites.c" || fail "voxel_spawn missing"
rg -q 'RT_MODEL' "$ROOT/renderers/vulkan/tr_sprite_props.c" || fail "RT_MODEL voxel draw missing"

echo "OK: voxel sprites smoke checks passed"
