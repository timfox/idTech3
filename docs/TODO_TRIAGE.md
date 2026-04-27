# TODO / FIXME Triage

**Date**: 2026-04-10 (last triage pass)  
**Scope**: Project code in `src/` (excluding `external/` third-party libraries)

---

## Summary

| Category | Count | Status |
|----------|-------|--------|
| `TODO` / `FIXME` in engine tree (`src/client`, `src/game`, `src/qcommon`, `src/renderers`, `src/server`, `src/botlib`, `src/navigation`, `src/physics`, `src/platform`, `src/audio`) | **0** | No literal `TODO`/`FIXME` comments (2026-04-10 grep). Track work in this doc + `docs/ROADMAP.md`. |
| net_sdr.c TODOs | 7 | Resolved: full Steam SDR implementation (Steamworks SDK) |
| be_aas_reach.c FIXME | 1 | Resolved: reworded to Legacy (dead code) |
| False positives (XXX in names, patterns) | 4 | Documented; no action |
| External/third-party | 150+ | Out of scope |

**Note**: The monolithic `src/renderers/vulkan/vk.c` was removed; Vulkan logic lives in `vk_*.c` (see `docs/ARCHITECTURE.md`). Stub/backlog rows below reference those modules, not `vk.c`.

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
| tr_image.c (Vulkan, OpenGL) | `lm_XXXX` | Lightmap texture naming pattern (e.g. lm_0001) |

---

## External Libraries (Out of Scope)

TODOs/FIXMEs in `src/external/` are from third-party code (duktape, zstd, cjson, flac, libpng, opus, etc.). These are not triaged; upstream fixes apply.

---

## Incomplete / Stub Items (Documented)

| Item | Location | Status |
|------|----------|--------|
| RB_ColorMask (Vulkan) | tr_backend.c | Partial: `vk_set_color_write_mask()` exists, but the VK_EXT_extended_dynamic_state3 path is currently disabled due to validation/driver issues; Vulkan falls back to full color writes. |
| r_renderMode 1/2 | tr_init.c | Deferred / alternate-pipeline placeholders (`r_renderMode`); not wired to Vulkan **optional** Forward+ (`r_forwardPlus`). Real deferred still needs G-buffers, etc. Documented in cvar description. |
| r_hdr 3 64-bit output | `vk_post_process_pipeline.c`, HDR format helpers | Infrastructure in place (vk_hdr64_active, _hdr64 modules, pipeline selection). glslangValidator rejects dvec4/f64vec4 fragment shader outputs. Falls back to RGBA32F. When glslang adds support, compile HDR64 variants and return RGBA64F from get_hdr_format. |
| Vegetation wind draw | `vk_vegetation_wind.c` + draw path | Compute dispatch runs **after** each vegetation batch draw (`RB_EndSurface`); buffer holds deformed verts for the **next** frame unless the raster path binds `vegwind_vertex_buffer` as stream 0 (still TODO). |
| Vulkan RTX | CMake `USE_VULKAN_RTX`, `vk_rtx.c` / `vk_rtx_world.c`, `r_rtx` / `r_rtxDemo` / `r_rtxWorldPrimCap` | Demo: world BSP BLAS over all map brush submodels (capped) + trace + blit when `r_rtxDemo` 1; entity TLAS and real lighting still TODO. See `docs/RENDERERS_FUTURE.md`. |

## Subsystem audit log (rolling)

