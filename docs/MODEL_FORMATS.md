# Model Format Support

## Supported Formats (7 total)

| Format | Extension | Features | Library |
|--------|-----------|----------|---------|
| glTF 2.0 | `.gltf`, `.glb` | PBR materials, skeletal animation, morph targets | cgltf (MIT) |
| Wavefront OBJ | `.obj` | Vertices, normals, texcoords, materials | tinyobjloader-c (MIT) |
| MD5 Mesh | `.md5mesh` | Skeletal mesh with weighted vertices | Built-in parser |
| IQM | `.iqm` | Inter-Quake Model, skeletal animation | Built-in |
| MDR | `.mdr` | Modified Renderable (Q3 extended) | Built-in |
| MD3 | `.md3` | Quake III Arena standard model | Built-in |

## Loader Priority

When a model is requested without extension, the engine tries formats in this order:
1. glTF (`.gltf`, `.glb`)
2. OBJ (`.obj`)
3. MD5 (`.md5mesh`)
4. IQM (`.iqm`)
5. MDR (`.mdr`)
6. MD3 (`.md3`)

## glTF 2.0 Details

**Renderer**: glTF loading and the full material/animation path are **Vulkan-only** (see [GLTF.md](GLTF.md)); OpenGL does not register `.gltf`/`.glb`.

**Materials and animation (summary)**:
- Base color, metallic/roughness, normal, emissive, occlusion maps
- Extensions: clearcoat, sheen, transmission, IOR, emissive strength (loader reads many KHR fields; shader coverage varies - see GLTF.md)
- Skeletal animation with joint hierarchy and inverse bind matrices; runtime clip playback and GPU skin on Vulkan PBR when enabled
- Keyframe animations (translation, rotation, scale) and morph targets (see GLTF.md for caps and `r_gltf*` cvars)
- GLB binary container support (embedded bufferView images remain a known loader gap - prefer external images or see GLTF.md)

## MD5 Details

Text-based format from id Tech 4 (Doom 3):
- `MD5Version 10` parser
- Joint hierarchy with quaternion orientations
- Weighted vertex skinning (multiple joints per vertex)
- Bind pose mesh generation

## IQM Morph Sidecar (`.morph`)

Vulkan renderer supports IQM morph targets through a sidecar file next to the `.iqm`:
- Example: `models/creature.iqm` + `models/creature.morph`
- Missing or invalid sidecar falls back safely to non-morph IQM rendering.

### Binary Layout (Version 1)
All values are little-endian.

1. Header:
- 8 bytes magic: `IQMMORPH`
- `u32 version` (currently `1`)
- `u32 numTargets`
- `u32 numSurfaces`
- `u32 flags` (`bit0` = normal deltas present, required)
2. Target table:
- `numTargets` entries of fixed 64-byte target names
3. Surface table:
- `numSurfaces` entries:
- fixed 64-byte surface name
- `u32 numVertexes` (must match IQM surface vertex count)
4. Payload:
- For each sidecar surface, for each target:
- dense `deltaPos` float array (`numVertexes * 3`)
- dense `deltaNorm` float array (`numVertexes * 3`)

### Runtime Controls
- `r_morph` (0/1): master toggle
- `r_morphMaxActive` (1..4): top-K active targets evaluated per entity
- `r_morphLodStart`: start distance for morph fade
- `r_morphLodEnd`: end distance (morph weight reaches 0)
- `r_morphDebug` (0/1): debug coloring by morph strength
- `r_morphBreath` (0/1), `r_morphBreathAmp`, `r_morphBreathFreq`: optional procedural demo drive for target name `breath`

### Renderer API
- `re.SetEntityMorphWeight(const refEntity_t *ent, const char *name, float weight)`
- Queues per-entity morph weights for the current scene submission.
- Weights are deduplicated by channel hash and clamped.
