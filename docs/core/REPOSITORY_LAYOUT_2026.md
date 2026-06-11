# Repository layout (2026 AAA target)

Evolutionary map from today's `src/` tree to the layercake folders in [BRANCHES.md](../BRANCHES.md). **No capability is removed** — extensions stay buildable via `IDTECH3_PROFILE=full` or `-DUSE_*=ON`.

## Current (shipping migration)

```
idtech3/
├── src/
│   ├── qcommon/              # engine core (vanilla)
│   ├── platform/             # SDL, unix, win32
│   ├── client/               # runtime client (modularizing)
│   ├── server/               # runtime server
│   ├── game/                 # VM traps + native hooks
│   ├── world/                # open-world module (USE_OPEN_WORLD)
│   ├── navigation/           # Recast module
│   ├── physics/              # Bullet middleware
│   ├── audio/                # codecs + backends
│   ├── renderers/vulkan/     # shipping + experimental (CMake manifests)
│   ├── extensions/
│   │   ├── generative/       # FLUX, TRELLIS, genetic GAN client
│   │   └── research/         # RadiusFPS, DaX, GCC-FER, …
│   └── external/             # vendored deps
├── third_party/              # symlink → src/external/ (2026 naming)
├── cmake/
│   ├── profiles/             # core | game | full | research
│   ├── client/               # ClientExtensionSources.cmake
│   ├── server/               # ServerExtensionSources.cmake
│   ├── renderers/            # VulkanCore/Extension manifests
│   └── IdTech3Extension.cmake
├── examples/                 # demo_game, templates
├── samples/                  # symlink → examples/ (2026 naming)
├── tools/
├── tests/
└── docs/                     # tiered index: docs/README.md
```

## Target (Phase 5+, after profiles stable)

```
engine/core/          ← qcommon core + vm + jobs
engine/platform/
runtime/client/       ← client/core, client/world, …
runtime/server/
runtime/game/
modules/world/        ← src/world/*
modules/audio/
modules/navigation/
modules/physics/
renderers/vulkan/core/
renderers/vulkan/extensions/
extensions/{generative,research,usd,studio}/
third_party/          ← src/external/
samples/              ← demo_game, templates
```

## Compatibility during migration

- CMake lists use **explicit extension manifests** (not bare `AUX_SOURCE_DIRECTORY` for optional code).
- Moved paths: one-release include shims — [DEPRECATION_POLICY.md](../DEPRECATION_POLICY.md).
- Authoritative source inventory: [ENGINE_MODULE_MANIFEST.md](../ENGINE_MODULE_MANIFEST.md).
- Rollout plan: [ENGINE_REORG_PLAN.md](../ENGINE_REORG_PLAN.md).
