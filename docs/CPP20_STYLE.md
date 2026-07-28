# C++20 Style (restrained)

**Status:** Foundation  
**Applies to:** Newly converted internal C++20 TUs

C++23-specific guidance lives in [CPP23_MIGRATION.md](CPP23_MIGRATION.md). Until `IDTECH3_CXX_STANDARD` is raised by CMake for a target, code in that target must stay within the C++20 subset described here.

## Allowed early

- `nullptr`
- anonymous namespace for file-local helpers
- `constexpr` constants
- `enum class` for **new internal-only** enums
- `std::span` / `std::array` at internal boundaries
- local RAII (later milestone; destructors `noexcept`)
- `static_assert`, `[[nodiscard]]`, `override` / `final`
- explicit constructors; deleted copy where ownership requires it

## Avoid early

- Broad class hierarchies / pervasive templates  
- Exceptions / RTTI (defaults OFF)  
- iostreams  
- Replacing every array with `std::vector` or string with `std::string`  
- Coroutines / ranges in hot legacy loops  
- Sweeping ownership or public-struct→class changes  

## Preserve engine conventions

- Zone / hunk / temp allocators remain authoritative  
- Cvars, commands, existing logging  
- Fixed-size network structures  
- Deterministic math  
- Platform abstraction  

## Allocation

No unrestricted global `new`/`delete` in engine code.  
STL containers require an explicit allocator strategy before broad adoption.  
Frame-critical paths must not hide allocations.

## Naming

- New internal types may use clear C++ naming  
- Do not mass-rename legacy APIs  
- `qboolean` remains at ABI boundaries; `bool` OK internally after conversion  

## Warnings

Prefer fixing warnings over pragmas.  
`CPP20_STRICT` enables a stronger C++ warning set on converted TUs; expand gradually per milestone budget.
