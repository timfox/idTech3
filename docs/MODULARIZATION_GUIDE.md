# Modularization Guide

**Status:** Active engineering guide.  
**Scope:** How to split more engine code into domain folders without changing
runtime behavior or breaking legacy ABI surfaces.

See also:
[ENGINE_REORG_PLAN.md](ENGINE_REORG_PLAN.md),
[core/REPOSITORY_LAYOUT_2026.md](core/REPOSITORY_LAYOUT_2026.md),
[CPP23_MIGRATION.md](CPP23_MIGRATION.md).

## Goals

- Keep large source areas navigable by domain.
- Preserve C ABI, QVM, native module, renderer plugin, and mod compatibility.
- Make source ownership visible in CMake manifests.
- Keep moves reviewable and bisectable.

This is structural work. Avoid mixing a move with behavior changes unless the
behavior change is required to make the moved file compile.

## Canonical Roots

Use these roots before creating new top-level directories:

| Root | Purpose |
|------|---------|
| `engine/core/` | qcommon, VM, filesystem, networking, collision, core services |
| `engine/platform/` | OS/window/platform backends |
| `runtime/client/` | client runtime and shell/UI glue |
| `runtime/server/` | server runtime |
| `runtime/game/` | native game/runtime glue and ABI headers |
| `modules/` | optional or reusable engine subsystems |
| `extensions/` | profile-gated research/generative/experimental features |
| `renderers/` | renderer backends and renderer-shared code |
| `third_party/` | external code and submodules |

Do not revive `src/*` forwarding shims.

## Domain Folder Pattern

A good domain folder has:

- one clear responsibility
- a CMake source list or manifest entry
- no hidden source glob if the area is high-churn or profile-gated
- public headers kept at the stable ABI root when external code includes them
- a local README when the domain split is not obvious

Examples already in the tree:

- `runtime/client/core`, `world`, `media`, `platform`, `shell`
- `runtime/server/core`, `client`, `gameplay`, `net`, `services`, `world`
- `runtime/game/middleware`, `systems`, `scripting`, `ecs`
- `renderers/vulkan/extensions/{neural,splats,rtx,scaffold}`

## What Stays at the Root

Keep files at a domain root when they are stable public surface:

- `runtime/server/server.h`
- `runtime/server/sv_*.h` public/internal headers included by several domains
- `runtime/game/g_public.h`, `bg_public.h`, and `g_*.h`
- `runtime/client/client.h`, `keys.h`, `keycodes.h`
- renderer public import/export headers

Move implementation files first. Header moves should be separate unless all
include sites are local and tests are small.

## CMake Rules

Prefer explicit source lists for reorganized first-party domains.

Good:

```cmake
set(IDTECH3_SERVER_CORE_SRCS
	runtime/server/core/sv_main.c
	runtime/server/core/sv_init.c
)
```

Avoid adding new broad globs in modularized areas:

```cmake
file(GLOB SERVER_SRCS "runtime/server/*.c")
```

When moving files:

1. Update the owning manifest.
2. Update extension strip/append manifests if the feature is gated.
3. Update `REQUIRED_FILES` in CTest registrations.
4. Update source guard scripts.
5. Update docs that link to the moved files.

## Include Rules

Prefer flat includes resolved by target include directories:

```c
#include "q_shared.h"
#include "server.h"
#include "phys_bullet.h"
#include "world_config.h"
```

Avoid adding new bridge-relative includes:

```c
#include "../qcommon/q_shared.h"
#include "../physics/phys_bullet.h"
```

If a move requires many `../` edits, add the correct include directory to the
target and flatten the include names in the moved files.

## ABI Rules

Modularization must not change:

- exported C symbols
- VM trap numbers
- networked struct layout
- demo/save binary formats
- renderer plugin import/export structs
- native module load names and paths

If an ABI-facing header must change, make that a separate compatibility patch
with tests and docs.

## Suggested Next Splits

### Low Risk

- Add explicit domain manifests for `modules/audio` to replace inline client
  source lists in `CMakeLists.txt`.
- Move more client shell-only helpers under narrower `runtime/client/shell/*`
  subfolders if the file count keeps growing.
- Split docs by runtime domain when feature pages already point at moved files.

### Medium Risk

- Split `engine/core/common.c` into startup, cvar bootstrap, command-line, and
  frame/error pieces.
- Split `engine/core/files.c` into search paths, pk3, pure server, downloads,
  and native library loading.
- Split `renderers/vulkan/tr_init.c` after renderer certification checks are
  already green.

### High Risk

- Moving renderer backend globals from `tr_local.h`.
- Moving VM trap dispatch code.
- Changing `q_shared.h` / `qcommon.h` public surface.
- Moving networking snapshot encode/decode code.

High-risk splits should be done after a focused test is added first.

## Commit Shape

Good modularization commits are narrow:

- `Move server sources into runtime/server domains`
- `Add explicit server source manifest`
- `Move game Lua bindings into scripting domain`

Avoid:

- moving files and changing behavior
- moving files and renaming APIs
- moving files and reformatting unrelated code
- moving files across multiple major roots in one commit

## Validation Matrix

Minimum validation after a source move:

```bash
cmake -S . -B build-vk-Release
cmake --build build-vk-Release --target <focused_target> -j2
./tests/scripts/test_module_include_canonical.sh
```

Pick focused targets:

| Area | Build target | Extra checks |
|------|--------------|--------------|
| `runtime/server` | `idtech3_server`, `qcommon` | `test_p2p_*`, `test_stock_baseq3.sh`, `q3_openarena_compat_check.sh release` |
| `runtime/game` | `client`, `ecs_module` | `test_facs_facial.sh`, `test_engine_save.sh`, `test_physics.sh`, Lua feature guards |
| `runtime/client` | `client` | `test_client_modular.sh`, client runtime smoke where display/GPU permits |
| `modules/world` | `qcommon`, related unit tests | `test_openworld_*`, `test_nav_bake.sh`, `ctest -L sector_stream` |
| `renderers/vulkan` | renderer target / full build | renderer static checks plus GPU validation when available |

When a broad smoke script fails on unrelated pre-existing renderer contracts,
record that explicitly in the change summary rather than hiding the failure.

