# API Stability Charter

This document formalizes the **layer cake** contract from [CLAUDE.md](../CLAUDE.md) for mod authors, native game modules, and idtech3.com SDK consumers.

## Layers

| Layer | Scope | Stability | Breaking changes |
|-------|--------|-----------|------------------|
| **Vanilla** | Original Q3A APIs (`entityState_t` core fields, QVM syscalls 0–106, network baseline) | Eternal | Never without MAJOR semver + migration guide |
| **Chocolate** | Optional enhancements (PBR, volumetrics, engine sprites, Forward+) | Stable when documented; toggled by cvar | MINOR deprecation cycle ([DEPRECATION_POLICY.md](DEPRECATION_POLICY.md)) |
| **Layercake** | New traps, CS blocks, Lua `Engine.*`, Studio tools | Documented in [MOD_SDK.md](MOD_SDK.md) | MINOR deprecation; append-only trap IDs |

## Semver (engine releases)

- **MAJOR**: Network wire-format break (e.g. `eFlags` bit width), removed traps, incompatible pk3 pure rules
- **MINOR**: New traps, new CS ranges, new `EF_*` flags (with wire room), new cvars (default off)
- **PATCH**: Bug fixes, docs, regression tests; no ABI break

Engine version is exposed via `com_version` / release tags. Layercake API semver is **`com_engine_api`** (`IDTECH3_ENGINE_API_MAJOR.MINOR` in [q_shared.h](../src/qcommon/q_shared.h)). Mods should declare `requires_engine >= X.Y` in `gameinfo.txt`; startup logs a **warning** if the running engine API is older (non-fatal).

## Trap numbering rules

### Game (`g_public.h`)

- Vanilla traps: unchanged indices through `G_TESTPRINTFLOAT`
- Engine extensions: append **before** `G_TRAP_GETVALUE` (= `COM_TRAP_GETVALUE` = 700)
- Never insert or renumber existing traps
- New traps require: handler in `sv_game.c`, entry in [MOD_SDK.md](MOD_SDK.md), regression grep in `scripts/renderer_regression_check.sh` when renderer-adjacent

### Cgame (`cg_public.h`)

- Same append-before-`CG_TRAP_GETVALUE` rule
- Register `trap_GetValue` alias in `CL_GetValue()` (`cl_cgame.c`)

## Configstring (`CS_*`) allocation

- All new catalogs extend after the previous `CS_MAX` block in [bg_public.h](../src/game/bg_public.h)
- Each catalog: `CS_FOO`, `MAX_FOO`, optional `CS_FOO_META` for spawn/dedup counts
- `#if (CS_MAX) > MAX_CONFIGSTRINGS` guard must pass
- Document field contract in server module header (see `sv_engine_sprites.c`)

## Networked entity flags (`EF_*`)

Engine flags live in [q_shared.h](../src/qcommon/q_shared.h) bits 20+ and are serialized via `entityStateFields` in [msg.c](../src/qcommon/msg.c). When adding flags, ensure `NETF(eFlags)` bit width covers them (currently **24 bits**).

## PR checklist (layer assignment)

- [ ] Which layer? (vanilla / chocolate / layercake)
- [ ] Cvar toggle + startup log line for new code paths
- [ ] Fallback when disabled (no crash, degraded OK)
- [ ] [MOD_SDK.md](MOD_SDK.md) updated if mod-visible
- [ ] Regression or unit test for non-trivial behavior
- [ ] [DEPRECATION_POLICY.md](DEPRECATION_POLICY.md) if removing/changing chocolate/layercake API

## References

- [DEPRECATION_POLICY.md](DEPRECATION_POLICY.md)
- [COMPATIBILITY.md](COMPATIBILITY.md)
- [MOD_SDK.md](MOD_SDK.md)
- [BRANCHES.md](BRANCHES.md)
