# glTF 2.0 support (engine status)

This document describes **what the engine actually does today** for glTF / GLB assets. It complements the high-level feature list in the root `README.md`.

## Where it runs

| Renderer | glTF / GLB |
|----------|------------|
| **Vulkan** | Yes — `.gltf` and `.glb` registered in `tr_model.c`, loaded via **cgltf** (`tr_model_gltf.c`). Full path: optional **device VBOs**, **PBR GPU** skin/morph (`r_gltfGpu`), qtangent recompute on CPU tess when needed. |
| **OpenGL** | Yes — same loader and `MOD_GLTF` / `R_AddGLTFSurfaces`; draws via **CPU tessellation** in `tr_gltf_rb_opengl.c` (no `r_gltfGpu`, no Vulkan VBO upload). Morph evaluation gated by **`r_morph`**; clip timing uses **`r_gltfAnim`** (same as Vulkan). |

Use Vulkan for **PBR GPU** glTF and maximum parity with advanced materials; OpenGL is suitable for bringing glTF entities up on the compatibility backend.

## What works well

- **Triangle meshes** with positions, normals, tangents, two UV sets, vertex colors.
- **Indices** (indexed geometry).
- **Multiple meshes / primitives** per file (subject to caps below).
- **Materials (partial)**: metallic-roughness base color, normal map, metallic-roughness texture, emissive, occlusion; factors and texture **URIs** are read. Base color drives the registered shader; extra maps are loaded where wired in `R_RegisterGLTF`.
- **Skinned meshes (bind pose)**: skeleton from **first skin only** (`skins[0]`), inverse bind matrices, up to **4 influences** per vertex, joint indices/weights from standard attributes.
- **Vulkan VBO path**: primitives upload **device-local** vertex/index buffers (`vk_create_gltf_buffers`) with **joint indices/weights** packed for optional GPU skinning. When those buffers exist, the renderer uses them for static draws (skin/morph still use the tessellation path as needed).

## Runtime animation and morph (Vulkan)

- **Clip selection**: `refEntity_t.frame` chooses the animation clip **by index** into `gltfModel_t.animations[]` (first clip = `0`). Use **`RF_WRAP_FRAMES`** so the index wraps modulo clip count; otherwise out-of-range indices clamp to clip `0`.
- **Time**: clip time is `refEntity.shaderTime` (seconds) when set, else `refdef.time * 0.001f`, scaled by cvar **`r_gltfAnim`** (default `1`). Time **loops** by each clip’s stored duration.
- **Cross-clip blend**: `oldframe` selects the second clip; **`backlerp`** blends joint TRS (and morph weights from weight tracks) between **current** and **old** clip at the **same** clock time.
- **Skeletal sampling**: translation/rotation/scale channels on skin joints update local pose, then **`world * inverseBindMatrix`** joint matrices are computed each draw.
- **GPU skin + morph (Vulkan PBR)**: when **`r_gltfGpu`** is `1`, **`vk.pbrActive`**, and the surface shader is PBR, `RB_GLTFSurface` uploads joint matrices to the same **SSBO slot** as IQM skin data, packs **top-8** morph targets by **combined** weight (glTF animation + **`RE_SetEntityMorphWeight`** / `morphChannelCount` + mesh defaults) into the shared IQM-style morph SSBO (`IQM_MORPH_TOP_K`), and draws with **`USE_GLTF_GPU_SKIN`** vertex shaders (`gen_vert.tmpl`). Vertex count must be ≤ **`SHADER_MAX_VERTEXES`**. **Tangent / qtangent**: bind-pose tangents from the asset feed the shader; when **`r_gltfGpuTangentFix`** is `1` (default), after joint skin + morph the vertex shader **Gram–Schmidt**-orthonormalizes **T** against the deformed **N** (closer to CPU tess qtangent behavior than raw bind-pose **T**). Set **`r_gltfGpuTangentFix 0`** for the legacy bind-pose-only tangent path.
- **CPU tess fallback**: used when GPU path is disabled, storage/index upload fails, or for non-PBR shaders. After CPU morph + skinning, **qtangent** is recomputed from deformed positions and **TEXCOORD_0** when PBR is active.
- **Morph weights**: primitives load **mesh `target_names`** when present; `RE_SetEntityMorphWeight(ent, name, w)` matches those names (same hash path as IQM). **glTF weight animation** channels on the mesh node add to the same weight array. **`mesh.weights`** default blend shape weights from the file are added as a baseline.

## Known limitations (important)

### 1. Animation scope

- Only **`skins[0]`** joint nodes receive TRS channels (same as skeleton load). Other animated nodes are ignored for skinning.
- **Morph weight** animation must target the **mesh node** that owns the morph targets (glTF convention).

### 2. Morph targets (blend shapes)

- Up to **`GLTF_MAX_MORPH_TARGETS`** (8) per primitive. **POSITION** and **NORMAL** deltas blend on the CPU; **TANGENT** morph deltas are not accumulated (PBR uses recomputed qtangent from the deformed mesh + UV0).

### 3. Single skin (unchanged)

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

### 7. GPU vs CPU path

- **Skinned / morphed** primitives prefer the **GPU path** under Vulkan PBR when **`r_gltfGpu 1`** and constraints above are met; otherwise **`RB_GLTFSurface`** falls back to **CPU tess**.
- **More than eight** non-zero morph weights per vertex: GPU path keeps only the **eight largest** weights per draw (same cap as IQM `IQM_MORPH_TOP_K`; tune `r_morphMaxActive` for IQM batching only).

## Engine references

- Loader / registration: `src/renderers/vulkan/tr_model_gltf.c`, `tr_model_gltf.h`
- Draw: `RB_GLTFSurface` in `src/renderers/vulkan/tr_surface.c`
- GPU buffer helper: `src/renderers/vulkan/vk_gltf.c`

## README alignment

The README lists **GLTF** under model formats and mentions blend shapes for IQM/GLTF. For glTF specifically:

- **Skeletal skinning** with **clip playback** (see above): supported on Vulkan when clips exist and `frame` selects a valid index.
- **Blend shapes on glTF**: runtime blending via **animation weights** and/or **`RE_SetEntityMorphWeight`** when `target_names` are present.

When implementing animation or morph playback, update this file and the README bullet so marketing and engineering stay in sync.
