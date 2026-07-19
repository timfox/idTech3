# idTech3 documentation index (2026 tier layout)

Documentation is grouped by **tier**. Most files remain at their historical paths; subfolder READMEs link here until Phase 6 physical moves.

## Core (engine + build)

- [ARCHITECTURE.md](ARCHITECTURE.md)
- [COMPATIBILITY.md](COMPATIBILITY.md)
- [BUILD.md](../BUILD.md)
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md)
- [ENGINE_MODULE_MANIFEST.md](ENGINE_MODULE_MANIFEST.md) — build profile + source gates
- [DEPRECATION_POLICY.md](DEPRECATION_POLICY.md)

## Modules (world / gameplay)

- [OPEN_WORLD.md](OPEN_WORLD.md)
- [WORLD_CONFIG.md](WORLD_CONFIG.md) — named map-state configs (geometry/nav/spawns/lighting)
- [DISTRICTS.md](DISTRICTS.md)
- [PHYSICS.md](PHYSICS.md)
- [PROC_PATTERNS.md](PROC_PATTERNS.md)
- [FOG_BIOLOGY.md](FOG_BIOLOGY.md)
- [GENETIC_GAN.md](GENETIC_GAN.md)

## Extensions

### Generative / ML client

- [GENETIC_GAN.md](GENETIC_GAN.md)
- FLUX / TRELLIS — see AGENTS.md gotchas

### Neural renderer pack

- [NEURAL_RENDERER_PHASES.md](NEURAL_RENDERER_PHASES.md)
- Child docs: NIV, NDGI, NVC, Hybrid1, etc.

### Research (paper repro)

- [RADIUSFPS.md](RADIUSFPS.md), [DAX.md](DAX.md), [GCCFER.md](GCCFER.md), [X3DPRA.md](X3DPRA.md)
- [VUDA.md](VUDA.md), [VKSPLAT.md](VKSPLAT.md), [CURAST.md](CURAST.md), [MIMIR.md](MIMIR.md), [IRIS.md](IRIS.md)
- [HOWDARK.md](HOWDARK.md) — Filip & Vávra black materials (arXiv:2601.05094)
- [NEBRDF.md](NEBRDF.md) — Shen et al. neural-enhanced analytical BRDF (arXiv:2604.24081)
- [RTFEM.md](RTFEM.md) — Parker & O’Brien SCA 2009 real-time FEM/fracture scaffold
- [VGS.md](VGS.md) — McGraw MIG 2024 Gram-Schmidt voxel soft-body scaffold
- [CEM.md](CEM.md) — Xie et al. arXiv:2508.04076 3D Crack Element Method scaffold

### Optional submodules (external trees)

- [IDTECH3_BACKEND.md](IDTECH3_BACKEND.md) — game/backend + App CRDT
- [IDTECH3_EMULATOR.md](IDTECH3_EMULATOR.md) — QEMU fork for in-world OS sandbox (render texture path planned)

## Guides

- [QUICKSTART.md](QUICKSTART.md)
- [MOD_SDK.md](MOD_SDK.md)
- [VOXEL_SPRITES.md](VOXEL_SPRITES.md) — MagicaVoxel `.vox` cube-mesh props (`misc_voxel` / `voxel_spawn`)
- [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md) — `r_renderMode 3` Unified Clustered (heterogeneous shading / lighting ownership)
- [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md) — production-certified frame matrix (stabilize before new techniques)
- [RENDERER_2027.md](RENDERER_2027.md) — GPU-driven hybrid visibility north-star (vis buffer Phase 1)
- [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md) — MBOIT (`r_oit 2`) + stochastic alpha clip
- [RADIANT.md](RADIANT.md)
- [VOIP.md](VOIP.md) — Opus voice chat + head lip flap
- [P2P_NETWORKING.md](P2P_NETWORKING.md) — Steam SDR / direct_udp P2P, ICE-lite, reconnect/migrate
- [P2P_NAT_TESTING.md](P2P_NAT_TESTING.md) — P2P NAT CI tiers and live harnesses
- [IDTECH3_TV.md](IDTECH3_TV.md) — external idTech3-tv / Owncast live-stream publishing
- [TORRENT_CONTENT.md](TORRENT_CONTENT.md) — optional peer-assisted `.pk3` package delivery
- [OSCAR_INTEGRATION.md](OSCAR_INTEGRATION.md) — Open OSCAR hybrid AIM (direct FLAP/BOS + Chat rooms, roster, client ImGui)
- [FACS.md](FACS.md) — Facial Action Coding System Action Units

## Repository layout (2026)

Top-level symlinks (`engine/`, `runtime/`, `modules/`, `extensions/`, `renderers/`) → canonical `src/*` tree. **Legacy paths under `src/` remain authoritative.** See [core/REPOSITORY_LAYOUT_2026.md](core/REPOSITORY_LAYOUT_2026.md), [core/LEGACY_AND_MODERN.md](core/LEGACY_AND_MODERN.md), and [ENGINE_REORG_PLAN.md](ENGINE_REORG_PLAN.md).

## Archive

Historical FBO audits and stale roadmaps — see [docs/archive/fbo_investigation.md](archive/fbo_investigation.md).
