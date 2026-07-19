# Renderer Spine 1.0

**Date**: July 19, 2026  
**Status**: Active milestone (highest-value renderer work)  
**Audience**: Anyone choosing what to implement next in the Vulkan renderer

---

## Verdict

The renderer has reached **modern architectural breadth**. The primary challenge is no longer adding rendering categories, but consolidating them into a **reliable production spine**.

Architecture parity is **not** production parity. Competitive gains now come from:

- deterministic pass and resource ownership
- temporal correctness
- material parity across paths
- scalable shadowing
- GPU-driven geometry submission
- selective Hybrid1 integration

Experimental visibility-buffer, neural, and reservoir systems stay **downstream** of this stabilization effort.

> The gap is depth, integration, and reliability—not renderer category coverage.

---

## Maturity labels (revised)

Do **not** mark a path “Strong / shipping” only because the technique exists.

| Label | Meaning |
|-------|---------|
| **Architecturally strong** | Real implementation on the mode-3 / clustered shape; lighting ownership is explicit |
| **Production-hardening required** | Must survive the Spine validation matrix before counting as shipping-strong |
| **Experimental** | Outside the locked matrix; may regress or be latched off |

**Currently architectural, not yet shipping-strong** (until Spine 1.0 exit criteria pass):

- Heterogeneous mode 3 (Unified Clustered)
- Clustered Forward+
- WBOIT / MBOIT
- Froxel volumetrics

“Shipping strong” means surviving: `vid_restart`, resize, alt-tab, portals, weapon views, TAA on/off, OIT permutations, dynamic resolution, multiple maps, classic and malformed content, low-end fallbacks, and prolonged play without leaks or stale resources.

Feature completeness ≠ production readiness. A path can be feature-complete and only partially production-ready.

---

## Locked supported matrix

Spine 1.0 freezes **one** certified combination set. Everything else is experimental until it meets the same bar.

| System | Shipping path |
|--------|----------------|
| Opaque world | Deferred clustered (`r_renderMode 3` + deferred lighting) |
| Transparent world | Forward+ and/or WBOIT (`r_oit` 0/1); MBOIT (`r_oit` 2) hardens behind the same matrix |
| Weapon / view-model | Forward+ **after** world temporal reconstruction when TAA is on |
| Presentation AA | SMAA 1x (`r_aaMode 2`) — default |
| Temporal reconstruction | Optional, confidence-guided (`r_taa` / `r_aaMode` 4–5) |
| Shadows | Stable raster shadow path (cascades / atlas — deepen in Phase 2) |
| GI | Lightmaps + probes |
| Reflections | SSR with probe fallback |
| Volumetrics | Froxel path |
| Hybrid1 | Experimental quality tier (selective signals only) |
| Path tracing | Reference mode |

**Defaults contract** (product profile): `exec modern_vulkan.cfg` — mode 3 + SMAA; Temporal Reconstruction and Hybrid1 remain opt-in overlays.

Recovery: `renderer_modern_safe` / `renderer_clustered_safe` must remain boring and documented.

---

## Spine 1.0 engineering priorities

### 1. Frame / resource ownership (highest leverage)

Lightweight pass registry — **not** a full declarative frame-graph rewrite. Every resource needs explicit:

- producer / consumer
- format / dimensions / clear value
- layout and barrier contract
- history ownership
- lifetime, resize, and `vid_restart` behavior
- debug name

OIT corruption, black frames, and DEVICE_LOST-class failures are the failure mode this layer prevents.

### 2. Temporal correctness (before fancy reconstruction)

- Rigid + skeletal previous transforms
- Camera jitter conventions
- Disocclusion and transparency reactivity
- Weapon separation from world history
- Portal history isolation
- Camera-cut and projection-change resets
- Per-effect histories
- Rejection / ownership debug views

A clean SMAA frame beats an advanced temporal frame with trails.

### 3. Material parity

One interpretation of base color, normal, roughness, metallic, emissive, alpha test, transmission, clearcoat, anisotropy, lightmap contribution, and legacy shader behavior across Forward+, deferred, OIT, SSR, and RT/PT consumers.

### 4–6. After Spine exit (Phases 2–4)

Shadow depth, geometry throughput (instance → MDI → Hi-Z → meshlets), and **selective** Hybrid1 (hero shadows, reflections, limited GI)—not universal hybrid RT as a prerequisite for looking good.

---

## Roadmap phases (ordered)

| Phase | Name | Focus |
|-------|------|--------|
| **1** | **Stabilize (Spine 1.0)** | Resource ownership, OIT correctness, input/presentation regressions, temporal history validation, restart/resize, DEVICE_LOST, automated combination matrix |
| **2** | Deepen raster quality | Shadows, material parity, SSR/probes, bent-normal AV, volumetric temporal stability, transparency quality |
| **3** | Scale geometry | GPU culling, MDI, meshlets, occlusion, instance/material buffers |
| **4** | Production Hybrid1 | Selected RT shadows/reflections/limited GI, denoise, fallback ownership |
| **5** | Research renderer | Visibility late-shade, reservoirs, neural reconstruction, PT reference |

**Immediate priority:** Phase 1 only, until the supported matrix is boring.

---

## Exit criteria

Spine 1.0 is done when:

1. The locked matrix passes automated + manual combination tests (OIT × TAA × weapon × resize × alt-tab × `vid_restart` × windowed/FS).
2. Every Spine attachment used in that matrix has a debug name and a documented producer/consumer.
3. Temporal histories invalidate correctly for camera cut, projection change, resize, map change, and weapon/portal ownership.
4. `input_status` / relative mouse lifecycle remain correct across focus and `vid_restart` (presentation path).
5. DEVICE_LOST and black-frame classes from late post toggles are diagnosed with pass-diag context and do not recur on the certified profile.
6. Mode/feature combinations **outside** the matrix are latched or clearly experimental, with a safe recovery command.

Related checklists: [RENDERER_PHASE1_CHECKLIST_2026Q3.md](RENDERER_PHASE1_CHECKLIST_2026Q3.md), [RENDERER_MODERNIZATION_ROADMAP_2026H2.md](RENDERER_MODERNIZATION_ROADMAP_2026H2.md), [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md), [scripts/renderer_regression_check.sh](../scripts/renderer_regression_check.sh).

---

## What Cursor / contributors should work on

**Do next**

- Pass/resource registry and validation for Spine attachments
- Temporal history ownership and debug views
- OIT × TAA × weapon matrix hardening
- Restart / resize / focus / DEVICE_LOST reliability
- Expanding the automated combination matrix

**Do not next (unless fixing a Spine blocker)**

- New named techniques for category coverage
- Making Hybrid1 or path tracing required for the default look
- Expanding neural / visibility-buffer / reservoir research as default paths
- Full virtualized geometry or DLSS-class reconstruction before temporal data is correct

---

## Related docs

- [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md) — mode 3 architecture
- [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md) — Forward+ risks
- [HDR_GAPS.md](HDR_GAPS.md) — HDR / TAA / OIT order
- [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md) — OIT
- [RENDERER_2027.md](RENDERER_2027.md) — research north-star (after Spine)
- [RENDERERS.md](RENDERERS.md) — feature overview
