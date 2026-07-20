# Authored / hard-edge mesh normals (experimental)

Optional geometry pipeline that preserves **authored vertex normals**, **custom split
normals**, and **crease-angle hard edges** for imported models. This is **not** a
screen-space outline or Sobel filter.

**Certification:** experimental quality opt-in. Default remains legacy-compatible
(`r_meshNormalPolicy 0`). Do **not** enable in `modern_vulkan.cfg` until DCC
reference validation passes.

## Enable

```
exec vulkan_overlay_authored_normals.cfg
vid_restart
```

Then reload models (map change). Policy cvars are **latched**.

Recovery: `exec modern_vulkan.cfg` then `vid_restart`.

## CVars (implemented)

| CVar | Default | Meaning |
|------|---------|---------|
| `r_meshNormalPolicy` | `0` | `0` legacy (certified). `1` preserve authored glTF normals. `2` preserve or crease-angle generate + splits. `3` force crease-angle. `4` preserve + debug logs. |
| `r_preserveCustomNormals` | `1` | Keep valid glTF `NORMAL` attributes when policy ≥ 1. |
| `r_hardEdgeAngle` | `60` | Crease angle (degrees) for policies 2–3. |
| `r_splitTangentsAtHardEdges` | `1` | After normal finalization, regenerate tangents with MikkTSpace. |
| `r_meshNormalDebug` | `0` | Log per-model stats on glTF load. |

Not exposed yet (no runtime behavior): `r_preserveSmoothingGroups`,
`r_splitNormalsAtUvSeams`, `r_splitNormalsAtMaterialBoundaries`.

## Diagnostics

```
mesh_normal_status
havenrp_renderer_status   // meshNrm line
```

## Format coverage

| Format | Behavior |
|--------|----------|
| **glTF / GLB** | Policy processing after load. Authored `NORMAL` / `TANGENT` preserved when present. Crease path expands corners and welds only soft partitions with matching UV/skin keys. Morph-target prims skip topology expansion. |
| **OBJ** | Already expands corners by `(v, vt, vn)` — no position-only weld. Authored `vn` stored as MD3 lat/long. Policy module does not reprocess OBJ yet. |
| **MD3 / MDC / IQM** | Untouched (legacy). |
| **BSP / patches** | Untouched (legacy). Do not harden tessellation edges inside smooth patches. |
| **USD / FreeUSD** | Depends on exporter writing split normals into the mesh snapshot; no separate Maya hard-edge metadata. |

## Maya / DCC export contract

The engine preserves the **result** of Soften/Harden Edge, Set to Face, Average /
Weighted Normals, and locked custom normals **only if the interchange file
contains split vertices and `NORMAL` values**.

### Recommended glTF path (Maya)

1. Author hard/soft edges in Maya (Mesh Display → Soften / Harden Edge).
2. Export glTF/GLB with an exporter that writes the final `NORMAL` attribute and
   **duplicates vertices** at normal discontinuities (and UV seams).
3. Verify in a glTF viewer that a hard cube has distinct normals per face corner
   (typically ≥ 24 vertices for a box, not 8).
4. Load in-engine with `r_meshNormalPolicy 1`.

Do **not** rely on Maya-specific smoothing-group plugs at runtime if the file
already encodes splits.

### OBJ

Export with `vn` and per-face `f v/vt/vn` indices. Smoothing groups alone (no
`vn`) are not yet consumed by a dedicated generator.

## Pipeline (glTF)

```
cgltf attributes → gltfVertex_t[] (indexed)
        ↓ policy ≠ 0
  preserve / fill missing / crease-split
        ↓ r_splitTangentsAtHardEdges
  MikkTSpace (final normals + UVs)
        ↓
  existing GPU upload / skinning / RT pack
```

Vertex identity for crease compaction includes position, soft-normal partition,
UV0, and skin joints/weights. Soft geometric edges may share an averaged normal
across a UV seam while keeping separate GPU vertices for tangent discontinuity.

## Renderer parity

Raster paths consume the same model vertex normals/tangents after upload.
Forward+, deferred, and clustered modes do not regenerate geometric normals per
draw. Ray-traced entity packs should use the same vertex attributes; face-flat
fallback in RT packs remains a known gap when policy is active but RT rebuild
ignores authored N.

## Validation assets (recommended)

Export from Maya/Blender and compare viewport vs engine:

- Cube all-hard / all-soft / mixed edges
- Beveled box, cylinder (hard caps, soft sides)
- Weighted-normal hard-surface prop
- Mirrored UV + normal-mapped hard edge
- Multi-material boundary, negative-scale instance, skinned hard-surface

Measure angular error vs source normals and seam luminance only with captured
references — do not invent numbers.

## Remaining gaps (ranked)

1. **Shading correctness** — RT/path hit geometric vs authored normal parity
2. **Import coverage** — OBJ smoothing groups; IQM/MD5 policy hook; USD path
3. **Tangent parity** — mirrored UV certification; debug magenta invalid TBN views
4. **Skinning** — verify splits survive CPU/GPU skin + previous-frame motion
5. **LOD / meshlets** — ensure simplifiers do not weld hard edges
6. **Memory** — crease expansion vertex growth; no versioned mesh cache yet

## Certification decision

**Experimental** — quality opt-in overlay only. Certified default remains
`r_meshNormalPolicy 0`.
