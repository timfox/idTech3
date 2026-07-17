# TODO / FIXME Triage

**Date**: 2026-07-11 (last triage pass)  
**Scope**: First-party code under canonical roots — `engine/`, `runtime/`, `modules/`, `renderers/`, `extensions/` (exclude `third_party/` / `**/external/**`). Phase 5e dropped `src/*` shims; see [`ENGINE_REORG_PLAN.md`](ENGINE_REORG_PLAN.md) and [`core/SHIM_REMOVAL_CHECKLIST.md`](core/SHIM_REMOVAL_CHECKLIST.md).

---

## Summary

| Category | Count | Status |
|----------|-------|--------|
| `TODO` / `FIXME` in first-party tree (`engine`, `runtime`, `modules`, `renderers`, `extensions`) | **0** | No literal `TODO`/`FIXME` comments (2026-07-11 grep). Track work in this doc + `docs/ROADMAP.md`. |
| net_sdr.c TODOs | 7 | Resolved: full Steam SDR implementation (Steamworks SDK) |
| be_aas_reach.c FIXME | 1 | Resolved: reworded to Legacy (dead code) |
| False positives (XXX in names, patterns) | 4 | Documented; no action |
| External/third-party | 150+ | Out of scope (`third_party/`, vendored `**/external/**`) |

**Note**: The monolithic `src/renderers/vulkan/vk.c` was removed; Vulkan logic lives in `vk_*.c` (see `docs/ARCHITECTURE.md`). Stub/backlog rows below reference those modules, not `vk.c`. Historical FBO audits may still name `vk.c` (out of scope for mass edit this pass).

---

## Chocolate / id Tech 8 (2026-07) — DONE

| Slice | Status | Notes / limits |
|-------|--------|----------------|
| Temporal upscale | **DONE** | `r_upscale` 1\|2, `upscale_status`; mode 2 = engine Halton+TAA temporal upsample (**not** FidelityFX/DLSS SDK) |
| Hybrid1 deepen | **DONE** | Soft sun, contact harden, GGX, IBL mode, diffuse direct, dlight shadows — see [`HYBRID_RENDERING1.md`](HYBRID_RENDERING1.md) |
| Open-world LOD + district unmerge | **DONE** | `r_bspStreamLod`; district unload → `BspStreamUnmergeSector` |
| Virtual texture + sparse + GPU feedback | **DONE** | `r_vt` / `r_vtSparse` / `r_vtFeedback`; dense atlas fallback; **not** full BSP UV streaming VT |
| Meshlets | **DONE** | Bake-at-load CPU cull + compact draw; **≠** Nanite / mesh shaders |

---

## Project TODOs (Actionable)

### net_sdr.c - Steam Networking Sockets (SDR)

**Resolution**: Full implementation complete. Uses Steamworks SDK (ISteamNetworkingSockets) for P2P SDR. Enable with `USE_STEAM_NETWORKING=ON` and set `STEAMWORKS_SDK` to SDK path. Cvar `net_sdr 1` enables SDR transport. Connect via `connect steam:STEAMID`. See `docs/ROADMAP.md` and `docs/DEVELOPMENT_SETUP.md`.

---

### be_aas_reach.c - Jump pad velocity

| Line | Item | Priority | Notes |
|------|------|----------|-------|
| 3595 | 1.1 overshoot factor | P4 | Reworded FIXME → Legacy; in commented-out block. Dead code. |

**Resolution**: Comment updated to "Legacy: 1.1 overshoot factor for trigger_push (commented block)". No FIXME remaining.

---

## False Positives (Not TODOs)

| File | Pattern | Reason |
|------|---------|--------|
| vm_armv7l.c:36 | `XXX` in pragma | Compiler warning ID (signed/unsigned mismatch) |
| vm_aarch64.c:29 | `XXX` in pragma | Same |
| vm_x86.c:3819 | `OP_XXX` | Opcode name in comment |
| tr_image.c (Vulkan) | `lm_XXXX` | Lightmap texture naming pattern (e.g. lm_0001) |

---

## External Libraries (Out of Scope)

