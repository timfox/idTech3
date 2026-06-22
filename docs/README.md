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

## Guides

- [QUICKSTART.md](QUICKSTART.md)
- [MOD_SDK.md](MOD_SDK.md)
- [RADIANT.md](RADIANT.md)

## Repository layout (2026)

Top-level symlinks (`engine/`, `runtime/`, `modules/`, `extensions/`, `renderers/`) → canonical `src/*` tree. **Legacy paths under `src/` remain authoritative.** See [core/REPOSITORY_LAYOUT_2026.md](core/REPOSITORY_LAYOUT_2026.md), [core/LEGACY_AND_MODERN.md](core/LEGACY_AND_MODERN.md), and [ENGINE_REORG_PLAN.md](ENGINE_REORG_PLAN.md).

## Archive

Historical FBO audits and stale roadmaps — see [docs/archive/fbo_investigation.md](archive/fbo_investigation.md).
