# Renderer Modernization Roadmap (2026 H2)

**Date**: July 19, 2026 (Spine 1.0 framing)  
**Scope**: Vulkan renderer stabilization, lighting architecture, temporal behavior, pass ownership, and backend prioritization

---

## Purpose

This document defines the practical renderer modernization plan for the second half of 2026.

**Canonical next milestone:** [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md) — production-certified frame architecture and locked feature matrix. Architecture breadth is already high; the work is depth, integration, and reliability.

The goal is not to replace the renderer with a brand-new framegraph or to chase every experimental rendering tier in parallel. The goal is to make the existing Vulkan path boringly reliable first, then widen capability in a controlled way.

This roadmap aligns with:

- [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md)
- [ROADMAP.md](ROADMAP.md)
- [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md)
- [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- [RENDERER_PHASE1_CHECKLIST_2026Q3.md](RENDERER_PHASE1_CHECKLIST_2026Q3.md)

---

## Core Strategy

The right modernization strategy for 2026 is:

1. Stabilize the shipping Vulkan path first.
2. Finish the current lighting architecture instead of replacing it.
3. Treat temporal behavior as a subsystem, not as isolated fixes.
4. Make pass ownership and state transitions explicit.
5. Improve material and transparency correctness before adding more effects.
6. Use the existing G-buffer as shared infrastructure.
7. Keep platform priorities disciplined.

---

## Current Known Instabilities

As of **July 18, 2026**, the renderer still has concrete stability issues that should be treated as **Phase 1 blockers**, not as polish items.

### Reproduced Vulkan failure class

Recent live bisecting on the shipping config stack isolated a failure where the renderer could survive the broader mode-2 modern feature set, but still fall into:

- black output
- corrupted intermediate frames
- **`VK_ERROR_DEVICE_LOST`**
- worker/driver-thread crash during shutdown after device loss

The notable part of this repro is that it did **not** require enabling an entirely new renderer mode. It was triggered by a final post/bloom tuning delta on top of an otherwise working modern stack. That strongly suggests the project still needs stronger guarantees around:

- post-pass ownership and resume behavior
- attachment/state lifetime correctness
- feature-toggle safety inside an already-running Vulkan frame pipeline
- device-loss handling and reporting when a pass destabilizes the driver

### What this means for roadmap priority

- Stability work is still the top renderer task for 2026 H2.
- "Looks mostly fine except for one late pass" is **not** good enough for the shipping path.
- Mode 2 should remain the product path, but only with conservative defaults until these failure classes are closed.

---

## Strategic Decisions

### 1. Stabilize the Spine shipping path first

Treat **Unified Clustered (`r_renderMode 3`)** as the **architectural product path**, but only as **shipping-strong** after [Renderer Spine 1.0](RENDERER_SPINE_1.0.md) exit criteria.

Certified defaults (when Spine-certified):

- Deferred opaque + Forward+ transparent / weapon
- HDR + PBR
- Shared light-grid / clustered lists
- SMAA 1x presentation AA (`r_aaMode 2`)
- Temporal Reconstruction optional and confidence-guided

Keep **`r_renderMode 0` / `1` / `2`** as supported fallbacks and recovery profiles (`renderer_modern_safe`), not as competing “add another modern path” targets. Hybrid1, path tracing, visibility-buffer, and neural stacks stay **experimental** until the Spine matrix is boring.

#### Required work

- Pass/resource ownership registry (producer, consumer, layout, history, resize/`vid_restart`).
- Expand runtime validation around `renderer_profile`, `renderer_status`, render-mode latches, pass ordering, and AA handoff.
- Combination matrix: OIT × TAA × weapon × resize × alt-tab × `vid_restart`.
- Keep recovery commands and safe renderer profiles documented and reliable.

#### Non-goals

- Do not treat every renderer mode or research overlay as equally production-ready.
- Do not add new named techniques for category coverage until Spine 1.0 exits.
- Do not make Hybrid1 or path tracing a prerequisite for the default look.

### 2. Finish the lighting architecture

Keep **Forward+** as the default lighting architecture.

Use deferred as a correct opaque-lighting path, not as the new universal default.

Treat **mode 3 / Unified Clustered** as a hybrid target:

- opaque deferred lighting
- transparent Forward+
- shared clustered light lists

#### Required work

- Finish deferred opaque correctness: direct/specular balance, AO coupling, material classification, and debug views.
- Finish clustered transparent handoff and OIT interaction.
- Keep lighting scale work focused on opaque scalability without forcing transparent/content paths into a full-deferred model.

#### Non-goals

- Do not chase "full deferred everywhere."
- Do not let experimental hybrid paths displace the stable mode-2 shipping path.

### 3. Make temporal behavior a real subsystem

Temporal behavior must be handled centrally instead of one feature at a time.

#### Required work

- Centralize reset/invalidation for:
  - resize
  - map load
  - camera cut
  - FOV jump
  - render-scale change
  - bad or missing motion data
- Close motion-vector gaps for:
  - CPU-skinned geometry
  - custom-shader deformation
  - billboards
  - first-person geometry
- Add debug overlays for:
  - history confidence
  - invalidation reasons
  - reactive/problem pixels

#### Policy

- Keep TAA optional until motion confidence is trustworthy enough that it helps more than it hides bugs.

### 4. Unify pass ownership and state transitions

Continue replacing implicit pass restoration with explicit helpers and documented contracts.

#### Required work

- Remove ad hoc pass detours where possible.
- Make resume behavior explicit for the main scene pass.
- Document lightweight pass/resource ownership before attempting a full framegraph rewrite.

#### Non-goals

- Do not build a large framegraph system before current pass ownership is reliable.

### 5. Raise material and transparency quality

Quality work should prioritize correctness over effect count.

#### Required work

- Improve direct G-buffer export quality for roughness, metalness, and AO.
- Make transparency paths explicit about whether they are:
  - lit
  - deferred-backed
  - isolated
- Keep PBR energy behavior and material consistency ahead of adding more screen-space or hybrid features.

### 6. Use the G-buffer as shared infrastructure

The mode-2 sidecar G-buffer should be treated as shared substrate, not as a one-off deferred artifact.

#### Required work

- Reuse G-buffer data for temporal, debug, neural, and future RT consumers where it makes sense.
- Avoid creating separate one-off material/visibility buffers when existing G-buffer data can serve the same purpose.
- Build future classification and visibility features on top of shared data contracts.

### 7. De-risk platform strategy

For 2026, the shipping renderer strategy remains:

- Vulkan-only shipping backend
- Metal as the next real backend investment if expansion matters
- RTX / DXR / research renderers behind the core renderer, not in front of it

#### Non-goals

- Do not pivot the product path around RTX.
- Do not spread stabilization effort across too many backend targets at once.

---

## Execution Plan

### Phase 1: July-August 2026

- Lock down mode contracts and recovery commands.
- Add runtime/source diagnostics for:
  - render-pass ownership
  - AA source selection
  - clustered handoff
  - G-buffer validity
- Add targeted validation around post/bloom tuning and other late-frame toggles that can still trigger black output or **`VK_ERROR_DEVICE_LOST`** inside an otherwise working mode-2 stack.
- Improve device-loss diagnostics so the engine reports the active renderer profile, recent pass/toggle changes, and likely failing stage before recursive shutdown noise takes over.
- Eliminate remaining black-scene and device-loss regressions in mode 1 and mode 3.
- Eliminate mode-2 late-pass/device-loss regressions before widening the default high-end profile again.

### Phase 2: August-September 2026

- Finish deferred opaque correctness:
  - lighting
  - specular
  - AO coupling
  - material-class usage
  - debug views
- Finish clustered transparent handoff and OIT interaction.
- Add a small set of GPU/manual validation scenes for:
  - many-light stress
  - transparency stress
  - mixed-material stress

### Phase 3: September-October 2026

- Close temporal motion coverage gaps.
- Add overlays for history confidence, invalidation reasons, and reactive/problem pixels.
- Re-evaluate whether TAA should remain default in every profile or only in safer modern profiles.

### Phase 4: Late 2026

- Build a minimal pass-graph/resource-lifetime layer.
- Expand G-buffer consumers cleanly:
  - visibility-side systems
  - neural/RT sidecars
  - future hybrid passes
- Only after that, resume deeper Hybrid1/RTX/advanced research-tier work.

---

## Exit Criteria

The renderer is "modernized enough" for this phase when all of the following are true:

- Mode 2 is stable and visually consistent across ordinary maps.
- Mode 1 and mode 3 no longer black-screen, corrupt, or trigger device loss in normal play.
- Temporal systems report clear reset reasons and behave predictably.
- Transparency lighting rules are explicit and test-covered.
- New renderer features plug into shared pass/data contracts instead of inventing new one-off paths.

---

## Summary

The 2026 H2 renderer plan is not "rewrite everything."

It is:

- stabilize the shipping Vulkan path
- finish the current Forward+/deferred hybrid architecture
- make temporal behavior and pass ownership explicit
- use shared G-buffer infrastructure
- delay research-tier expansion until the product path is stable

That sequence gives the renderer a credible modernization path with lower risk than a wholesale architectural reset.
