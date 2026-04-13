# Renderer validation

Evidence for **Tier B** (automated + `GAME_BASE`) and **Tier C** (manual GPU / validation layers). These tiers **do not change gameplay**; they exercise load paths, shaders, and parity checks.

## Quick start

| Goal | What to do |
|------|------------|
| **Tier B locally** | `GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_check.sh` then same with `renderer_regression_maps.sh`. Optional: `RELEASE_DIR` if binaries are not under `release/`. |
| **Tier B on GitHub** | Self-hosted runner with labels `self-hosted` + `idtech3-tierb`; set `IDTECH3_GAME_BASE_PATH` as a **variable** or **secret** (see [SELF_HOSTED_TIER_B.md](SELF_HOSTED_TIER_B.md)). Workflow: `.github/workflows/renderer-tier-b.yml` (push to `main` or **Run workflow**). |
| **Tier C (GPU notes)** | Follow the proof loop in [docs/RENDERER_CONFIDENCE.md](../RENDERER_CONFIDENCE.md); copy [TEMPLATE_TIER_C.md](TEMPLATE_TIER_C.md) or append a row to [FINDINGS.md](FINDINGS.md). |
| **Gap report** | `./scripts/evidence_status.sh` and optionally `GAME_BASE=... ./scripts/evidence_status.sh` |

| Document | Purpose |
|----------|---------|
| [SELF_HOSTED_TIER_B.md](SELF_HOSTED_TIER_B.md) | Enable Tier B on every `main` push via GitHub Actions + self-hosted runner |
| [TEMPLATE_TIER_C.md](TEMPLATE_TIER_C.md) | Copy per session; record date, commit, maps, Vulkan vs OpenGL, validation-layer result |
| [FINDINGS.md](FINDINGS.md) | Rolling log: Tier C GPU sessions + optional Tier A automated baseline rows |

Related: [docs/RENDERER_CONFIDENCE.md](../RENDERER_CONFIDENCE.md), [docs/PRODUCTION_CERTIFICATION.md](../PRODUCTION_CERTIFICATION.md).
