# Renderer Modernization Roadmap (2026 H2)

**Date**: July 18, 2026  
**Scope**: Vulkan renderer stabilization, lighting architecture, temporal behavior, pass ownership, and backend prioritization

---

## Purpose

This document defines the practical renderer modernization plan for the second half of 2026.

The goal is not to replace the renderer with a brand-new framegraph or to chase every experimental rendering tier in parallel. The goal is to make the existing Vulkan path boringly reliable first, then widen capability in a controlled way.

This roadmap aligns with:

- [ROADMAP.md](ROADMAP.md)
- [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md)
- [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)

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

## Strategic Decisions

### 1. Stabilize the shipping path first

Treat **`r_renderMode 2`** as the product path:

- Forward+ primary
- HDR + PBR
- sidecar G-buffer
- TAA / motion-vector capable path

Keep **`r_renderMode 1`** and **`r_renderMode 3`** explicitly experimental until they stop producing:

- black frames
- corrupted output
- device-loss crashes

#### Required work

- Expand runtime validation around `renderer_profile`, `renderer_status`, render-mode latches, pass ordering, and AA handoff.
- Add more source/runtime guards around render-pass resume behavior and attachment/state correctness.
- Keep recovery commands and safe renderer profiles documented and reliable.

#### Non-goals

- Do not treat every renderer mode as equally production-ready.
- Do not broaden default profiles while mode-specific instability still exists.

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
- Eliminate remaining black-scene and device-loss regressions in mode 1 and mode 3.

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
