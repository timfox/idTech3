# idTech3 documentation index (2026 tier layout)

Documentation is grouped by **tier**. Most files remain at their historical paths; subfolder READMEs link here until Phase 6 physical moves.

## Core (engine + build)

- [ARCHITECTURE.md](ARCHITECTURE.md)
- [COMPATIBILITY.md](COMPATIBILITY.md)
- [BUILD.md](../BUILD.md)
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md)
- [STEAM.md](STEAM.md) — Steamworks API (`USE_STEAM`), Deck, achievements, overlay
- [OPENHMD.md](OPENHMD.md) — OpenHMD VR head tracking / software stereo (`USE_OPENHMD`)
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
- [COLLADA.md](COLLADA.md) — native renderer `.dae` static model loading, separate from PMD/PSA conversion tooling
- [VOXEL_SPRITES.md](VOXEL_SPRITES.md) — MagicaVoxel `.vox` cube-mesh props (`misc_voxel` / `voxel_spawn`)
- [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md) — `r_renderMode 3` Unified Clustered (heterogeneous shading / lighting ownership)
- [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) — surface-class path owners, shared clusters, `R_SelectSurfaceRenderPath`
- [CLUSTERED_LIGHTING.md](CLUSTERED_LIGHTING.md) — Clustered Hybrid M2 log-Z grid, compact lists, overflow, hybrid compare
- [CPP20_MIGRATION.md](CPP20_MIGRATION.md) — slow C→C++20 compatibility migration (foundation)
- [CPP23_MIGRATION.md](CPP23_MIGRATION.md) — staged plan for raising converted engine-owned C++ from C++20 to C++23
- [MODULARIZATION_GUIDE.md](MODULARIZATION_GUIDE.md) — rules and validation for further source-domain splits
- [RASTER_ULTRA_1.0.md](RASTER_ULTRA_1.0.md) — Raster Ultra base (high-end raster-only; RT locked off). Later milestones: [1.1](RASTER_ULTRA_1.1.md) lighting · [1.3](RASTER_ULTRA_1.3.md) probe GI · [1.4](RASTER_ULTRA_1.4.md) transparency/particles/decals · [1.5](RASTER_ULTRA_1.5.md) present-time AA · [1.6](RASTER_ULTRA_1.6.md) GPU geometry · [1.7](RASTER_ULTRA_1.7.md) atmosphere/weather · [1.8](RASTER_ULTRA_1.8.md) materials · [1.9](RASTER_ULTRA_1.9.md) virtual shadows · [1.10](RASTER_ULTRA_1.10.md) HDR presentation · [1.11](RASTER_ULTRA_1.11.md) reference lab · [1.12](RASTER_ULTRA_1.12.md) frequency-aware / moiré suppression
- [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md) — production-certified frame matrix (stabilize before new techniques)- [AUTHORED_MESH_NORMALS.md](AUTHORED_MESH_NORMALS.md) — experimental DCC hard-edge / authored-normal import policy
- [RENDERER_2027.md](RENDERER_2027.md) — GPU-driven hybrid visibility north-star (vis buffer Phase 1)
- [COLOR_PIPELINE.md](COLOR_PIPELINE.md) — authoritative color spaces + 17-stage order; WBOIT production OIT (`color_pipeline_status`)
- [WBOIT_CONTRACT.md](WBOIT_CONTRACT.md) — Phase 2.1 frozen WBOIT formats/blends/equations (`oit_contract_status`)
- [WBOIT_ALPHA_ENCODING.md](WBOIT_ALPHA_ENCODING.md) — Phase 2.2 source vs internal alpha (`oit_alpha_status`)
- [DEPTH_CONTRACT.md](DEPTH_CONTRACT.md) — Phase 2.3.1 frozen reversed-Z / view-depth (`depth_contract_status`)
- [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md) — production WBOIT (`r_oit 1`), experimental MBOIT (`r_oit 2`), stochastic alpha
- [WBOIT_GPU_CERTIFICATION.md](WBOIT_GPU_CERTIFICATION.md) — live GPU cert levels, B0–B7 operator runner, soak (`oit_certify_wboit` / `oit_soak_wboit`)
- [WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md) — fog-through-layers pass order, `r_oitFogMode`, no double-fog on resolve
- [OIT_FUTURE_TRACKS.md](OIT_FUTURE_TRACKS.md) — deferred OIT research tracks (not this milestone)
- [UI_BLUR.md](UI_BLUR.md) — CSS-style `filter` / `backdrop-filter` blur for the JS UI compositor (`ui_blurQuality`, `ui_filterDebug`)
- [RADIANT.md](RADIANT.md)
- [VOIP.md](VOIP.md) — Opus voice chat + head lip flap
- [P2P_NETWORKING.md](P2P_NETWORKING.md) — Steam SDR / direct_udp P2P, ICE-lite, reconnect/migrate
- [P2P_NAT_TESTING.md](P2P_NAT_TESTING.md) — P2P NAT CI tiers and live harnesses
- [IDTECH3_TV.md](IDTECH3_TV.md) — external idTech3-tv / Owncast live-stream publishing
- [TORRENT_CONTENT.md](TORRENT_CONTENT.md) — optional peer-assisted `.pk3` package delivery
- [OSCAR_INTEGRATION.md](OSCAR_INTEGRATION.md) — Open OSCAR hybrid AIM (direct FLAP/BOS + Chat rooms, roster, client ImGui)
- [FACS.md](FACS.md) — Facial Action Coding System Action Units

## Repository layout (2026)

Canonical source roots are `engine/`, `runtime/`, `modules/`, `extensions/`, `renderers/`, and `third_party/`. The old `src/*` forwarding shims are gone; only layout bridges for remaining cross-domain includes/MSVC compatibility remain. See [core/REPOSITORY_LAYOUT_2026.md](core/REPOSITORY_LAYOUT_2026.md), [core/LEGACY_AND_MODERN.md](core/LEGACY_AND_MODERN.md), and [ENGINE_REORG_PLAN.md](ENGINE_REORG_PLAN.md).

## Archive

Historical FBO audits and stale roadmaps — see [docs/archive/fbo_investigation.md](archive/fbo_investigation.md).
