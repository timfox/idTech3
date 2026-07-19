# Renderer confidence checklist

This document lists **automated** checks you can run today and **manual** passes that still require a GPU, game data, or artist judgment. Use it to tighten the Vulkan renderer without turning every idea into a CI job.

## Release gate (renderer changes)

Treat this file as a **default gate** for any change that touches rendering, shaders, mesh tangents, or image loaders used by the renderer:

1. Run **automated** rows in the table below (at minimum `renderer_regression_check.sh` + **`spine_stability_check.sh`** for renderer/G-buffer/temporal changes + your usual build).
2. For **graphics PRs**, run the **manual** short list or note why it is N/A (e.g. docs-only).
3. Prefer **first-party** warning/correctness fixes over new subsystems until the gate is green.

Visual regression **specs** and suggested map names: [samples/renderer_regression/README.md](samples/renderer_regression/README.md).

## Automated (headless / CI-friendly)

Run these after any renderer, shader, or mikktspace change.

On **`main`**, GitHub Actions **`.github/workflows/build.yml`** runs **`renderer_regression_check.sh`** (Tier A: manifest + cap parity + full GLSL `glslang` when the job installs `glslang`) on **Windows MSYS2**, **Windows MSVC** (Vulkan SDK install for `glslangValidator`, Release matrix only), **macOS**, **Ubuntu ARM**, and **Android** (CMake matrix + Gradle APK job), in addition to Ubuntu x86_64 / ASAN paths that already ran smoke + ctest. MSVC **smoke_test** runs on **x64 Release** only (ARM64 PEs are not executed on the x64 GitHub runner). Forks without `glslang` in a job still skip only the GLSL subtree inside the script.

| Check | Command | What it proves |
|--------|---------|----------------|
| Renderer regression (repo) | `./scripts/renderer_regression_check.sh` | Regression docs present (manifest includes `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`, `BUILD.md`, `docs/FORWARD_PLUS_PIPELINE_AUDIT.md`, `src/renderers/vulkan/vk_temporal.h`, …); `shader_data.c` / `shader_binding.c` exist; recursive GLSL `glslang` pass; IQM/glTF morph **#define** parity; Forward+ **tile cull vs `tr_types.h` / `vk_forward_plus.c`** caps; **`gen_frag.tmpl`** tile stride `tileId * Nu` matches **`MAX_PER_TILE`**; **`r_forwardPlusMaxPerTile`** range wired to `vk_forward_plus_get_*_per_tile_cap`; PBR **`forward_plus_shade_strength`** `constant_id` matches `vk_create_pipeline.c`; Forward+ **`depthCull`** push + depth binding **3**; volumetric VDB binding **17** + **`vdbParams`** UBO fields; **`vk_temporal`** reset bitmask count matches switch + `knownReasons[]` table. Manifest lines strip **CRLF** so Windows checkouts do not false-fail path existence. Optional: set `GAME_BASE` and uncomment BSP paths in `OPTIONAL_GAME_ASSETS.txt` to require packaged maps. |
| **Renderer Spine stability** | `./scripts/spine_stability_check.sh` | Umbrella static contract for [Spine 1.0](RENDERER_SPINE_1.0.md): `modern_vulkan_stable.cfg` / quality / gfx_safe profile pins, mouse `input_status` symbols, WBOIT clears/barriers, packaging. Delegates to `gbuffer_av_lifecycle_check.sh` and `temporal_ownership_check.sh`. |
| **G-buffer / AV lifecycle** | `./scripts/gbuffer_av_lifecycle_check.sh` | Soft-fail create paths for deferred G-buffer, Ambient Visibility, and visibility-buffer scaffolds; presentation teardown/restore order; `r_dgbFailInject` / `r_avFailInject`; descriptor generation; TLAS→AV history invalidation; stable profile keeps `r_ambientVisibilityMode 2`. |
| **Temporal ownership** | `./scripts/temporal_ownership_check.sh` | Shared `apply_resets` (TAA/AV/volumetric/motion/occlusion); RDF_NOWORLDMODEL thrash guard; weapon-after-TAA deferral; portal/world-matrix isolation; `temporal_status` registration; stable profile must not enable `r_taa 1`. |
| Map load sanity (content) | `GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_maps.sh` | Dedicated server runs `+map` for each `rtest_*` map; log scanned for `ERROR:`, `couldn't load`, `CM_LoadMap`, crashes. Requires full game base (VM + assets), not the regression pk3 alone. `RELEASE_DIR` optional (default `<repo>/release`). Optional `MAPS_EXTRA="name1 name2"` (or repo var `IDTECH3_MAPS_EXTRA` on Tier B self-hosted) appends more BSPs. Mixed dlights checklist: `docs/samples/renderer_regression/scenes/08_tier_b_mixed_dlights.md`. |
| Full local CI parity | `./scripts/validate_ci_build.sh` | SPIR-V generation, Vulkan Release build, smoke test, renderer regression check. |
| CMake smoke + artifacts | `ctest --output-on-failure` (from `build-vk-Release`) | Smoke, **renderer_regression_check**, artifacts, unit hooks, **demo_game pk3 layout** (`test_demo_game_pk3`). |
| Standalone GLSL | `./scripts/smoke_test.sh` (or the smoke step inside `validate_ci_build.sh`) | Every `.vert`, `.frag`, `.geom`, and `.comp` under `src/renderers/vulkan/shaders/glsl/` validates with `glslangValidator` (recursive, including `volumetric/`, `terrain/`, `postfx/`). |
| Tier resolver | `./scripts/resolve_renderer_tiers.sh --tier A` | Runs the headless renderer tier contract in one place: renderer regression, GPU golden manifest, modern renderer profile source contract, and selected CTest checks when `build-vk-Release` exists. Use `--strict` with `GAME_BASE` for release candidates after Tier B/C evidence exists. |

