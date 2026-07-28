# C++20 Migration Program

**Status:** Foundation milestone (Phases 1–2 + first leaf batch)  
**Language level:** C++20 only (`IDTECH3_CXX_STANDARD` / `-std=c++20`). Do not depend on C++23+ in this foundation track.  
**Related:** [CPP20_ABI_BOUNDARIES.md](CPP20_ABI_BOUNDARIES.md), [CPP20_CONVERSION_ORDER.md](CPP20_CONVERSION_ORDER.md), [CPP20_STYLE.md](CPP20_STYLE.md), [CPP23_MIGRATION.md](CPP23_MIGRATION.md)

## Goal

Compile progressively more of the existing engine as **C++20** while preserving:

- runtime behavior
- C ABI / mod / QVM / native DLL contracts
- renderer and networking stability
- deterministic gameplay
- bisectability (one conversion commit per file where practical)

This is a **compatibility migration**, not a rewrite.

The C++23 plan is documented separately in [CPP23_MIGRATION.md](CPP23_MIGRATION.md). Treat C++23 as the next staged language target; do not start using C++23-only library or language features in files governed by this C++20 program until the build system and CI matrix explicitly opt in.

## Non-goals (this program)

- Engine-wide redesign into class hierarchies
- Mixing language migration with renderer/physics/net/gameplay redesign
- Converting third-party code
- Requiring every public API to become C++
- Exceptions or RTTI as a dependency (defaults OFF)

## Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `USE_CPP20` | ON | Enable C++20 migration tooling, ABI guards TU, and converted leaves |
| `CPP20_EXCEPTIONS` | OFF | Allow C++ exceptions (`-fexceptions` / MSVC `/EHsc`) |
| `CPP20_RTTI` | OFF | Allow RTTI (`-frtti` / MSVC `/GR`) |
| `CPP20_STRICT` | ON | Extra pedantic warnings on C++ TUs |

CMake module: `cmake/IdTech3Cpp20.cmake`.

**Exceptions/RTTI:** applied only to first-party targets via `idtech3_cpp20_apply_target_flags()` (hooked from `idtech3_first_party_warnings_as_errors`). Do **not** set `-fno-exceptions` / `-fno-rtti` globally — FetchContent deps such as FreeUSD require exceptions.

## Inventory snapshot (foundation)

Approximate first-party counts under `engine/`, `runtime/`, `renderers/`, `modules/`, `tests/` (excludes most of `third_party/`):

| Category | C | C++ | Notes |
|----------|--:|---:|-------|
| engine/core (qcommon) | ~71 | growing | Leaf conversions start here |
| runtime/client | ~61 | ~5 | Keep C façades for modules |
| runtime/server | ~18 | 0 | High ABI sensitivity |
| renderers/vulkan | ~209 | ~15 | Convert support only first |
| renderers/common | ~23 | ~2 | |
| modules (game/world/…) | ~98 | ~12 | Native game ABI stays C |
| tests | ~76 | ~1 | Convert tests early |

Classification labels (see conversion order doc):

- `CPP_READY`
- `CPP_WITH_MECHANICAL_FIXES`
- `CPP_BLOCKED_BY_ABI`
- `CPP_BLOCKED_BY_C_ONLY_DEPENDENCY`
- `KEEP_C_EXTERNAL_BOUNDARY`
- `THIRD_PARTY_DO_NOT_CONVERT`

Full per-file TSV: [`docs/cpp20_inventory.tsv`](cpp20_inventory.tsv) (seed + first batch; grow incrementally).

## Status command

Console: `cpp20_status`  
Shell: `./scripts/cpp20_status.sh`

Prints totals, converted/blocked counts, ABI boundary count, and option flags.

## Commit categories

| Prefix | Contents |
|--------|----------|
| `CPP20_BUILD` | CMake / toolchain / options |
| `CPP20_HEADER_COMPAT` | Shared header guards / keyword fixes |
| `CPP20_FILE_CONVERSION` | One leaf `.c` → `.cpp` mechanical |
| `CPP20_ABI_GUARD` | sizeof/offsetof asserts, symbol tests |
| `CPP20_LOCAL_RAII` | Separate ownership cleanup (later) |
| `CPP20_INTERNAL_REFACTOR` | Behavior-preserving internal cleanup after conversion proved |

## First leaf batch (this milestone)

Mechanically converted (C ABI preserved via `extern "C"`):

1. `engine/core/md4.cpp` — `Com_BlockChecksum`
2. `engine/core/md5.cpp` — MD5 helpers
3. `engine/core/huffman_static.cpp` — static Huffman tables
4. `engine/core/q_utf8.cpp` — UTF-8 decode/encode
5. `renderers/vulkan/vk_cluster_math.cpp` — cluster Z math CPU reference

## Tests

- `tests/scripts/test_cpp20_build.sh`
- `tests/scripts/test_cpp20_abi.sh`
- `tests/scripts/test_cpp20_symbols.sh`
- `tests/scripts/test_cpp20_headers.sh`
- existing `tests/scripts/test_cpp20_sources.sh` (prior world/.cpp migrations)

## Definition of done (foundation)

- [x] Mixed C/C++20 builds on primary Linux config
- [x] `USE_CPP20` / exceptions / RTTI / strict options
- [x] `cpp20_compat.h` + ABI guards
- [x] Dual C/C++ header compile tests
- [x] `cpp20_status`
- [x] ≥3 low-risk leaves as C++20
- [x] No intentional gameplay/renderer/net redesign in this milestone

## Next batch (recommended)

1. `engine/core/cm_bounds.c` (already unit-tested)
2. `engine/core/huffman.c` (after static tables proven)
3. `runtime/client/core/cl_compat_math.c`
4. Isolated parser helpers in `q_shared.c` **only after** splitting the TU (do not convert the whole file yet)
