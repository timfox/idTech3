# C++23 Migration Roadmap

**Status:** Planning overlay on top of the C++20 foundation.  
**Current build reality:** first-party C++ targets still select `IDTECH3_CXX_STANDARD` from CMake, currently preferring C++20 and falling back to C++17. This document describes how to move the codebase from mixed C/C++20 toward C++23 without breaking QVMs, native modules, renderer plugins, or legacy mods.

See also: [CPP20_MIGRATION.md](CPP20_MIGRATION.md), [CPP20_ABI_BOUNDARIES.md](CPP20_ABI_BOUNDARIES.md), [CPP20_STYLE.md](CPP20_STYLE.md), [CPP20_CONVERSION_ORDER.md](CPP20_CONVERSION_ORDER.md).

## Goal

Use C++23 as the implementation language for new engine-owned subsystems and progressively converted internal files, while keeping the public engine contract stable:

- C ABI for VM/native module/renderer plugin boundaries
- fixed network, demo, and save formats
- deterministic server/game simulation
- existing allocator and error-handling policies
- clean bisection, with language conversion separate from behavior changes

This is still a compatibility migration. It is not permission to redesign the engine around broad inheritance, exceptions, RTTI, or STL-heavy ownership in hot paths.

## Why C++23

C++23 is useful here mostly because it gives better compile-time and library vocabulary for engine internals:

- clearer internal initialization with `constexpr` and `consteval` helpers
- `std::span`, `std::array`, and ranges-style helpers in non-hot or carefully measured code
- `std::expected` for internal parse/load results where exceptions remain disabled
- `std::string_view` for read-only parsing surfaces that do not own memory
- safer enum and type wrappers behind C ABI facades

These benefits matter most in new module code (`modules/rts`, world, navigation, physics adapters, asset import, tools) and low-level leaf helpers. They matter least at ABI and packet boundaries, where C layouts remain the source of truth.

## Migration Stages

### Stage 0: C++20 Foundation Stays Green

Before raising the language level, keep the existing C++20 program healthy:

- `ctest -R test_cpp20 --output-on-failure`
- `./tests/scripts/test_cpp20_headers.sh`
- `./tests/scripts/test_cpp20_symbols.sh`
- `./tests/scripts/test_cpp20_sources.sh`
- `./scripts/cpp20_status.sh`

Do not begin a C++23 flip while C++20 converted leaves are failing, ABI guards are weakened, or public headers do not dual-compile.

### Stage 1: Add C++23 Toolchain Detection

Introduce a separate CMake switch before changing defaults:

```cmake
option(USE_CPP23 "Prefer C++23 for first-party C++ engine targets" OFF)
```

Then choose the standard explicitly:

```cmake
if(USE_CPP23 AND "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  set(IDTECH3_CXX_STANDARD 23)
elseif("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  set(IDTECH3_CXX_STANDARD 20)
else()
  set(IDTECH3_CXX_STANDARD 17)
endif()
```

Keep `USE_CPP20` as the compatibility/migration umbrella until every script and doc is renamed or generalized. A later cleanup can rename it to `USE_CPP_MIGRATION` or similar.

### Stage 2: Compile New Modules as C++23 First

Start with targets that already expose small C-compatible surfaces:

- `rts_module`
- `recast_nav`
- world/district/open-world helpers
- isolated renderer math/support helpers
- asset import/conversion tools

Do not convert `q_shared.h`, `qcommon.h`, VM trap tables, renderer exports, or game ABI records just to use C++23 syntax.

### Stage 3: Convert Leaf Files One at a Time

Good first C to C++23 candidates:

- pure math helpers with unit tests
- format parsers after binary-layout assertions exist
- CRC/hash/compression helpers with golden tests
- asset import helpers that already avoid VM ABI
- renderer CPU-side utility code that does not export plugin ABI directly

Poor early candidates:

- `q_shared.c` as a whole
- networking snapshot encode/decode
- VM loader/trap dispatch
- filesystem search path core
- SDL/platform entry points
- renderer plugin import/export structs

Each conversion commit should do one of these, not all three:

- rename `.c` to `.cpp` and fix mechanical C++ compile issues
- add internal RAII/type wrappers behind an unchanged C facade
- change behavior

Mixing those makes regressions much harder to bisect.

### Stage 4: Raise Default After CI Proves It

Only make C++23 the default after these are true on Linux, Windows/MSVC, and any supported cross-builds:

- all current C++20 migration tests pass under C++23
- public headers still compile as both C and C++
- ABI guard sizes and exported symbols are unchanged
- first-party targets do not require exceptions or RTTI
- third-party dependencies are not forced into engine flags
- build profiles `core`, `game`, `full`, and `research` configure cleanly

The default flip should be a small commit with no source conversions.

## ABI Rules

Everything in [CPP20_ABI_BOUNDARIES.md](CPP20_ABI_BOUNDARIES.md) continues to apply under C++23.

Keep these surfaces C-shaped:

