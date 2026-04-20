# Renderer Architecture Pass for 2026

**Date**: March 8, 2026  
**Scope**: Vulkan renderer architecture, temporal systems, and backend strategy

---

## Purpose

This document turns the current renderer state into a practical 2026 plan. It is intentionally narrower than the feature wishlists in `RENDERERS_FUTURE.md` and `SIGGRAPH_FEATURES_ROADMAP.md`.

The objective is not "add every modern rendering acronym." The objective is to make the existing Vulkan renderer scale to more lights, behave predictably across temporal effects, and converge on a platform strategy that is realistic for shipping.

---

## Current Snapshot

### What is strong already

- Vulkan is the primary renderer and the only backend with the modern feature stack.
- The engine already ships a substantial HDR/post pipeline: PBR, HDR, bloom, SSR, SSAO/HBAO, SMAA, OIT, atmosphere, volumetric fog, and GPU occlusion culling.
- The current renderer is visually ahead of a typical idTech3 fork.

### What still limits the architecture

- The Vulkan renderer is still **forward-only**. `r_renderMode 1/2` are placeholders, not real deferred or Forward+ paths.
- Dynamic lighting still inherits legacy constraints such as `MAX_DLIGHTS == 32` and surface-bit assumptions in the classic renderer path.
- Temporal behavior is fragmented. Volumetric fog, exposure, motion vectors, occlusion visibility, and post effects each track history differently.
- Platform strategy is incomplete. Vulkan is primary, OpenGL is fallback, Vulkan RTX is only extension scaffolding, and Metal/DXR are not started.

---

## Architectural Decisions

## 1. Lighting Scalability

### Decision

Keep **forward rendering** as the main architecture, but move Vulkan toward **clustered Forward+** rather than investing in a classic deferred renderer.

### Why

- The current renderer already depends heavily on forward shading behavior, material evaluation, alpha-tested content, and post-stack composition.
- A full deferred migration would add a G-buffer, extra bandwidth pressure, more material split paths, and a second lighting architecture to maintain.
- The biggest real bottleneck is not "lack of deferred." It is the legacy light model and CPU-era light selection.

### 2026 target

- Vulkan path supports significantly more than 32 local lights through clustered or tiled light lists.
- Light influence for Vulkan is decoupled from surface bit flags.
- The engine keeps a compatibility forward path for OpenGL and for low-feature Vulkan fallback.
- Shadowing remains budgeted: a small set of shadowed key lights, many unshadowed fill lights.

### Required work

1. Introduce a Vulkan-only GPU light record buffer for visible local lights.
2. Add a compute pass that builds per-tile or per-cluster light lists.
3. Add a Forward+ shader path that consumes those lists.
4. Split "lighting scalability" from "shadow scalability" so the engine can support many lights without trying to shadow all of them.
5. Retire `MAX_DLIGHTS` as the effective Vulkan lighting ceiling while preserving legacy compatibility in shared structures.

### Explicit non-goals

- Do not increase `MAX_DLIGHTS` in place and call that solved.
- Do not make deferred the default path unless clustered Forward+ proves insufficient.
- Do not block lighting scalability on RTX or ReSTIR.

---

## 2. Temporal Robustness

### Decision

Treat temporal stability as a first-class renderer subsystem instead of a set of isolated per-feature fixes.

### Why

- The renderer already uses temporal behavior in multiple places: volumetric fog history, exposure adaptation, previous-frame occlusion visibility, motion-vector-driven post effects, and camera-cut handling.
- The codebase has fixed several correctness issues in the post/FBO chain, which is a signal that temporal state management needs to be more centralized.
- Adding TAA, temporal upscaling, ReSTIR, or hybrid RT before the history model is cleaned up would multiply instability.

### 2026 target

- A shared history/reset policy exists for resize, map load, teleport, camera cut, FOV jump, render-scale change, and missing previous-frame data.
- Motion vectors are trustworthy enough for all temporal consumers that rely on them.
- Temporal effects expose debug information and failure modes consistently.
- SMAA remains a valid default, but the renderer is structurally ready for TAA or an upscaler later.

