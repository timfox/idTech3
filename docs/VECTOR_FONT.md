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
| **2** | Loop & Blinn + mesh | **Not implemented** — falls back to mode 0; see [research/amd-gpuopen-loop-blinn-mesh-fonts.md](research/amd-gpuopen-loop-blinn-mesh-fonts.md) |

```text
set r_vectorFont 1
set r_vectorFontMode 0    // Lengyel (default)
// set r_vectorFontMode 2  // future: AMD GPUOpen glyphlet + mesh shader per string
```

## How mode 0 works (Lengyel JCGT 2017)

1. **CPU (load):** FreeType decomposes each glyph outline into quadratic Bézier segments; control points pack into a large float **`curveTexture`** (`fonts/_vcur_*`).
2. **Draw:** Each glyph is a screen-aligned quad; em-space UVs index the curve range (`curveStart`, `curveCount` push constants).
3. **GPU:** `frag_ui_vector_text.frag` evaluates robust **winding-number** coverage per pixel (no triangulation, no discard-per-curve triangles).

Code: `src/renderers/common/tr_vector_font.c`, `src/client/cl_vector_font.c`, shader `src/renderers/vulkan/shaders/glsl/frag_ui_vector_text.frag`.

## Planned mode 2 (AMD GPUOpen / Loop & Blinn)

[AMD’s mesh-shader font article](https://gpuopen.com/learn/mesh_shaders/mesh_shaders-font-rendering/) describes:

- **Loop & Blinn (2005):** CDT solid fill + convex/concave curve triangles; fragment discard with `u² − v` on canonical Bézier coords.
- **Mesh shaders:** Per-primitive `triangleType`; **one dispatch renders an entire string** from glyphlet SSBOs.

Prerequisites not yet in-tree:

- `VK_EXT_mesh_shader` pipelines (RDNA) — today only optional **`VK_NV_mesh_shader`** enablement via `r_vk_meshShaderNV` (no draw path).
- Glyphlet build (vertex + index + per-primitive-type buffers) at font load.
- Optional `VK_KHR_fragment_shader_barycentric` for a non-mesh Loop & Blinn fallback.

See [research/amd-gpuopen-loop-blinn-mesh-fonts.md](research/amd-gpuopen-loop-blinn-mesh-fonts.md).

## Comparison to other text paths

| Path | Cvars | Notes |
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
