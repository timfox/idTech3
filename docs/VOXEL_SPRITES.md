# Voxel sprites (MagicaVoxel `.vox` props)

Engine-native **3D cube-mesh props** from MagicaVoxel `.vox` files, placed like billboards (`misc_voxel` / `voxel_spawn`).

## Enable

Requires Vulkan renderer (default). Map props need **`r_spriteProps 1`** (default).

```bash
./scripts/compile_engine.sh vulkan
```

## Map entity

```
{
"classname" "misc_voxel"
"origin" "128 64 32"
"model" "models/vox/demo_crate.vox"
"scale" "2"          // Quake units per voxel (default 1)
"angle" "90"         // yaw degrees
}
```

`shader` is accepted as an alias for `model`.

## Console (local only)

```
voxel_spawn models/vox/demo_crate.vox [scale] [x y z] [yaw]
```

Defaults: scale `1`, origin near the player (or `0 0 64`), yaw `0`.

**Not networked** in this slice — no `sv_voxel_spawn`, `EF_VOXEL`, or QVM traps yet.

## Demo

```
exec demo_voxel_sprites.cfg
```

Ships a sample crate at `models/vox/demo_crate.vox` (demo_game / `base/models/vox/`).

## Format support (v1)

| Feature | Status |
|---------|--------|
| MagicaVoxel `SIZE` + `XYZI` (first model) | Yes |
| `RGBA` palette (else MagicaVoxel default) | Yes |
| Exposed-face mesh (no internal faces) | Yes |
| Scene graph / `nTRN` / multi-model / `MATL` | No |
| Greedy meshing / collision | No |
| Soft caps | dim ≤ 256, ≤ 32768 voxels |

Loader: `RE_RegisterModel( "*.vox" )` → `MOD_MESH` MD3 + 16×16 palette shader (`*voxpal/…` / `*voxsh/…`).

## Related

- Billboard/flipbook/imposter: `docs/MOD_SDK.md`, `demo_sprites.cfg`
- Parse unit: `ctest -R unit_vox_parse` / `unit_engine_sprite_map`
