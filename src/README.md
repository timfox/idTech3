# `src/` (removed forwarding shims)

Phase 5e dropped the one-release `src/*` symlinks that pointed at the 2026
canonical roots. Use these paths instead:

| Old shim | Canonical |
|----------|-----------|
| `src/qcommon` | `engine/core` |
| `src/platform` | `engine/platform` |
| `src/client` | `runtime/client` |
| `src/server` | `runtime/server` |
| `src/game` | `runtime/game` |
| `src/cgame` | `runtime/cgame` |
| `src/ui` | `runtime/ui` |
| `src/world` | `modules/world` |
| `src/navigation` | `modules/navigation` |
| `src/physics` | `modules/physics` |
| `src/audio` | `modules/audio` |
| `src/botlib` | `modules/botlib` |
| `src/asm` | `engine/asm` |
| `src/extensions` | `extensions` |
| `src/renderers` | `renderers` |
| `src/external` | `third_party` |

Cross-domain layout bridges (`runtime/qcommon`, `engine/platform/client`, …)
remain for relative `#include` and MSVC path resolution — see
`scripts/layout_forwarding_symlinks.sh` and `docs/core/SHIM_REMOVAL_CHECKLIST.md`.