TODOs/FIXMEs in `third_party/` and vendored `**/external/**` (duktape, zstd, cjson, flac, libpng, opus, FreeUSD, etc.) are not triaged; upstream fixes apply.

---

## Incomplete / Stub Items (Documented)

| Item | Location | Status |
|------|----------|--------|
| RB_ColorMask (Vulkan) | `tr_backend.c`, `vk_clear_attachments.c` | **PARTIAL**: `vk_set_color_write_mask()` exists; **`r_vk_colorWriteMaskDynamic`** (latched, default 0) gates `VK_EXT_extended_dynamic_state3`. Disabled by default (OIT/driver issues); falls back to full color writes. |
| r_renderMode 1 | `tr_init.c`, `tr_render_mode_vk.c` | **`r_deferredLighting 1`**: G-buffer fill + Forward+ tile diffuse (point+spot); **`r_deferredUnlitBase 1`** additive dynamic on static base; latches `r_forwardPlusShade` 0. |
| r_renderMode 2 | `tr_render_mode_vk.c` | Latched Forward+ primary: sets `r_forwardPlus` / `r_forwardPlusShade` (GPU cap 64; classic `dlightBits` still 32). |
| r_hdr 3 64-bit output | `vk_post_process_pipeline.c`, HDR format helpers | **PARTIAL**: Infrastructure in place (`vk_hdr64_active`, `_hdr64` modules, pipeline selection). glslangValidator rejects `dvec4`/`f64vec4` fragment outputs → falls back to RGBA32F. |
| Temporal CPU-skin prev-vertex | `tr_init.c`, `vk_view_state.c`, `RENDERER_2026_ARCHITECTURE_PASS.md` | **PARTIAL**: `r_temporalCpuSkinPrev` 1 (default) uses per-entity motion fallback; true CPU-skinned prev-vertex tess + richer reactive debug still open. |
| Vegetation wind draw | `vk_vegetation_wind.c` + `tr_shade.c` | **Same-frame deform:** `vk_vegetation_wind_prepare_draw()` runs before the stage iterator on `SURF_VEGETATION` batches; compute writes deformed positions back into `tess.xyz`. Optional future: bind `vegwind_vertex_buffer` as stream 0 to skip CPU readback. |
| Vulkan RTX entities | `vk_rtx_entities.c` | **PARTIAL**: MD3 LOD0 + **CPU-skinned IQM/MDR** + static/CPU-skinned glTF mesh BLAS; AABB for pack failures. Entity hit albedo: **`r_rtxEntityMaterials`** (default 1) + **`r_rtxEntityUvSample`** (default 1) pack UV-centroid 8×8 diffuse thumbs into albedo SSBO (fallback: texture averages; **`R_EnsureImageThumb`** lazy GPU/avgColor for shell uploads). World hit albedo: **`r_rtxWorldMaterials`** / **`r_rtxWorldUvSample`** (defaults 1) mirror that path in `vk_rtx_world.c` (fallback: BSP vertex colors; **`r_rtxWorldAlbedoMode 1`** = material×vertex). Shared pack helpers: **`vk_rtx_material.c`** (prefers **`lightingStage`**). Entity BLAS: **`r_rtxEntityBlasUpdate`** (default 1) UPDATEs when triangle count stable. **`rtx_status`**: `cpuskin=` / `skinfail=` / `proxy_rate=%`. World BSP BLAS + demo blit when `r_rtxDemo` 1. **`USE_VULKAN_RTX` OFF**: `#else` stubs. Still open: true hit-shader UV/bindless texturing. See `docs/RENDERERS_FUTURE.md`. |
| Surfel GI (GIBS) | `vk_surfel_gi.c` | **PARTIAL**: Chocolate RTX — spawn/update/hash/resolve/composite; world + **entity** albedo/normal SSBOs + stale recycle + fixed-bucket spatial hash (16/cell) + **Hybrid1 fusion** + **density presets** (`r_surfelGiDensity` / adaptive spawn / minSep). Still open: hit-shader UV texturing (pack-time world+entity UV thumbs landed). See `docs/SURFEL_GI.md`. |
| Hybrid Rendering 1 | `vk_hybrid1.c` | **SHIPPED** (experimental): soft sun / contact harden / GGX / IBL / diffuse / dlight shadows. Long-term polish remains product taste, not a stub. See [`HYBRID_RENDERING1.md`](HYBRID_RENDERING1.md). |
| VDB Woodcock volumetrics | `volumetric_fog.frag`, `vk_vdb.c` | **DONE**: mode `3` + majorant bricks; demo `exec demo_vdb_woodcock.cfg` + shipped `vdb/fog_2cubed.nvdb`; SIM profile documents mode 3. See [`VDB_WOODCOCK_VOLUMETRICS.md`](VDB_WOODCOCK_VOLUMETRICS.md). |
| Experimental renderers | `USE_EXPERIMENTAL_RENDERERS` (default ON) | NIV/NIST/NVC/VFGI/NDGI/NSLM/RenderFormer/VkSplat/WPT/MGS/WSP. OFF → `vk_experimental_renderer_stubs.c`. |
| FreeType off | `BUILD_FREETYPE` OFF | `tr_font_stub.c` + `tr_vector_font_stub.c` (cached `.dat` fonts; no TTF/vector load). |
| Layout / include rewrite | `modules/*`, `runtime/*`, `engine/`, `renderers/`, `extensions/` | **DONE** for first-party `#include "../qcommon/..."` → flat headers. Client shell → `shell/`. Layout bridges kept pending MSVC / `"qcommon/..."` audit before drop. |

