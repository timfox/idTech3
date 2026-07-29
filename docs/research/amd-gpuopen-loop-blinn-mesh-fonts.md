# AMD GPUOpen — Loop & Blinn fonts via mesh shaders (RDNA)

**Source:** [Font- and vector-art rendering with mesh shaders](https://gpuopen.com/learn/mesh_shaders/mesh_shaders-font-rendering/) (AMD GPUOpen, March 2024). Authors: Quirin Meyer, Bastian Kuth, Max Oberberger.

**SIGGRAPH reference:** Loop & Blinn, *Resolution independent curve rendering using programmable graphics hardware* (2005).

## What the article adds

| Topic | Vertex-pipeline Loop & Blinn | Mesh-shader variant (AMD) |
|--------|---------------------------|---------------------------|
| Curve fill | Rasterize triangles; fragment shader discards outside quadratic Bézier (`u² − v`) | Same discard logic |
| Canonical `(u,v)` | Stored per vertex (duplicated at shared endpoints) | Recomputed from **`SV_Barycentrics`** in the pixel shader |
| Triangle kinds | Solid / convex curve / concave curve → **3 draw calls per glyph** | **Per-primitive attribute** (`triangleType`) → **1 draw per glyph** |
| Whole string | One draw per character (API overhead) | **One mesh-shader dispatch per string** (glyphlets + `CharacterRenderInfo` buffer) |

Glyph preprocessing: TrueType outlines → constrained Delaunay triangulation of control points + curve triangles classified convex vs concave.

## Relation to this engine

| Path | Status | Algorithm |
|------|--------|-----------|
| FreeType atlas | Default (`cl_builtInTtf` 1) | CPU raster → textured quads |
| SDF atlas | Optional (`r_sdfEnable`) | Pre-baked distance field |
| **Lengyel 2017** | **`r_vectorFont 1`** (Vulkan) | Curve control points in a float texture; fragment shader **winding-number** coverage (no triangulation) |
| **Loop & Blinn + mesh** | **Planned** (`r_vectorFontMode 2`) | Glyphlet buffers + `VK_EXT_mesh_shader` / task+mesh pipeline; needs barycentrics or per-primitive IDs |

Today:

- `r_vectorFont 1` already gives **resolution-independent** console/HUD text without atlases (see [VECTOR_FONT.md](../VECTOR_FONT.md)).
- `r_vk_meshShaderNV 1` enables **`VK_NV_mesh_shader`** at device create for virtual-geometry/font experiments; the portable production virtual-geometry path remains indexed MDI.
- AMD RDNA uses **`VK_EXT_mesh_shader`** (not in our bundled `vulkan_core.h` snapshot); NVIDIA path is the first experimental hook.

## Implementation checklist (future)

1. **Offline / load-time:** For each code point, build glyphlet `{ vertex[], index[], perPrimitiveType[] }` + `GlyphletInfo` table (AMD `StructuredBuffer` layout).
2. **Extensions:** `VK_EXT_mesh_shader` (AMD) or `VK_NV_mesh_shader` (NVIDIA); `VK_KHR_fragment_shader_barycentric` for Loop & Blinn without mesh (fallback).
3. **Shaders:** Mesh shader emits `SetMeshOutputCounts` + `outputPrimAttr[tid].triangleType`; pixel shader matches AMD appendix (`computeUV(bary)`, discard on `u² − v`).
4. **Runtime:** Upload `CharacterRenderInfo[]` (position + code point); `dispatch(stringLength, 1, 1)` thread groups.
5. **Toggle:** `r_vectorFontMode 2`, startup log, graceful fallback to mode 1.

## Why not replace Lengyel immediately?

- Lengyel avoids CDT and per-glyph triangle meshes; one quad + curve texture per glyph is already simple and cross-vendor.
- Mesh-shader string dispatch wins when **many glyphs** are drawn per frame and API overhead dominates; our HUD/console strings are modest length.
- Both are valid; mesh + Loop & Blinn is the better fit when **`VK_EXT_mesh_shader`** is a hard requirement (AMD doc target).

## Disclaimers (from AMD)

Not a production-grade global glyph renderer; edge cases and performance tuning are out of scope for the blog post. Treat as architecture reference for per-primitive attributes and on-chip string mesh generation.