Optional: `SKIP_IDPAK_CHECK=ON` is normal for engine-only trees; the dedicated server may exit with “no game data” after init - that is still a useful crash-free signal.

**Vulkan Forward+ (optional, `r_forwardPlus 1`):** `r_forwardPlusMaxPerTile` (4–8, latched, default 8) trims per-tile light index work while keeping the same SSBO stride; requires `vid_restart` after changes. Does not affect mod game code or QVMs.

## Manual (GPU + content)

These do not run in headless CI; use the **[visual regression pack](samples/renderer_regression/README.md)** (scene docs + optional BSP list) so the same six concerns are hit every time.

1. **Tangent / normal maps** - Load meshes that rely on MikkTSpace tangents; inspect lighting seams at UV splits and mirrored UVs. Regress after changes to `mikktspace` or normal-map sampling.
2. **PBR materials** - Scenes with metalness/roughness, clearcoat, and normal maps; toggle `r_pbr_debug` modes where applicable. See [PBR_TEXTURES.md](PBR_TEXTURES.md).
3. **Vulkan stability** - Same map, same cvars across sessions: confirm no silent init or postFX regressions (water, fog, volumetrics).
4. **Validation** - Debug build or `r_vulkan_validation` (see project cvars/docs): clean validation for a representative play session.
5. **MSAA / SMAA / SSAO** - Spot-check toggles; watch for black screens, NaN tint, or resolution mismatches.
6. **Emissive** and **volumetric fog** - See scene docs under `samples/renderer_regression/scenes/`.

## Renderer proof loop

Use this loop to move renderer work from “builds and scripts pass” to “actual rendered behavior is proven.”