| Date | Scope | Notes |
|------|--------|------|
| 2026-04-27 | Vulkan / post | `vk_post_process_pipeline.c`: named `Vk_PostProcess_FragSpecData`, `#define` map count (29), `_Static_assert` array ≥ count; avoids silent spec/map drift. |
| 2026-04-27 | OpenGL / IQM | `tr_model_iqm.c`: const-correct `RB_IQMSurfaceAnim` surface pointer; blend loop bound `j < 4` (fixes `-Wcast-qual` / `-Wsign-compare` on `ARRAY_LEN(float[4])`). |
| 2026-04-27 | Vulkan / PBR | `vk_create_pipeline.c`: PBR `ADD_FRAG_SPEC` wrote 41 map entries into `spec_entries[38]` (stack smash / SIGABRT after `VarInfo`); enlarged buffer + runtime guard + `_Static_assert` so array size cannot drift below entry count. |
| 2026-04-27 | Vulkan / IQM | `tr_model_iqm.c`: avoid `-Wcast-qual` on `backEnd.currentEntity` (uintptr_t bridge). Vegetation wind: `vk_vegetation_wind.c` header + triage row clarify dispatch-after-draw vs binding `vegwind_vertex_buffer` (still TODO). |
| 2026-04-11 | Network / downloads | `cl_curl.c`: `dl->Name` from `Content-Disposition` uses **`Q_strncpyz`** (Phase 2 P0 item remains fixed). |
| 2026-04-11 | Botlib / preprocessor | **`src/botlib/l_precomp.c`** already bounded; duplicate **`src/platform/botlib/l_precomp.c`** and **`src/platform/win32/botlib/l_precomp.c`** aligned: `sprintf` → **`Com_sprintf(..., MAX_TOKEN, ...)`** (5 sites each). |
| 2026-04-11 | Botlib / chat (Windows tree) | **`src/platform/win32/botlib/be_ai_chat.c`** `BotLoadChatMessage`: `sprintf`/`strcpy` → **`Com_sprintf` / `Q_strncpyz`** to match **`src/botlib/be_ai_chat.c`**; **`src/platform/botlib/be_ai_chat.c`** fixed-string append → **`Q_strncpyz`**. |
| 2026-04-11 | Client / server / qcommon | `rg '\\b(TODO|FIXME)\\b' src/{client,server,qcommon}` - **no matches** (same triage expectation as 2026-04-10). |
| 2026-04-11 | Renderer validation | `renderer_regression_check.sh` + manifest caps (`GLTF_MAX_*`, `IQM_*`) - CI-style parity checks on `main`. |
| 2026-04-11 | Build / link | Optional **`ENABLE_LTO`** (`./scripts/compile_engine.sh … lto`): CMake `CheckIPOSupported` + `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for GCC/Clang Release/RelWithDebInfo; off by default. |
| 2026-04-11 | Vulkan / future GPU | **`r_vk_meshShaderNV`** (default 0): optional **`VK_NV_mesh_shader`** + `VkPhysicalDeviceMeshShaderFeaturesNV.meshShader` for NVIDIA; no mesh pipelines yet. **DLSS:** not in-repo; startup log documents use `r_renderScale` / driver scaling. |

## Recommendations

1. **net_sdr.c**: Full SDR implementation; requires `USE_STEAM_NETWORKING=ON` and Steamworks SDK.
2. **be_aas_reach.c**: No action; dead code.
3. **USE_VULKAN_RTX**: Build with `-DUSE_VULKAN_RTX=ON` to request ray tracing extensions on RT-capable GPUs.
4. Re-run triage after major refactors or when adding new features.

---

## How to re-scan (engine only)

From repo root:

```bash
rg 'TODO|FIXME' src --glob '!**/external/**'
```

Expect **no matches** in first-party code until new comments are added. Third-party hits under `src/external/` remain out of scope for this triage doc.

---

## Documentation (post-`vk.c` split)

Vulkan audits and roadmaps should point at **`vk_*.c`** modules and **`docs/ARCHITECTURE.md`**, not removed `vk.c` line numbers. A pass in April 2026 updated the main stale references (`VULKAN_FBO_AUDIT.md`, `FBO_BREAKAGE_ANALYSIS.md`, `SIGGRAPH_FEATURES_ROADMAP.md`, volumetric docs, `CODEBASE_AUDIT_PHASE2.md`).

To find any stragglers:

```bash
rg 'vk\.c' docs --glob '*.md'
```