### Required work

1. Add a renderer-wide "history invalidation" layer used by volumetrics, exposure, SSR/TAA candidates, and future RT reuse systems.
2. Standardize camera-cut detection instead of letting each subsystem guess independently.
3. Improve motion-vector validity for skinned, deformed, and first-person geometry.
4. Add per-effect history confidence controls:
   - neighborhood clamp / rejection
   - reactive mask or equivalent for rapidly changing pixels
   - debug overlays for current vs history contribution
5. Keep temporal AA optional until motion-vector coverage and history invalidation are reliable.

### Incremental (engine)

- **`VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE`** in `vk_temporal.c` compares the **effective render target** size from **`vk_get_render_target_width()` / `vk_get_render_target_height()`** (FBO / `r_renderScale`), not `glConfig` alone, so internal resolution changes still clear motion, TAA, volumetric, exposure, and occlusion history consistently with the color pass.

### Explicit non-goals

- Do not add TAA just to match a checklist if the motion-vector path is still partial.
- Do not let individual effects invent separate camera-cut heuristics when a shared policy can exist.

---

## 3. Platform Strategy

### Decision

Use **Vulkan as the primary renderer architecture**, freeze OpenGL as compatibility-only, prioritize **Metal before DXR**, and treat RTX as a Vulkan feature tier rather than a separate product direction.

### Why

- Vulkan already contains the real renderer investment.
- Apple support is strategically blocked by the lack of a native backend more than Windows is blocked by the lack of DXR.
- A separate DXR renderer before Metal would duplicate architecture before the existing renderer model is fully stabilized.
- Vulkan RTX can extend the existing renderer incrementally once the lighting and temporal base is stronger.

### 2026 target

- Windows/Linux: Vulkan primary.
- macOS/iOS: native Metal backend is the next serious platform expansion.
- OpenGL: compatibility renderer with no expectation of feature parity.
- Ray tracing: hybrid Vulkan RT for selective effects after Forward+ and temporal cleanup.

### Required work

1. Stabilize shader ownership and translation strategy before adding Metal:
   - short term: GLSL remains source of truth for Vulkan
   - medium term: introduce a translation path suitable for Metal instead of hand-diverging shader logic everywhere
2. Preserve shared frontend/model/material code while isolating backend-specific resource management.
3. Keep DXR behind Metal in priority unless a Windows-only product requirement changes that ordering.
4. Keep WebGPU/WebAssembly exploratory until the renderer has a cleaner resource graph and shader portability story.

### Explicit non-goals

- Do not build a standalone DXR renderer first.
- Do not promise full feature parity across Vulkan, OpenGL, Metal, and future DXR simultaneously.

---

## Recommended Execution Order

## Phase 1: Foundation

- **Fix doc/code drift and make the forward-only architecture explicit** - ✅ RENDERERS.md states **forward** main pass, **`r_renderMode` 1/2** placeholders, and **optional Vulkan Forward+** (`r_forwardPlus`) as scaffolding (not a `r_renderMode` switch).
- **Create a shared temporal reset policy** - ✅ Implemented in `vk_temporal.c`. Central reset reasons (renderer_init, swapchain_change, world_change, camera_cut, etc.); `vk_temporal_apply_resets()` clears motion history, volumetric froxel history, occlusion visibility, and exposure. `vk_temporal_request_sticky_reset()` for subsystems to request invalidation.
- **Audit motion-vector coverage and history consumers** - ✅ Documented. Motion vectors: main scene pass (gen_frag, light_frag, color.frag, fog.frag) via `vk_get_prev_mvp_transform`. **Incremental:** entities with **`RF_FIRST_PERSON`** skip per-entity previous model for prev-MVP (view-relative weapon; avoids bogus motion vs stale history). **GPU skin SSBO:** packs **current + previous** influence/joint blocks (same layout twice); vertex shaders use the second block for **`var_PrevClip`** position while **`prevMvp`** still carries rigid entity motion. **`vk_draw_geometry`** re-pushes MVP after skin commit when `iqm_skin_offset` is set. **glTF GPU:** previous joint matrices from the **old animation** sample when blending clips; morph SSBO packs **current + previous** top-K weights (previous from last view’s baked state + clip morph sample at `animOld`). **IQM GPU:** same dual weight row in morph SSBO, with `morphGpuWeightPrev` / `morphChannelWeightPrev` snapshotted in **`RE_EndScene`** per view. Gaps: first-frame morph motion (prev weights zero until second frame), `customShader` deformation, 2D/menus. Consumers: volumetric fog, exposure, motion blur. Occlusion culling now resets visibility on temporal reset (`vk_reset_occlusion_visibility`).