- VM and QVM trap interfaces
- native game DLL exports
- renderer `refimport_t` / `refexport_t`
- networked structs and configstrings
- save/demo binary layouts
- Cvars, console commands, and platform callbacks

Internal C++23 can sit behind those boundaries, but boundary headers must remain consumable from C. Prefer this shape:

```c
#ifdef __cplusplus
extern "C" {
#endif

void RTS_Init( void );
void RTS_Shutdown( void );
void RTS_RunTurn( int msec );

#ifdef __cplusplus
}
#endif
```

Then put C++ implementation details in private headers or `.cpp` files:

```cpp
namespace rts {
struct State;
void ApplyQueuedCommands(State &state, int msec);
}
```

## C++23 Feature Policy

### Allowed Early

- `nullptr`, `constexpr`, `consteval` for internal compile-time helpers
- `std::span` for non-owning contiguous data views
- `std::array` for fixed-size internal tables
- `std::string_view` for read-only parser inputs
- `std::expected` for internal parse/load errors when available
- `enum class` for new internal-only enums
- `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`
- local RAII wrappers for handles that already have clear acquire/release pairs

### Use With Care

- ranges: fine for tools and cold code; avoid in measured frame/server hot loops until profiled
- `std::vector`: only with an explicit allocation story; never silently in per-frame paths
- `std::unordered_map`: avoid in deterministic simulation unless hashing/order are controlled
- concepts/templates: useful for local helpers, not broad engine architecture
- modules: do not use yet; build tooling and compiler support are not ready across this repo

### Still Avoid

- exceptions as control flow
- RTTI-dependent dispatch
- iostreams in engine/runtime code
- global constructors with meaningful side effects
- `std::thread`/async abstractions that bypass engine job/thread ownership
- public C++ classes in mod/plugin ABI

## Ownership and Allocation

The engine allocators remain authoritative:

- hunk for long-lived level/session allocations
- zone for general engine allocations
- temp/frame allocators where already used
- renderer/device allocators for GPU resources

C++ RAII is welcome when it wraps an existing engine ownership rule. It should not silently switch ownership to unrestricted global `new`/`delete`.

Good:

```cpp
class FileHandle {
public:
	explicit FileHandle(fileHandle_t handle) noexcept : handle_(handle) {}
	~FileHandle() noexcept { if (handle_ != FS_INVALID_HANDLE) FS_FCloseFile(handle_); }
	FileHandle(const FileHandle &) = delete;
	FileHandle &operator=(const FileHandle &) = delete;

private:
	fileHandle_t handle_;
};
```

Bad:

```cpp
static std::vector<Thing> perFrameThings; // hidden lifetime + allocation policy
```

## Error Handling

Keep existing fatal/error paths at engine boundaries:

- `Com_Error` for fatal/drop errors where the engine already expects longjmp-style unwind
- `Com_Printf` / status structs for diagnostics
- integer or enum result codes across C APIs

Inside C++23 implementation code, prefer small explicit result types over exceptions:

```cpp
struct ParseResult {
	bool ok;
	int errorLine;
};
```

When the toolchain/library baseline supports it, `std::expected<T, E>` may be used in internal-only code. Do not expose it in public C headers.

## Determinism

Server/gameplay and RTS-style simulation code must remain deterministic:

- no unordered iteration in authoritative sim paths unless ordering is normalized
- no wall-clock reads inside deterministic turns
- no floating-point mode surprises across platforms
- no hidden allocation failure behavior inside core simulation
- no background mutation that races the main game frame

For new C++23 simulation modules, use plain data plus explicit turn/frame entry points. `modules/rts` is the intended pattern: C ABI outside, namespaced implementation inside.

## Conversion Checklist

For each `.c` to `.cpp` conversion:

1. Confirm the file is listed or classified in `docs/cpp20_inventory.tsv` or its C++23 successor.
2. Identify exported symbols and wrap them with `extern "C"`.
3. Build before behavior changes.
4. Add or extend ABI/static assertions for any public structs touched.
5. Add focused tests or source guards.
6. Keep the commit message scoped to language conversion.
7. Run:

```bash
cmake -S . -B build-vk-Release
cmake --build build-vk-Release --target <focused_target>
ctest --test-dir build-vk-Release -R 'test_cpp20|unit_<area>' --output-on-failure
```

## Documentation Updates Per Milestone

When C++23 work lands, update:

- this file
- [CPP20_MIGRATION.md](CPP20_MIGRATION.md) if the C++20 baseline changes
- [CPP20_CONVERSION_ORDER.md](CPP20_CONVERSION_ORDER.md) or a future `CPP23_CONVERSION_ORDER.md`
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) for compiler requirements
- `CLAUDE.md` / `AGENTS.md` only when build commands or repo policy change

## Open Questions

- Should `USE_CPP20` be renamed before the C++23 flip, or kept as a historical umbrella?
- Which MSVC version is the minimum acceptable C++23 baseline?
- Do we want a custom STL allocator bridge before allowing broader containers?
- Should converted renderer helper TUs use C++23 before core engine TUs?
- What is the first authoritative sim path that should prove deterministic C++23 containers?