---

## Subsystem audit log (rolling)

| Date | Scope | Notes |
|------|--------|------|
| 2026-07-11 | Triage follow-up | Flattened remaining `../qcommon` in `engine/` / `renderers/` / `extensions/` (65 files); Vulkan gains `IDTECH3_DIR_ENGINE_CORE`. Bridges retained for soak. Next: bridge-drop audit or RTX long-term. |
| 2026-07-11 | Triage follow-up | Include rewrite: modules + `runtime/{client,game,server}`; client shell → `shell/`. `MOD_SDK` FSR2 wording fixed. |
| 2026-07-11 | Triage | Retargeted this doc to canonical roots (Phase 5e); re-scan: **0** `TODO`/`FIXME` in first-party tree. |
| 2026-07-11 | Chocolate / id Tech 8 | Temporal upscale, Hybrid1 deepen, open-world LOD + district unmerge, VT+sparse+GPU feedback, meshlets — marked **DONE** (intentional SDK/Nanite limits documented above). |
| 2026-07-11 | VDB Woodcock | Demo cfg + bootstrap `.nvdb` + `SIM_RENDER_PROFILE` mode 3 + `test_vdb_woodcock.sh` — polish **DONE**. |
| 2026-07-11 | Layout | Include rewrite started: navigation + physics flat includes; next audio → world → botlib → client. |
| 2026-07-11 | Sparse VT | `vk_sparse.c` + `r_vtSparse` / GPU `vt_feedback` compute; dense atlas fallback. |
| 2026-04-27 | Vulkan / post | `vk_post_process_pipeline.c`: named `Vk_PostProcess_FragSpecData`, `#define` map count (29), `_Static_assert` array ≥ count; avoids silent spec/map drift. |
| 2026-04-27 | OpenGL / IQM | `R_LoadIQM` / mesh validation: cast `header->num_*` / `mesh->num_vertexes` to `int` in loop bounds; fix `ARRAY_LEN` / joint name compares for `-Wsign-compare` (Clang). |
| 2026-04-27 | OpenGL / IQM | `tr_model_iqm.c`: const-correct `RB_IQMSurfaceAnim` surface pointer; blend loop bound `j < 4` (fixes `-Wcast-qual` / `-Wsign-compare` on `ARRAY_LEN(float[4])`). |
| 2026-04-27 | Vulkan / PBR | `vk_create_pipeline.c`: PBR `ADD_FRAG_SPEC` wrote 41 map entries into `spec_entries[38]` (stack smash / SIGABRT after `VarInfo`); enlarged buffer + runtime guard + `_Static_assert` so array size cannot drift below entry count. |
| 2026-04-27 | Vulkan / IQM | `tr_model_iqm.c`: avoid `-Wcast-qual` on `backEnd.currentEntity` (uintptr_t bridge). Vegetation wind: same-frame prepare path documented (see triage row). |
| 2026-05-19 | Tooling | Optional **Tiled Map Editor** submodule `tools/tiled` (GPL-2.0, pinned **v1.9.91**); see `docs/TILED.md`. |
| 2026-04-11 | Network / downloads | `cl_curl.c`: `dl->Name` from `Content-Disposition` uses **`Q_strncpyz`** (Phase 2 P0 item remains fixed). |
| 2026-04-11 | Botlib / preprocessor | Botlib `l_precomp.c` bounded `Com_sprintf` sites (historical `src/platform` duplicates noted in older audits). |
| 2026-04-11 | Botlib / chat (Windows tree) | Windows botlib chat load paths aligned to `Com_sprintf` / `Q_strncpyz`. |
| 2026-04-11 | Client / server / qcommon | Historical `src/{client,server,qcommon}` triage: no `TODO`/`FIXME` matches. |
| 2026-04-11 | Renderer validation | `renderer_regression_check.sh` + manifest caps (`GLTF_MAX_*`, `IQM_*`) — CI-style parity checks on `main`. |
| 2026-04-11 | Build / link | Optional **`ENABLE_LTO`** (`./scripts/compile_engine.sh … lto`): CMake `CheckIPOSupported` + `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for GCC/Clang Release/RelWithDebInfo; off by default. |
| 2026-04-11 | Vulkan / future GPU | **`r_vk_meshShaderNV`** (default 0): optional **`VK_NV_mesh_shader`** + `VkPhysicalDeviceMeshShaderFeaturesNV.meshShader` for NVIDIA; no mesh pipelines yet. **DLSS:** not in-repo; startup log documents use `r_renderScale` / driver scaling. |

## Recommendations (ordered)

1. ~~Include rewrite (`modules/*` + `runtime/*` + `engine/` / `renderers/` / `extensions/`)~~ — **done 2026-07-11**.
2. ~~Woodcock demo cfg + sample `.nvdb` + SIM mode 3~~ — **done 2026-07-11**.
3. ~~Fix `docs/MOD_SDK.md` FSR2 wording~~ — **done 2026-07-11**.
4. ~~Client root shelving~~ — **done 2026-07-11** (`runtime/client/shell/`).
5. **Bridge-drop audit**: confirm no `"qcommon/..."` / MSVC bridge dependence, then remove `modules/qcommon` / platform qcommon bridges.
6. **RTX long-term**: True hit-shader UV/bindless texturing (shared `vk_rtx_material` pack helpers + pack-time world+entity UV thumbs + MD3/IQM/glTF/MDR mesh AS + entity attrs + Hybrid1↔Surfel fusion + materials + entity BLAS UPDATE + Surfel density budgeting landed).

---

## How to re-scan (engine only)

From repo root:

```bash
rg '\b(TODO|FIXME)\b' engine runtime modules renderers extensions --glob '!**/third_party/**' --glob '!**/external/**'
rg '#include\s+".*\.\./qcommon/' engine runtime modules renderers extensions --glob '*.{c,h,cpp,hpp}' --glob '!**/third_party/**' --glob '!**/external/**'
```

Expect **no** `TODO`/`FIXME` matches and **no** `../qcommon` relative includes in first-party code until new comments/paths are added. Third-party hits under `third_party/` / vendored externals remain out of scope.

---

## Documentation (post-`vk.c` split)

Vulkan audits and roadmaps should point at **`vk_*.c`** modules and **`docs/ARCHITECTURE.md`**, not removed `vk.c` line numbers. A pass in April 2026 updated the main stale references (`VULKAN_FBO_AUDIT.md`, `FBO_BREAKAGE_ANALYSIS.md`, `SIGGRAPH_FEATURES_ROADMAP.md`, volumetric docs, `CODEBASE_AUDIT_PHASE2.md`). Historical audits may still mention `vk.c` by name — leave them unless editing those docs for other reasons.

To find any stragglers:

```bash
rg 'vk\.c' docs --glob '*.md'
```
