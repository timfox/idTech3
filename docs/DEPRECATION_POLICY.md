# One-release shim policy for engine reorganization moves.

When sources move under `src/extensions/` or renderer subdirs:

1. **CMake** — update manifest macros first; avoid duplicate unconditional `list(APPEND ...)`.
2. **Includes** — prefer centralized `target_include_directories` over path-relative `#include "../..."` when practical.
3. **Docs/tests** — update `docs/ENGINE_MODULE_MANIFEST.md` and grep-based script tests in the same PR.
4. **Optional stub** — a `README` at the old path may point to the new location for one release cycle.
5. **Removal** — delete shims only after two weeks on `main` with no downstream references.

**2026 layout (Phase 5c — canonical physical roots):**

- `engine/{core,platform}/` — was `src/qcommon`, `src/platform`
- `runtime/{client,server,game}/` — was `src/client`, `src/server`, `src/game`
- `modules/{world,navigation,physics,audio}/` — was matching `src/*`
- `extensions/`, `renderers/`, `third_party/` — was `src/extensions`, `src/renderers`, `src/external`
- `samples/` → `examples/` (alias unchanged)

**One-release forwarding shims:** `src/qcommon` → `../engine/core`, etc. Remove after two weeks on `main` with no downstream references (target Phase 5d).

CMake path variables: `cmake/IdTech3Layout.cmake` (`IDTECH3_DIR_*`). Test: `test_repository_layout_2026.sh`.

**Phase 5d (MSVC):** manifest export + `scripts/msvc/sync_all_vcxproj.sh` keep `quake3e` / `quake3e-ded` / `botlib` / `vulkan` vcxproj aligned with CMake. `renderer2.vcxproj` / `opengl.vcxproj` deprecated (Vulkan-only shipping). CI: `test_msvc_manifest_drift`.

**Deferred (not yet scheduled):**

- Drop remaining **layout bridge** symlinks (`runtime/qcommon`, `engine/platform/client`, …) after relative `#include` rewrite
- Full vcxproj regeneration (replace hand-maintained lists entirely)

**Completed:** Phase 5e dropped `src/*` one-release forwarding shims (`migrate_phase_5e_drop_shims.sh`).

See `docs/ENGINE_MODULE_MANIFEST.md` and `docs/ROADMAP.md`.
