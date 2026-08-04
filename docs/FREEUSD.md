# FreeUSD integration

[idTech3](https://github.com/gopexllc/idtech3) embeds [FreeUSD](https://github.com/gopexllc/FreeUSD) (GPL-2.0-or-later) for **USDA-first** scene I/O, runtime assessment, and **UsdGeom mesh** import. The current MD3-style model path is a compatibility bridge, not the 2027 scene representation.

This is **not** a full OpenUSD / Hydra / USD Imaging stack. Scope matches FreeUSD’s [engine-supported subset](https://github.com/gopexllc/FreeUSD/blob/main/docs/engine-supported-subset.md).

## Build

```bash
git submodule update --init third_party/FreeUSD   # preferred library source
./scripts/compile_engine.sh vulkan               # USE_FREEUSD=ON by default; auto-inits submodule
cmake -DUSE_FREEUSD=OFF ...                        # disable (smaller link; no usd_* tools)
./scripts/compile_engine.sh vulkan nofreeusd       # same via compile script
```

**Library source** (`cmake/FreeUSD.cmake`):

1. **Git submodule** at `third_party/FreeUSD` → [gopexllc/FreeUSD](https://github.com/gopexllc/FreeUSD) (pinned commit in parent repo).
2. Legacy fallback path `src/external/FreeUSD` if present.
3. **FetchContent** fallback if the submodule is not initialized (network on first configure).

`./scripts/init_optional_submodules.sh --freeusd` (or `--all`) initializes the submodule without building.

### Platform matrix

| Platform | FreeUSD |
|----------|---------|
| Linux / macOS CMake | **ON** by default (`game`/`full`/`research` profiles) |
| Windows MinGW (CMake) | **ON** by default |
| Android NDK | **OFF** by default (desktop-oriented; CI passes `-DUSE_FREEUSD=OFF`) |
| Hand MSVC (`engine/platform/win32/msvc2017`) | **Stub only** — no `USE_FREEUSD`; `usd_*` prints “not built”. Use CMake/`compile_engine.sh` on Windows for real FreeUSD. |

`core` profile sets `USE_FREEUSD=OFF` for a faster Q3/OA path.

## Mesh models (compatibility bridge)

| Extension | Loader |
|-----------|--------|
| `.usda`, `.usd` | `R_RegisterFreeusdMesh` when `USE_FREEUSD` (first tessellatable `UsdGeom.Mesh` in composed scene) |
| fallback | ASCII vertex soup (`tr_model_mesh_import.c`) if FreeUSD fails or is off |

- World-space positions use composed `local_to_world` from `BuildEngineSceneSnapshot`.
- `EngineSceneNode.world_bound` includes `UsdGeom.Mesh` point bounds and unions descendant geometry for transform-only assemblies (district residency / sector grids).
- N-gons are fan-triangulated; result is a static MD3 mesh (no skeleton/animation yet).
- `primvars:st` applied when point count matches; `material:binding` → `UsdPreviewSurface` diffuse texture → Q3 shader name (`r_freeusdShaderMap`).
- Arbitrary binary `.usdc` scene decode is **not** supported unless the root layer is USDA or FreeUSD’s embedded-crate path applies.
- The embedded USDA parser admits layers up to 512 MiB. This covers the 417 MiB
  Sponza main validation layer while keeping parser allocation bounded; larger
  foliage and candle layers require streamed/composed import.

The bridge is deliberately bounded. `r_freeusdImportAllMeshes 1` aggregates
composed prims only up to `r_freeusdMeshBudget`; the renderer reports the
accepted mesh count and whether the source set was budget-truncated. This keeps
legacy `model_t` registration from becoming an implicit GPU residency policy.

### 2027 native scene handoff

The next FreeUSD renderer milestone is a native scene handoff with one record
per composed mesh or GeomSubset:

1. stable composed prim path and authored transform;
2. world bounds and material handle;
3. persistent vertex/index ownership suitable for meshlet/MDI culling;
4. explicit alpha/PBR policy and shadow participation;
5. residency state and budget accounting independent of `model_t`/MD3.

Districts should consume that handoff directly. The MD3 bridge remains for
OpenArena and legacy entity calls until the native path has matching golden
coverage.

## Client console

Requires `USE_FREEUSD` build and `com_freeusd 1` (default).

| Command | Purpose |
|---------|---------|
| `usd_info <path.usda>` | Stage metadata, prim/material counts |
| `usd_assess <path.usda>` | `AssessEngineRuntimeSupport` (layer stack, variants, PreviewSurface, Lux, physics, warnings) |
| `usd_entities <path.usda>` | Prim hierarchy (`com_usdEntities 1`) |
| `usd_shaders <path.usda>` | Materials and geom bindings (`com_usdShaders 1`) |
| `usd_meshes <path.usda>` | List tessellatable `UsdGeom.Mesh` prims and triangle counts |
| `usd_houdini <path.usda>` | Audit Houdini-style primvars, GeomSubset parents, material paths, variants, and time-sampled mesh data |
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

## World districts (proxy meshes)

FreeUSD `BuildEngineSceneSnapshot` also feeds the **district manifest** parser (`district_load`). District assemblies and `purpose=proxy` prims drive proxy/full residency and optional `cm_stream` sector loads. Registered proxy/full `.usda` meshes draw at manifest **`xformOp:translate`** origins when **`r_districtDraw` 1**. See **[DISTRICTS.md](DISTRICTS.md)**.

## Test assets

| File | Purpose |
|------|---------|
| `tests/data/usd/parity_geom_mesh.usda` | Single-triangle `UsdGeom.Mesh` (geometry parity) |
| `tests/data/usd/parity_shade_preview.usda` | `UsdPreviewSurface` + `material:binding` (shader-map parity) |
| `tests/data/usd/world_playfield.usda` | District manifest (North/South + proxy layer) |
| `examples/demo_game/mod/models/*.usda` | Shipped in `idtech3_demo.pk3`; `demo_usd.cfg` prints console hints |
| `examples/demo_game/mod/world/*.usda` | District demo tree; `demo_districts.cfg` |

Wiring: `tests/scripts/test_freeusd_wiring.sh` / `ctest -R test_freeusd`.

## License

FreeUSD is **GPL-2.0-or-later**. With `USE_FREEUSD=ON`, the Vulkan renderer plugin and client link FreeUSD static libraries; distribution must comply with GPL obligations for those binaries.
