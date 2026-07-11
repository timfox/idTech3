# Engine diet & reorganization plan (capability-preserving)

**North star:** shrink what everyone pays for by default; keep every capability reachable via **extensions + build profiles**.

| Principle | Rule |
|-----------|------|
| No capability loss | `full` / `research` profile or `-DUSE_*=ON` |
| **Legacy intact** | QVM, pk3, traps, cvars; **`src/*` forwarding shims** → `engine/` / `runtime/` / `modules/` |
| No mod break | One-release shims; see [LEGACY_AND_MODERN.md](core/LEGACY_AND_MODERN.md) |
| Layer cake → folders | Core → Modules → Extensions ([BRANCHES.md](BRANCHES.md)) |
| Incremental | One domain per PR + wiring tests |

## Build profiles (implemented)

| Profile | Audience | Default ON |
|---------|----------|------------|
| `core` | Q3/OA compat, minimal server | Engine core, Vulkan forward, classic audio, Lua |
| `game` | SP conversion (**2026 default**) | core + physics + nav + world + FreeUSD |
| `full` | Kitchen-sink parity | game + generative + experimental renderers + research |
| `research` | Paper repro | same gates as `full` |

```bash
./scripts/compile_engine.sh vulkan          # profile=game
./scripts/compile_engine.sh vulkan full     # today's kitchen sink
./scripts/compile_engine.sh vulkan core     # CI fast path
./scripts/compile_engine.sh vulkan research
```

See [BUILD.md](../BUILD.md) profile matrix and [ENGINE_MODULE_MANIFEST.md](ENGINE_MODULE_MANIFEST.md).

## Phase status

| Phase | Scope | Status |
|-------|--------|--------|
| 0 | Manifest, profiles, audit script, presets | Done |
| 1 | Gates: research, generative, open world, experimental renderers | Done |
| 2 | `src/extensions/{research,generative}/` | Done |
| 3 | Vulkan core vs extension CMake manifests | Done (physical subdirs optional) |
| 4 | Docs tier, `scripts/ci/`, `scripts/extensions/`, samples alias | Done |
| 5a | `engine/` + `runtime/` + `modules/` symlinks + `IdTech3Layout.cmake` | Done |
| 5b | CMake manifests + no AUX on qcommon/server/vulkan/client/botlib/mp3 | Done (vendored JPEG `EXTERNAL_JPEG_SRC_DIR` remains on AUX) |
| 5c | Physical move off `src/`; `src/*` forwarding-only shims; botlib/cgame/ui/asm relocated | Done |
| 5d | MSVC bridge + manifest sync (quake3e/ded/botlib/vulkan); CI drift tests | Done |
| 5e | Drop `src/*` shims (layout bridges kept for includes/MSVC) | Done |

## Next high-value PRs (structure-first)

1. ~~**Runtime client folders**~~ — `src/client/{core,world,media,platform}` done; generative stays in `extensions/generative/`.
2. ~~**`USE_GAME_AI_MIDDLEWARE`**~~ — Director/GOAP/Horde/BT gated; `g_engine_systems` always on.
3. ~~**`renderers/vulkan/extensions/`**~~ — `neural/`, `splats/`, `rtx/`, `scaffold/` done.
4. ~~**`third_party/`**~~ — symlink + `IDTECH3_DIR_THIRD_PARTY` in `IdTech3Layout.cmake`.
5. ~~**Docs archive**~~ — `docs/archive/fbo_investigation.md`.
6. ~~**Phase 5b**~~ — explicit manifests for client, qcommon, server, vulkan core (no AUX).
7. ~~**Phase 5c**~~ — physical move to `engine/`, `runtime/`, `modules/`, `extensions/`, `renderers/`, `third_party/`; `src/*` shims for one release. Script: `scripts/migrate_phase_5c.sh`.
8. ~~**Phase 5d**~~ — MSVC manifest sync + CI (`scripts/msvc/sync_all_vcxproj.sh`). See **`docs/MSVC_CODEGEN.md`**.
9. **Phase 5e** — drop `src/*` shims after CMake manifest migration. Layout bridges remain until include rewrite. See **`docs/core/SHIM_REMOVAL_CHECKLIST.md`**, **`scripts/migrate_phase_5e_drop_shims.sh`**.

## Validation (every phase)

- `./scripts/q3_openarena_compat_check.sh release` on **`core`**
- `./scripts/smoke_test.sh release` on **`game`**
- `./scripts/compile_engine.sh vulkan full` + `ctest`
- `ctest -L sector_stream` on **`game`**
- `./scripts/ci/audit_unconditional_sources.sh`

## Explicit non-goals

- Deleting neural/ML/research code
- Breaking QVM / retail mod loading
- Full `tr_local.h` C++ migration
- Making `core` the only long-term build (`game` is the 2026 default)