Record outcomes under [docs/renderer_validation/](renderer_validation/) (Tier C template and optional [FINDINGS.md](renderer_validation/FINDINGS.md)). For automated Tier B on `main`, see [docs/renderer_validation/SELF_HOSTED_TIER_B.md](renderer_validation/SELF_HOSTED_TIER_B.md) (repository **variable** or **secret** `IDTECH3_GAME_BASE_PATH`, self-hosted runner label `idtech3-tierb`). For **Vulkan glTF GPU tangent mode 2** (`r_gltfGpuTangentFix 2`), use the Tier B checklist in [docs/samples/renderer_regression/scenes/07_gltf_gpu_tangent_topo.md](samples/renderer_regression/scenes/07_gltf_gpu_tangent_topo.md).

### 1. Install the regression pack

Install `z_renderer_regression.pk3` into a real content tree so the engine can see the six regression maps exactly as documented under `docs/samples/renderer_regression/`. Use the devdata tree under `docs/renderer_validation/devdata/rtest_base/` or rebuild it with `./scripts/build_renderer_devdata.sh` if needed.

### 2. Run the file contract

Set `GAME_BASE` to the directory named `base` that contains the regression pack and required game data, then run:

```bash
GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_check.sh
```

This proves the repo-side manifest, required files, and expected regression content layout are present. It does not prove client/GPU output.

### 3. Run the runtime contract

With the same `GAME_BASE`, run:

```bash
GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_maps.sh
```

This proves each regression map can be loaded by `idtech3_server` without matching the scripted fatal/error signatures. It still does not prove the client frame.

### 4. Run the manual GPU pass

Run the client manually with Vulkan and inspect the regression scenes using the guidance in this document and the scene specs under `docs/samples/renderer_regression/`.

Priority order:

1. `rtest_parity`
2. `rtest_volumetric`
3. `rtest_tangent`
4. `rtest_pbr`
5. `rtest_emissive`
6. `rtest_postfx`

Focus on:

* Stable frame output across restarts and resolution changes
* tangent and normal-map seam behavior
* roughness/metallic response
* emissive and exposure/bloom interaction
* volumetric fog behavior
* postFX toggle correctness

### 5. Record findings

Record outcomes in `docs/renderer_validation/FINDINGS.md` or the active team equivalent. Every item should be classified as one of:

* **confirmed OK**
* **bug to fix**
* **known limitation**
* **needs more evidence**

Do not record vague results like “looks fine.” Each note should identify the map, symptom, backend, and the likely subsystem when known.

### Definition of success

A renderer change is not fully proven until:

* the file contract is green
* the runtime contract is green
* the manual Vulkan GPU pass is complete
* findings are written down with concrete outcomes

### First manual pass (strict)

For the **first** full renderer proof run against regression content, treat the pass as complete only when:

* `renderer_regression_check.sh` is green with the real `GAME_BASE`
* `renderer_regression_maps.sh` is green for all six maps
* `rtest_parity` has either acceptable Vulkan baseline stability or a **clearly written drift note** in findings
* `rtest_volumetric` has either acceptable fog behavior or a **clearly written limitation/bug** in findings
* every finding is recorded as **confirmed OK**, **bug to fix**, **known limitation**, or **needs more evidence** - not vague impressions

### Next automation step

After the first manual pass is stable, add screenshot or framebuffer capture for 1–2 anchor scenes, starting with `rtest_parity` and `rtest_pbr`. Do not add this before the manual pass is producing stable, repeatable results.

That sequence closes the gap between engine-side confidence and renderer evidence; run it before adding more unit tests or process churn.

## Repo discipline

- Prefer **first-party** correctness and warning fixes; touch **vendored** code only when the fix is small and behavior-preserving (e.g. const correctness).
- Keep **README** and this doc aligned: what is shipping vs in progress lives in [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) and [RENDERERS.md](RENDERERS.md).

## See also

- [RENDERERS.md](RENDERERS.md) - feature inventory and cvars
- [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md) - production-certified frame matrix and exit criteria
- [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) - pre-release steps
- `./scripts/validate_ci_build.sh` - local mirror of Ubuntu CI expectations
