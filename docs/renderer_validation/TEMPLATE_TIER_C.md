# Tier C manual session - template

Copy this file to a dated entry (e.g. `tier_c_2026-04-06.md`) or append a section to [FINDINGS.md](FINDINGS.md).

## Metadata

| Field | Value |
|--------|--------|
| **Date** | YYYY-MM-DD (local) |
| **Engine commit** | `git rev-parse HEAD` (short + full) |
| **Operator** | Name / initials |
| **Platform** | e.g. Linux x86_64, Windows 11, macOS 14 |
| **GPU / driver** | e.g. RTX 4070, driver 550.xx |
| **Client build** | e.g. Release Vulkan from `release/idtech3`, or MSVC path |

## Content

| Field | Value |
|--------|--------|
| **GAME_BASE** | Absolute path to `base` (do not commit secrets; use “internal lab path”) |
| **Regression maps run** | List: `rtest_parity`, `rtest_volumetric`, … |

## Renderer paths

| Path | Result (pass / fail / N/A) | Notes |
|------|----------------------------|--------|
| **Vulkan** | | |
| **OpenGL** | | |

Brief notes (black screen, wrong fog, parity diff, etc.):

## Validation layers (Vulkan)

| Setting | Value |
|---------|--------|
| **Enabled?** | e.g. `r_vulkan_validation 1` or debug build |
| **Result** | Clean / warnings / errors (summarize) |
| **Log excerpt** | Attach file or link (do not paste huge logs in repo) |

## Evidence (optional)

- Screenshot paths or links (internal storage OK)
- Short screen capture link

## Sign-off

- [ ] Tier C checklist in [docs/RENDERER_CONFIDENCE.md](../RENDERER_CONFIDENCE.md) addressed for this milestone
- [ ] Follow-up issues filed (IDs: …)
