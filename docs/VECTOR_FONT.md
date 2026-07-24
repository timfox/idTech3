# Vector font rendering (Vulkan)

Resolution-independent text for engine console and HUD without a baked glyph atlas.

## Quick start

```text
set r_font fonts/YourFont.ttf
set r_vectorFont 1
reloadTtf
vid_restart
```

Startup should log: `VectorFont: GPU outline text enabled for '…'` and `Vector font: loaded … (N curves, … curve texture)`.

Requires **Vulkan**, **FreeType** (`BUILD_FREETYPE`), and a valid **`r_font`** TTF path.

## Modes (`r_vectorFontMode`)

| Value | Name | Status |
|-------|------|--------|
| **0** | Lengyel 2017 | **Default** when `r_vectorFont 1` — curve texture + winding-number fragment shader (`uiVectorText`) |
| **2** | Loop & Blinn + mesh | **Implemented** — ear-clip solid fill + curve triangles; vertex fallback always, **`VK_NV_mesh_shader`** string dispatch when **`r_vk_meshShaderNV 1`** |

```text
set r_vectorFont 1
set r_vectorFontMode 2    // Loop & Blinn glyphlets (+ r_vk_meshShaderNV 1 for NV mesh dispatch)
// set r_vectorFontMode 0  // Lengyel winding-number (default)
```

## Mode 2 (Loop & Blinn + optional NV mesh dispatch)

1. **CPU (load):** FreeType outline → ear-clipped solid triangles + convex/concave curve triangles (`tr_vector_font_glyphlet.c`).
2. **Draw (fallback):** Expanded indexed triangles + `frag_ui_vector_glyphlet` (Loop & Blinn `u² − v` discard).
3. **Draw (NV mesh):** When **`r_vk_meshShaderNV 1`**, one **`CmdDrawMeshTasksNV`** per string from glyphlet SSBOs (`mesh_nv_ui_vector_font.mesh`).

Code: `src/renderers/vulkan/vk_vector_font.c`, shaders under `src/renderers/vulkan/shaders/glsl/`.

## Mode 0 (Lengyel JCGT 2017)

1. **CPU (load):** FreeType decomposes each glyph outline into quadratic Bézier segments; control points pack into a large float **`curveTexture`** (`fonts/_vcur_*`).
2. **Draw:** Each glyph is a screen-aligned quad; em-space UVs index the curve range (`curveStart`, `curveCount` push constants).
3. **GPU:** `frag_ui_vector_text.frag` evaluates robust **winding-number** coverage per pixel (no triangulation, no discard-per-curve triangles).

Code: `src/renderers/common/tr_vector_font.c`, `src/client/cl_vector_font.c`, shader `src/renderers/vulkan/shaders/glsl/frag_ui_vector_text.frag`.

## Planned extensions

- Full HarfBuzz face caching (`BUILD_HARFBUZZ`) for complex scripts.
- Horizontal/vertical band lists beyond dual sorted early-exit.
- `VK_EXT_mesh_shader` (AMD RDNA) for mode 2.
- See [VECTOR_FONT_RENDERING.md](VECTOR_FONT_RENDERING.md) for the production Lengyel contract.

## Comparison to other text paths
|------|-------|-------|
| FreeType atlas | `cl_builtInTtf 1`, `r_font` | Default; CPU raster each reload |
| SDF | `r_sdfEnable 1`, `r_sdfFont` | Pre-baked `.fnt` + atlas |
| **Vector (this doc)** | **`r_vectorFont 1`** | GPU outlines; best when scaling text sharply |

Priority when multiple are on: FreeType atlas wins unless `cl_builtInTtf 0` (see [RENDERERS.md](RENDERERS.md) HUD section).

## Client integration

- `SCR_DrawStringExt` / small console strings try **`VectorFont_DrawStringExt`** when `r_vectorFont 1`.
- Shadow: `r_fontShadow` (pixels or virtual units depending on path).

## Related research

- [recker2009-gpu-rip-fonts.md](research/recker2009-gpu-rip-fonts.md) — CPU outline → GPU span fill (print RIP).
- [amd-gpuopen-loop-blinn-mesh-fonts.md](research/amd-gpuopen-loop-blinn-mesh-fonts.md) — mesh shader + Loop & Blinn.
