# Platform and live-ops (Phase D — product-gated)

Work in this document starts only when the product SKU requires it. **SP conversion (Phase A–C) does not depend on Phase D.**

| Track | Trigger | Engine artifacts |
|-------|---------|------------------|
| Native Metal | macOS ship without MoltenVK | `USE_METAL_RENDERER`, [METAL_RENDERER.md](METAL_RENDERER.md) |
| Steam / Epic | PC store release | `USE_STEAM` in CMake, achievements via game mod |
| Android tier | Mobile SKU | [ANDROID.md](ANDROID.md), touch + thermal presets |
| Remote config | Live service | HTTP + cvar latch ([CURL_NETWORKING.md](CURL_NETWORKING.md)); no hosted stack in-engine |
| Matchmaking / MMR | Competitive MP | External service; not in `src/` |
| Replication graph | Live MP at scale | Design doc only; Q3 snapshots remain baseline |
| Client rollback | Competitive MP | Not SP blocker |
| Networked audio | VOIP relevance at scale | Server events + interest cull |
| Console cert | Console SKU | TRC/XR — out of scope until platform chosen |
| Full asset DB / cook | Large studio pipeline | Beyond `validate_assets.sh` |

## Already available (hooks)

- Master server list — classic `sv_master` / client browser
- `sv_pureSigned`, `sv_auth`, interest cull — [ANTICHEAT_INTEGRATION.md](ANTICHEAT_INTEGRATION.md)
- MoltenVK — default macOS path today

## When enabling a row

1. Add CMake option + startup log line (constitution).
2. Document cvars in [MOD_SDK.md](MOD_SDK.md) or platform doc.
3. Add `ctest` or smoke script if automatable without secrets.
