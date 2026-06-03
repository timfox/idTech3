# Production certification (engine and titles)

This document separates **what the engine repo can certify** from **what a shipped game certifies**. “AAA production certified” is not a single switch; it is **evidence** against explicit gates.

## Two different goals

| Scope | What “production” means | Owned by |
|--------|-------------------------|----------|
| **Engine** | Reproducible builds, CI green, smoke + regression scripts, documented manual GPU proof, stable ABI for mods | This repository |
| **Game / title** | Content complete, platform holder compliance (Sony / Microsoft / Nintendo), ratings, localization, live ops, crash telemetry, soak QA | Product team + each platform |

Shipping a **game** on this engine still requires a **game project** (content, rules, UI, networking policy). The engine can be **production-ready** while the title is not; conversely, a title can ship on a fork that never upstreams.

## Engine production-ready (target state)

Use this as the **internal bar** before calling an engine release “production”:

### Tier A - Automated (machine-verifiable)

- [ ] **Full GitHub Actions matrix green** on the release commit (`build` + any required workflows).
- [ ] **`./scripts/production_readiness.sh`** completes with exit code 0 on a clean tree (see script header for options).
- [ ] **No known Sev-1 issues** (data loss, security, deterministic crash on dedicated startup with documented minimal args) open against the milestone.

### Tier B - Content-backed (still automated, needs `GAME_BASE`)

- [ ] **`GAME_BASE`** points at a full `base/` with VM + assets + regression pack; **`./scripts/renderer_regression_maps.sh`** passes (see [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md)).
- [ ] **`./scripts/renderer_regression_check.sh`** with the same `GAME_BASE` passes optional BSP/manifest checks if enabled in `OPTIONAL_GAME_ASSETS.txt`.

### Tier C - Manual evidence (GPU + human)

- [ ] **Renderer proof loop** in [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md): Vulkan GPU pass on the regression scenes; findings recorded (e.g. under `docs/renderer_validation/` if your team maintains that).
- [ ] **Validation layers**: representative session with Vulkan validation clean (or waivers documented).
- [ ] **Performance spot-check**: target hardware list and FPS / frame time budget documented for at least one reference map.

### Tier D - Release hygiene

- [ ] [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) complete for the tag.
- [ ] **Versioned tag** (e.g. `v1.x.y`) and release notes.
- [ ] **Artifacts** per platform verified (install, run dedicated server, client smoke on a machine with a display where applicable).

## Title / “AAA” platform certification (out of scope for engine-only repo)

Console and storefront certification includes **legal, privacy, multiplayer, achievements, save data, and binary submission pipelines**. Those checks apply to **the shipping product**, not to the generic engine tree.

If you are aiming for **AAA-style quality**:

1. Treat **Tier A–D** above as the **engine prerequisite**.
2. Add a **title-specific** checklist (your game repo): soak tests, telemetry, cert matrix per platform, and submission builds.
3. Budget **fixed hardware labs** and **signed soak** (multi-hour sessions, memory leak checks, driver matrix).

## Stricter evidence (recommended order)

Tightening proof is mostly **feeding stable inputs** (same `GAME_BASE`, same regression maps, same hardware class) and **recording outcomes** so regressions are obvious.

| Step | Gate | What to do |
|------|------|------------|
| 1 | Tier A | Keep GitHub matrix green; locally run `./scripts/production_readiness.sh` (optionally with `GAME_BASE` set). |
| 2 | Tier B | Fix **`GAME_BASE`** on a machine you control; run `renderer_regression_check.sh` + `renderer_regression_maps.sh` on every release candidate. Automate on **`main`** via [SELF_HOSTED_TIER_B.md](renderer_validation/SELF_HOSTED_TIER_B.md) (`IDTECH3_GAME_BASE_PATH` + runner `idtech3-tierb`). |
| 3 | Tier C | After Tier B is green, run the [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md) proof loop on **Vulkan**; log rows in [FINDINGS.md](renderer_validation/FINDINGS.md) using [TEMPLATE_TIER_C.md](renderer_validation/TEMPLATE_TIER_C.md). |
| 4 | Tier D | Tag only after [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md); attach build artifacts you actually smoke-tested. |
| 5 | Title / AAA | Engine tiers **do not** replace platform cert: use [examples/title-repo/CERTIFICATION_CHECKLIST.md](../examples/title-repo/CERTIFICATION_CHECKLIST.md) in the **game** repo for telemetry, soak, and submission SKUs. |

**Quick gap report (local):** `./scripts/evidence_status.sh` - prints what is configured vs skipped (read-only; no secrets).

## How to move forward from current health

1. **Run the orchestrator weekly on `main`**: `./scripts/production_readiness.sh`
2. **Tier B on every `main` push**: enable [docs/renderer_validation/SELF_HOSTED_TIER_B.md](renderer_validation/SELF_HOSTED_TIER_B.md) - set variable `IDTECH3_GAME_BASE_PATH` and a self-hosted runner labeled `idtech3-tierb` (workflow `.github/workflows/renderer-tier-b.yml`).
3. **Tier C**: record GPU proof using [docs/renderer_validation/TEMPLATE_TIER_C.md](renderer_validation/TEMPLATE_TIER_C.md) and the rolling log [docs/renderer_validation/FINDINGS.md](renderer_validation/FINDINGS.md).
4. **Title repo**: copy [examples/title-repo/CERTIFICATION_CHECKLIST.md](examples/title-repo/CERTIFICATION_CHECKLIST.md) into your game repository for cert, telemetry, soak, and submission builds.
5. **Tighten warnings** over time (`CI_BUILD=ON` on a reference configuration) so new code cannot regress silently.

## Related documents

- [renderer_validation/README.md](renderer_validation/README.md) - Tier B self-hosted setup + Tier C templates  
- [examples/README.md](../examples/README.md) - copy-paste workflows (build, validation, renderer, mods)  
- [examples/title-repo/README.md](../examples/title-repo/README.md) - title certification checklist template  
- [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) - tag and ship steps  
- [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md) - renderer automated + manual gates  
- [QUICKSTART.md](QUICKSTART.md) - end-user runbook  
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) - developer environment  
