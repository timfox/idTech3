# glTF 2.0 support (engine status)

This document describes **what the engine actually does today** for glTF / GLB assets. It complements the high-level feature list in the root `README.md`.

## Where it runs

| Renderer | glTF / GLB |
|----------|------------|
| **Vulkan** | Yes — `.gltf` and `.glb` registered in `tr_model.c`, loaded via **cgltf** (`tr_model_gltf.c`). |
| **OpenGL** | **No** — there is no `R_RegisterGLTF` path in the OpenGL renderer. |

Use the Vulkan build for glTF content.

## What works well

- **Triangle meshes** with positions, normals, tangents, two UV sets, vertex colors.
- **Indices** (indexed geometry).
- **Multiple meshes / primitives** per file (subject to caps below).
- **Materials (partial)**: metallic-roughness base color, normal map, metallic-roughness texture, emissive, occlusion; factors and texture **URIs** are read. Base color drives the registered shader; extra maps are loaded where wired in `R_RegisterGLTF`.
- **Skinned meshes (bind pose)**: skeleton from **first skin only** (`skins[0]`), inverse bind matrices, up to **4 influences** per vertex, joint indices/weights from standard attributes.
- **Static mesh fast path**: when a primitive has **no skinning and no morph targets**, geometry can use **Vulkan VBOs** (`vk_create_gltf_buffers`) if creation succeeds. Toggle: **`r_gltfVBO`** (default `1`).

## Known limitations (important)

### 1. Animation clips are not played

- Animation data is **parsed and stored** on the model (`gltfAnimation_t`, channels, keyframes, duration).
- **Draw code does not sample animations.** Joint matrices come from `R_ComputeGLTFJointMatrices()`, which uses the skeleton’s **rest/bind TRS** and inverse bind matrices only.
- **Expectation**: characters appear in **bind pose**, not animated over time, until a playback path is implemented.

### 2. Morph targets (blend shapes) are not blended

- Morph target deltas may be **loaded** (per primitive, up to `GLTF_MAX_MORPH_TARGETS`).
- The renderer **does not** apply morph weights at draw time; primitives with morph targets avoid the VBO fast path and still draw **base** geometry only.

### 3. Single skin

- Only **`skins[0]`** is loaded. Multi-skin assets are not fully supported.

### 4. Hard caps (truncation)

Defined in `tr_model_gltf.h`:

| Limit | Value |
|--------|--------|
| `GLTF_MAX_JOINTS` | 128 |
| `GLTF_MAX_MORPH_TARGETS` | 8 per primitive |
| `GLTF_MAX_MESHES` | 256 |
| `GLTF_MAX_MATERIALS` | 64 |

Larger assets are **silently clamped** during load.

### 5. Textures and embedded images

- Texture filenames are taken from **`image->uri`** when present.
- **Embedded bufferView images** (common inside `.glb`) are **not** turned into loadable paths automatically; prefer **external** image files (e.g. alongside the `.gltf`) or a pipeline that extracts them, unless/until the loader gains bufferView → image upload.
- **KTX2 / Basis Universal / exotic extensions** are not handled unless exposed as normal image files the engine already loads.

### 6. Material extensions vs shading

- The loader reads several **KHR-style fields** (e.g. clearcoat, sheen, transmission, IOR scalars) into `gltfMaterial_t`.
- The **full** glTF extension stack is **not** guaranteed to match **every** nuance in the Vulkan PBR shaders. Treat extension fields as **forward-compatible data** until verified per material.

### 7. CPU cost for skinned content

- If **skinning** or **morph** is flagged, the mesh goes through the **tessellation (CPU) path** in `RB_GLTFSurface` — no skinning GPU path for glTF in the current code.

## Engine references

- Loader / registration: `src/renderers/vulkan/tr_model_gltf.c`, `tr_model_gltf.h`
- Draw: `RB_GLTFSurface` in `src/renderers/vulkan/tr_surface.c`
- GPU buffer helper: `src/renderers/vulkan/vk_gltf.c`

## README alignment

The README lists **GLTF** under model formats and mentions blend shapes for IQM/GLTF. For glTF specifically:

- **Skeletal skinning** in bind pose: supported.
- **Skeletal animation playback**: not yet.
- **Blend shapes on glTF**: data may load; **runtime blending** not yet.

When implementing animation or morph playback, update this file and the README bullet so marketing and engineering stay in sync.