## Phase 2: Lighting Scale

- Add Vulkan light records plus cluster/tile culling. **Incremental (engine):** `r_forwardPlus 1` (default 0) allocates light + tile SSBOs, packs **at most `MAX_DLIGHTS` (32)** lights from `backEnd.refdef` (indices align with `tess.dlightBits`; excess lights are omitted and a **developer** log notes when the source count exceeds the cap), and runs a **compute tile cull** (`forward_plus_tile_cull.comp`, **`VK_FP_TILE_DIM` (16)** px tiles via `ceil(viewport / dim)`, up to **8** index slots per tile; active count **4–8** via latched **`r_forwardPlusMaxPerTile`**, default **8**) after `RB_BeginDrawingView` inside the main render pass. **Compute + PBR fragment** derive **tile pixel size** from the packed **viewport ÷ tile grid** so cull, debug overlay, and experimental shade stay aligned if tile policy changes. Tile grid and NDC→pixel use **`vk_get_render_target_width/height`** (FBO / `r_renderScale`); the tile SSBO is **reallocated when that resolution changes** (no `vid_restart` for resize alone). **PBR fragment:** descriptor set 18 binds light + tile + **param** SSBOs (clip matrix for shading). `r_forwardPlusDebug` (0–1) = debug overlay; **`r_forwardPlusShade`** (0–4, default 0) adds **experimental diffuse + microfacet spec** (per-light `CalcSpecular`, scaled) from tile-culled **point and linear/spot** lights. When `r_forwardPlus` is on, **`pbrForwardPlus.y`** carries **`floatBitsToUint(tess.dlightBits)`** so the shader **skips** any packed light index whose bit is set in `tess.dlightBits` (first **32** indices only; matches typical `MAX_DLIGHTS` range). Primary direct is still **softly renormalized** vs Forward+ energy. Works with deluxe/lightmap; toggling shade **invalidates cached pipelines** next frame.
- Introduce Forward+ shading for local lights.
- Keep shadow budgets conservative and explicit.

## Phase 3: Platform Readiness

- Clean up shader portability assumptions.
- Define the backend seam needed for Metal resource binding and pipeline creation.
- Freeze OpenGL expectations to compatibility support.

## Phase 4: Feature Expansion

- Hybrid Vulkan RT for shadows/reflections.
- Temporal AA or temporal upscaling once motion/history quality is good enough.
- Experimental features such as ReSTIR or MegaLights only after the previous phases land.

---

## Success Criteria

By 2026 expectations, the renderer should be able to claim all of the following without hedging:

- Vulkan is a stable shipping path, not just a feature lab.
- Lighting scale is no longer capped by legacy dynamic-light assumptions.
- Temporal behavior is predictable across fog, exposure, motion blur, and future temporal systems.
- OpenGL is clearly positioned as fallback.
- Metal is the next backend investment; DXR and hybrid RT remain optional extensions, not architectural distractions.

---

## Summary

The correct next move is not a wholesale renderer rewrite. It is a focused architectural pass:

- **Forward+ for light scale**
- **shared history management for temporal stability**
- **Vulkan-first, Metal-next platform strategy**

That sequence gets the renderer closer to 2026 expectations with less risk than chasing deferred, DXR, or research features prematurely.
