# Multi-material PBR height-blend

Vertex-painted multi-material blending for Vulkan PBR (Naughty Dog / TLOU-style): up to **8 layers**, **RGBA + stream2 weights**, and **height-aware** transitions from each layer’s `normalHeightMap` alpha.

## Authoring (`.shader`)

```
textures/demo/blend_ground
{
  {
    materialBlend vertex
    blendSharpness 8
    map textures/demo/dirt.tga
    normalHeightMap textures/demo/dirt_nh.tga
    ormMap textures/demo/dirt_orm.tga
    layerMap 1 textures/demo/rock.tga
    layerNormalHeightMap 1 textures/demo/rock_nh.tga
    layerOrmMap 1 textures/demo/rock_orm.tga
    // layerMap / layerNormalHeightMap / layerOrmMap 2..7 optional
  }
}
```

| Keyword | Meaning |
|---------|---------|
| `materialBlend vertex` | Enable blend; vertex color = layer weights (not lighting tint) |
| `materialBlend splat` | Terrain/control-map weights (`splatMap`) |
| `blendSharpness <f>` | Height-blend contrast (default 8) |
| `layerMap <1-7> <path>` | Albedo for layer 1..7 (layer 0 = `map`) |
| `layerNormalMap` / `layerNormalHeightMap` | Normal (+ height in alpha for NH) |
| `layerOrmMap` / `layerOrmsMap` | Packed ORM for that layer |
| `splatMap <path>` | RGBA control texture (splat mode) |

Layer 0 reuses existing `map` / `normalMap`|`normalHeightMap` / `ormMap`.

## Runtime

- **`r_materialBlend`** (default **1**): master toggle; **0** forces layer0-only.
- **`r_materialBlendSharpness`** (default **8**): global override when &gt; 0; else shader `blendSharpness`.
- Startup log: `Material blend: ON (vertex + height)`.

## Behavior

1. Weights 0–3 from `frag_color0` (vertex RGBA); weights 4–7 from `frag_color1` (`.paint` stream2 / `TESS_RGBA1`).
2. Per-layer height `h_i` from normal-map **alpha** when that layer used `*HeightMap`; else soft path.
3. Height-blend across up to 8 layers; renormalize.
4. Soft fallback (no heights): normalize active weights.
5. Blend albedo / ORM / tangent normals; **do not** tint albedo by vertex color.
6. Forces **`CGEN_EXACT_VERTEX`**; skips world SH overwrite so painted weights survive.
7. Layer textures bind on **descriptor set 19** (`blend_albedo` / `blend_normal` / `blend_orm` arrays × 8).

## Surfaces

Supported where vertex colors exist: **BSP**, **glTF**, **IQM**. **MD3** uses optional `.md3.paint` sidecar (bind-pose only; prefer glTF/IQM for new painted props).

## Studio paint authoring

- **`r_materialPaint` 1** + **`r_studio_tools` 1**: Studio / Paint panel.
- Brush cvars: **`r_materialPaintRadius`**, **`r_materialPaintStrength`**, **`r_materialPaintChannels`** (bits 0–3 stream0, 4–7 stream2).
- Sidecar: **`maps/<map>.paint`** (magic `ID3P`, version 2; optional second RGBA stream for layers 4–7).
- Commands: **`paint_save`**, **`paint_load`**, **`paint_clear`**, **`paint_status`**.
- Sidecar is source of truth for blend weights (q3map2 vertex light can clobber BSP colors).
- Radiant: import/export via `examples/radiant/Editor/bridge_tools.py` — see [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md).

## MD3 paint

- Sidecar: **`models/<name>.md3.paint`** (same `ID3P` header + sequential bind-pose RGBA).
- Loaded in `R_LoadMD3`; applied in `RB_SurfaceMesh` → `tess.vertexColors`.
- Does **not** change `md3XyzNormal_t` / on-disk MD3.

## Terrain / CBT

See [CBT_TERRAIN.md](CBT_TERRAIN.md) for `r_cbtTerrain`, `cbt_load` / `cbt_splat`, and splat authoring.

## Implementation notes

Descriptor set **19** holds unique layer albedo/normal/ORM sampler arrays (8 each). Specialization constants `material_blend_layers` (2..8) and `material_height_mask` (8 bits) avoid FS `#ifdef` explosion — see [MATERIAL_PERMUTATIONS.md](MATERIAL_PERMUTATIONS.md). POM is disabled on blend pipelines.

See also [PBR_TEXTURES.md](PBR_TEXTURES.md).

## Demo

- Shader: `examples/demo_game/mod/scripts/demo_material_blend.shader`
- Config: `exec demo_material_blend.cfg`
- CBT splat: `exec demo_cbt_splat.cfg`
- Placeholder textures under `examples/demo_game/bootstrap_media/textures/demo/`
