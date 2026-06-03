# FreeUSD integration

[idTech3](https://github.com/gopexllc/idtech3) embeds [FreeUSD](https://github.com/gopexllc/FreeUSD) (GPL-2.0-or-later) for **USDA-first** scene I/O, runtime assessment, and **UsdGeom mesh** import into the engine’s MD3-style mesh path.

This is **not** a full OpenUSD / Hydra / USD Imaging stack. Scope matches FreeUSD’s [engine-supported subset](https://github.com/gopexllc/FreeUSD/blob/main/docs/engine-supported-subset.md).

## Build

```bash
./scripts/compile_engine.sh vulkan          # USE_FREEUSD=ON by default
cmake -DUSE_FREEUSD=OFF ...                 # disable (smaller link; no usd_* tools)
```

First configure **fetches** `gopexllc/FreeUSD` via CMake `FetchContent` (`cmake/FreeUSD.cmake`).

## Mesh models (renderer)

| Extension | Loader |
|-----------|--------|
| `.usda`, `.usd` | `R_RegisterFreeusdMesh` when `USE_FREEUSD` (first tessellatable `UsdGeom.Mesh` in composed scene) |
| fallback | ASCII vertex soup (`tr_model_mesh_import.c`) if FreeUSD fails or is off |

- World-space positions use composed `local_to_world` from `BuildEngineSceneSnapshot`.
- N-gons are fan-triangulated; result is a static MD3 mesh (no skeleton/animation yet).
- `primvars:st` applied when point count matches; `material:binding` → `UsdPreviewSurface` diffuse texture → Q3 shader name (`r_freeusdShaderMap`).
- Arbitrary binary `.usdc` scene decode is **not** supported unless the root layer is USDA or FreeUSD’s embedded-crate path applies.

## Client console

Requires `USE_FREEUSD` build and `com_freeusd 1` (default).

| Command | Purpose |
|---------|---------|
| `usd_info <path.usda>` | Stage metadata, prim/material counts |
| `usd_assess <path.usda>` | `AssessEngineRuntimeSupport` (layer stack, variants, PreviewSurface, Lux, physics, warnings) |
| `usd_entities <path.usda>` | Prim hierarchy (`com_usdEntities 1`) |
| `usd_shaders <path.usda>` | Materials and geom bindings (`com_usdShaders 1`) |
| `usd_meshes <path.usda>` | List tessellatable `UsdGeom.Mesh` prims and triangle counts |
| `usd_load <path.usda> [index]` | `RegisterModel` via renderer (optional mesh index; needs `r_freeusd` 1) |
| `usd_shader_map <path.usda>` | Print mesh prim → resolved Q3 shader (preview of import mapping) |

### Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_freeusd` | `1` | Renderer: use FreeUSD for `.usd`/`.usda` mesh import |
| `r_freeusdPickLargest` | `1` | Pick mesh with most triangles (0 = use `r_freeusdMeshIndex`) |
| `r_freeusdMeshIndex` | `0` | Mesh index from `usd_meshes` when `r_freeusdPickLargest` is 0 |
| `r_freeusdTime` | `1.0` | USD time code for composed mesh samples |
| `r_freeusdMeshPath` | `` | Optional prim-path substring filter |
| `r_freeusdShaderMap` | `1` | Map `UsdPreviewSurface` + diffuse texture to Q3 shader path on import |
| `com_freeusd` | `1` | Master toggle for USD tools |
| `com_usdEntities` | `1` | Enable `usd_entities` listing |
| `com_usdShaders` | `1` | Enable `usd_shaders` listing |

Startup logs FreeUSD version in the client (`com_freeusd`) and renderer (`r_freeusd`).

Disable mesh import at runtime with `r_freeusd 0` (falls back to ASCII vertex soup). Disable tools with `com_freeusd 0`.

## Test asset

`tests/data/usd/parity_geom_mesh.usda` — single triangle mesh (from FreeUSD parity fixtures).

## License note

FreeUSD is **GPL-2.0-or-later**. With `USE_FREEUSD=ON`, the Vulkan renderer plugin and client link FreeUSD static libraries; distribution must comply with GPL obligations for those binaries.
