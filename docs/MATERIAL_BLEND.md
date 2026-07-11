# Multi-material PBR height-blend

Vertex-painted multi-material blending for Vulkan PBR (Naughty Dog / TLOU-style): up to **4 layers**, **RGBA vertex weights**, and **height-aware** transitions from each layer’s `normalHeightMap` alpha.

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
    // layerMap / layerNormalHeightMap / layerOrmMap 2..3 optional
  }
}
```

| Keyword | Meaning |
|---------|---------|
| `materialBlend vertex` | Enable blend; vertex color = layer weights (not lighting tint) |
| `blendSharpness <f>` | Height-blend contrast (default 8) |
| `layerMap <1-3> <path>` | Albedo for layer 1..3 (layer 0 = `map`) |
| `layerNormalMap` / `layerNormalHeightMap` | Normal (+ height in alpha for NH) |
| `layerOrmMap` / `layerOrmsMap` | Packed ORM for that layer |

Layer 0 reuses existing `map` / `normalMap`|`normalHeightMap` / `ormMap`.

## Runtime

- **`r_materialBlend`** (default **1**): master toggle; **0** forces layer0-only.
- **`r_materialBlendSharpness`** (default **8**): global override when &gt; 0; else shader `blendSharpness`.
- Startup log: `Material blend: ON (vertex + height)`.

## Behavior

1. Weights `w = max(vertexRGBA, 0)` (unused layers zeroed by layer count).
2. Per-layer height `h_i` from normal-map **alpha** when that layer used `*HeightMap`; else soft path.
3. Height-blend: `maxH = max(w_i + h_i)`, `w' = max(0, w_i + h_i - maxH + 1/sharpness)^sharpness`, renormalize.
4. Soft fallback (no heights): `w' = normalize(w)`.
5. Blend albedo / ORM / tangent normals by `w'`; **do not** tint albedo by vertex color.
6. Forces **`CGEN_EXACT_VERTEX`**; skips world SH overwrite so painted weights survive.

## Surfaces (v1)

Supported where vertex colors already exist: **BSP**, **glTF**, **IQM**. **MD3** uses optional `.md3.paint` sidecar (bind-pose).

## Studio paint authoring

- **`r_materialPaint` 1** + **`r_studio_tools` 1**: Studio / Paint panel.
- Brush cvars: **`r_materialPaintRadius`**, **`r_materialPaintStrength`**, **`r_materialPaintChannels`** (bitmask).
- Sidecar: **`maps/<map>.paint`** (magic `ID3P`, version 2; optional second RGBA stream for layers 4–7).
- Commands: **`paint_save`**, **`paint_load`**, **`paint_clear`**, **`paint_status`**.
- Sidecar is source of truth for blend weights (q3map2 vertex light can clobber BSP colors).

## Implementation notes

Descriptor slots for layers 1–3 reuse unused advanced-lobe / detail / deluxe bindings (no new descriptor sets). Layer-3 ORM uses a neutral constant `(1, 0.5, 0, 1)` when no dedicated slot remains. POM is disabled on blend pipelines. Texture arrays / layers 5–8: see phase-2 notes below when enabled.

See also [PBR_TEXTURES.md](PBR_TEXTURES.md), [MATERIAL_PERMUTATIONS.md](MATERIAL_PERMUTATIONS.md).

## Demo

- Shader: `examples/demo_game/mod/scripts/demo_material_blend.shader`
- Config: `exec demo_material_blend.cfg`
- Placeholder textures under `examples/demo_game/bootstrap_media/textures/demo/`
