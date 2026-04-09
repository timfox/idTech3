# glTF GPU Upload – Implementation Plan

## Current State (Updated)

- **Loader**: `tr_model_gltf.c` parses glTF/GLB via cgltf, produces `gltfModel_t` (meshes, materials, skeleton, animations, morph targets).
- **Registration**: `R_RegisterGLTF` sets `mod->type = MOD_GLTF`, stores `gltfRenderData_t` in `mod->modelData` (hunk), creates surfaces per primitive with material→shader mapping (baseColorTexture). Loads PBR textures (normal, metallic-roughness).
- **VBO**: Static meshes (no skinning, no morph) get device-local vertex+index buffers via `vk_create_gltf_buffers`. `RB_GLTFSurface` uses VBO path when available.
- **Rendering**: MOD_GLTF → `R_AddGLTFSurfaces` → `RB_GLTFSurface`. VBO path binds per-primitive buffers; tess path used for skinned/morph/fallback.
- **Skeletal animation**: CPU skinning in `RB_GLTFSurface` when `hasSkinning`. `R_ComputeGLTFJointMatrices` / `R_ComputeGLTFJointMatricesBlend` sample TRS channels on `skins[0]` joints and multiply world * inverseBindMatrix.
- **Morph targets**: Loaded from `primitive.targets`; CPU blend in tess path from glTF weight animation and/or `RE_SetEntityMorphWeight` (mesh `target_names`). GPU morph not implemented.
- **Bounds**: Computed from mesh vertices; `R_GLTFModelBounds` used for culling and fog.

## Gaps (vs Northlight / Full glTF Support)

| Component | Status | Required |
|-----------|--------|----------|
| Model data storage | ✅ Hunk `gltfRenderData_t` | - |
| Model type | ✅ `MOD_GLTF` | - |
| Vertex/index VBOs | ✅ Device-local buffers for static meshes | - |
| Texture loading | ✅ baseColor, normal, metallicRoughness | - |
| Material → shader | ✅ baseColorTexture → shader | PBR multi-texture bind at draw |
| Render path | ✅ VBO + tess | - |
| Bounds | ✅ `R_GLTFModelBounds` | - |
| Skeletal animation | ✅ CPU skinning + TRS clip sampling | GPU skinning |
| Morph targets | ✅ CPU blend (weights + entity) | GPU morph |

## Implementation Steps

### Phase 1: Basic Static Mesh Rendering
1. Add `MOD_GLTF` to `modtype_t` in `tr_local.h`.
2. In `R_RegisterGLTF`: Hunk-alloc `gltfModel_t`, copy data, store in `mod->modelData`, set `mod->type = MOD_GLTF`.
3. Add `R_AddGLTFSurfaces(trRefEntity_t *ent)` – iterate meshes/primitives, add draw surfaces.
4. Add `R_GetModelBounds` branch for `MOD_GLTF` using `boundsMin`/`boundsMax`.
5. Create VBO upload path: for each primitive, create vertex + index buffers (reuse IQM/VBO patterns).
6. Map materials to shaders: use `baseColorTexture` path for `RE_RegisterShaderNoMip` or equivalent; fallback to default PBR shader.
7. Add `RB_GLTFSurface` or use existing mesh surface type with glTF-specific data.

### Phase 2: Textures and PBR
1. Load `baseColorTexture`, `normalTexture`, `metallicRoughnessTexture` as `image_t`.
2. Build PBR shader or use existing PBR shader with material uniforms.
3. Handle `alphaMode` (OPAQUE, MASK, BLEND).

### Phase 3: Skinning and Animation (Optional)
1. Upload joint matrices to shader.
2. Implement vertex skinning in shader.
3. Support keyframe animation.

## Reference Files

- `tr_model_gltf.c` – loader, `gltfModel_t` layout
- `tr_model_iqm.c` – `R_AddIQMSurfaces`, `srfIQModel_t`, `RB_IQMSurfaceAnim`
- `vk_vbo.c` – VBO creation
- `tr_backend.c` – draw call dispatch
