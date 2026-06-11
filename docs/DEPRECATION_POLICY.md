# One-release shim policy for engine reorganization moves.

When sources move under `src/extensions/` or renderer subdirs:

1. **CMake** — update manifest macros first; avoid duplicate unconditional `list(APPEND ...)`.
2. **Includes** — prefer centralized `target_include_directories` over path-relative `#include "../..."` when practical.
3. **Docs/tests** — update `docs/ENGINE_MODULE_MANIFEST.md` and grep-based script tests in the same PR.
4. **Optional stub** — a `README` at the old path may point to the new location for one release cycle.
5. **Removal** — delete shims only after two weeks on `main` with no downstream references.

**Deferred (not yet scheduled):**

- Top-level `engine/`, `runtime/`, `modules/` physical roots
- `src/external/` → `third_party/` rename
- MSVC project codegen from CMake (replacing hand-maintained vcxproj lists)

See `docs/ENGINE_MODULE_MANIFEST.md` and `docs/ROADMAP.md`.
