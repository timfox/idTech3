# Repository layout (2026 AAA target)

Evolutionary map from today's `src/` tree to the layercake folders in [BRANCHES.md](../BRANCHES.md). **No capability is removed** — extensions stay buildable via `IDTECH3_PROFILE=full` or `-DUSE_*=ON`.

## Current (Phase 5c — physical layout)

```
idtech3/
├── engine/
│   ├── core/                 # was src/qcommon (vanilla)
│   └── platform/             # SDL, unix, win32
├── runtime/
│   ├── client/               # core/world/media/platform
│   ├── server/
│   └── game/
├── modules/
│   ├── world/                # open-world, Arc Blanc, fog biology, …
│   ├── navigation/
│   ├── physics/
│   └── audio/
├── extensions/
│   ├── generative/
│   └── research/
├── renderers/vulkan/         # core + extensions/{neural,splats,rtx,scaffold}
├── third_party/              # was src/external
├── samples/                  # symlink → examples
├── src/                      # forwarding shims only (Phase 5c complete)
│   ├── qcommon → ../engine/core
│   ├── client → ../runtime/client
│   ├── botlib → ../modules/botlib
│   └── …                     # see scripts/migrate_phase_5c.sh
├── cmake/
│   ├── IdTech3Layout.cmake   # IDTECH3_DIR_* path aliases
│   ├── profiles/             # core | game | full | research
│   ├── client/               # ClientExtensionSources.cmake
│   ├── server/               # ServerExtensionSources.cmake
│   ├── renderers/            # VulkanCore/Extension manifests
│   └── IdTech3Extension.cmake
├── examples/                 # demo_game, templates (also samples/)
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

- CMake extension manifests use **`IDTECH3_DIR_*`** from [IdTech3Layout.cmake](../../cmake/IdTech3Layout.cmake); physical roots are **`engine/`**, **`runtime/`**, **`modules/`**, etc. (Phase 5c). Cross-domain `#include` paths and MSVC `vcxproj` relative paths use **`scripts/layout_forwarding_symlinks.sh`** (bridge symlinks under `engine/platform/` and `engine/`) until Phase 5d — see [MSVC_CODEGEN.md](../MSVC_CODEGEN.md).
- Moved paths: one-release include shims — [DEPRECATION_POLICY.md](../DEPRECATION_POLICY.md).
- Authoritative source inventory: [ENGINE_MODULE_MANIFEST.md](../ENGINE_MODULE_MANIFEST.md).
- Rollout plan: [ENGINE_REORG_PLAN.md](../ENGINE_REORG_PLAN.md).
