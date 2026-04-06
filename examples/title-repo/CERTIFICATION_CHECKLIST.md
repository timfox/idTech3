# Title / product certification checklist (copy to your game repo)

**This file is a template.** Copy it into your **game** repository (not the engine fork) and customize for each platform SKU.

The engine’s automated bar lives in [docs/PRODUCTION_CERTIFICATION.md](../../docs/PRODUCTION_CERTIFICATION.md) and [docs/renderer_validation/](../../docs/renderer_validation/). This checklist covers **shipping a title**: compliance, telemetry, soak, and submission binaries.

---

## 1. Engine baseline (prerequisite)

- [ ] Pinned engine **tag or commit** recorded in game repo (submodule, vendor tree, or release artifact hash).
- [ ] **Tier A** green on that engine revision (CI matrix + `production_readiness.sh` as applicable).
- [ ] **Tier B** green if you maintain `GAME_BASE` regression (self-hosted or internal pipeline).
- [ ] **Tier C** recorded for your art style / maps ([docs/renderer_validation/TEMPLATE_TIER_C.md](../../docs/renderer_validation/TEMPLATE_TIER_C.md)).

## 2. Telemetry and diagnostics

- [ ] **Crash reporting** integrated (backend chosen; symbols uploaded for native builds).
- [ ] **Privacy**: data collection disclosed; regions/GDPR/CCPA as required.
- [ ] **PII**: no prohibited fields in logs; retention policy defined.
- [ ] **Engine identifiers**: build id, engine version, `fs_game`, renderer string sent with crashes.

## 3. Soak and stability

- [ ] **Multi-hour soak** on reference hardware (client + server if applicable).
- [ ] **Memory**: leak checks or long-run profiling on target platforms.
- [ ] **Save/load** (if applicable): corruption and edge cases.
- [ ] **Networking**: reconnect, NAT, dedicated + listen where supported.

## 4. Platform holder / store compliance (per SKU)

Duplicate this block per platform (PlayStation, Xbox, Nintendo, Steam Deck verified, Epic, etc.):

### Platform: _______________

- [ ] **Submission binary** pipeline documented (who builds, where artifacts live).
- [ ] **Cert requirements** checklist from platform docs completed (TCRs/TRCs).
- [ ] **Age rating** / questionnaire submitted.
- [ ] **Localization** scope vs cert build.
- [ ] **Offline / DRM** behavior matches store policy.

## 5. Release engineering

- [ ] Versioning scheme (semantic vs marketing).
- [ ] **Changelog** and player-facing patch notes.
- [ ] **Rollback** plan for bad build.
- [ ] **Staging** environment tested before production promotion.

## 6. Sign-off

| Role | Name | Date |
|------|------|------|
| Producer | | |
| Engineering lead | | |
| QA lead | | |
| Compliance / legal | | |

---

*End of template.*
