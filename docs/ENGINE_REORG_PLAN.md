# Engine diet & reorganization plan (capability-preserving)

**North star:** shrink what everyone pays for by default; keep every capability reachable via **extensions + build profiles**.

| Principle | Rule |
|-----------|------|
| No capability loss | `full` / `research` profile or `-DUSE_*=ON` |
| No mod break | QVM, pk3, traps, cvars stable; one-release shims |
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
| 5 | `engine/` + `runtime/` + `modules/` rename | Deferred |
| 5 | `third_party/` rename | Deferred |
| 5 | MSVC codegen from CMake | Deferred |

## Next high-value PRs (structure-first)

1. **Runtime client folders** — `runtime/client/{core,world,generative}` physical moves with CMake aliases.
2. **`USE_GAME_AI_MIDDLEWARE`** — gate `g_director`, `g_goap`, … (OFF in `core`); stub headers for `g_lua_bindings.c`.
3. **`renderers/vulkan/extensions/`** physical neural/RTX moves.
4. **`third_party/`** CMake alias over `src/external/`.
5. **Docs archive** — merge FBO audit cluster into `docs/archive/fbo_investigation.md`.

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
