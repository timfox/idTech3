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

Full PBR material support:
- Base color, metallic/roughness, normal, emissive, occlusion maps
- Extensions: clearcoat, sheen, transmission, IOR, emissive strength
- Skeletal animation with joint hierarchy and inverse bind matrices
- Keyframe animations (translation, rotation, scale)
- GLB binary container support

## MD5 Details

Text-based format from id Tech 4 (Doom 3):
- `MD5Version 10` parser
- Joint hierarchy with quaternion orientations
- Weighted vertex skinning (multiple joints per vertex)
- Bind pose mesh generation
