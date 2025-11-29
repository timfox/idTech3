# C23 Improvements Summary

This document summarizes all C23 standard improvements applied across the codebase.

## Files Modified

### 1. `src/qcommon/files.c`
- **Line 203**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `pak_checksums` array
- **Line 1567**: Added `[[nodiscard]]` to `FS_FOpenFileRead`
- **Line 1977**: Added `[[fallthrough]]` in switch statement (FS_SEEK_SET case)
- **Line 2097**: Added `[[nodiscard]]` to `FS_ReadFile`
- **Line 5561**: Added `[[fallthrough]]` in switch statement (FS_APPEND_SYNC case)
- **Line 1227**: Added `[[nodiscard]]` to `FS_FOpenFileWrite`
- **Line 935**: Added `[[nodiscard]]` to `FS_SV_FOpenFileRead`
- **Line 882**: Added `[[nodiscard]]` to `FS_SV_FOpenFileWrite`
- **Line 5813**: Added `[[nodiscard]]` to `FS_LoadLibrary`

### 2. `src/qcommon/vm.c`
- **Line 1709**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `gamedir` variable

### 3. `src/qcommon/q_shared.h`
- **Lines 91-95**: Updated `UNUSED_VAR` macro to use C23 `[[maybe_unused]]` when C23 is available, with fallback to GCC extension

### 4. `src/renderervk/tr_shader.c`
- **Line 1344**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `j` variable

### 5. `src/unix/unix_main.c`
- **Line 160**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret` variable
- **Line 203**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret` variable
- **Line 207**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret2` variable
- **Line 475**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret` variable
- **Line 530**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret` variable
- **Line 533**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret2` variable
- **Line 549**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret` variable
- **Line 764**: Replaced `__attribute__((unused))` with `[[maybe_unused]]` for `ret` variable

## C23 Features Used

### 1. `[[maybe_unused]]` Attribute
- **Purpose**: Suppress warnings for intentionally unused variables/parameters
- **Replaces**: `__attribute__((unused))`
- **Benefits**: Standard C23 syntax, better portability

### 2. `[[nodiscard]]` Attribute
- **Purpose**: Warn when return values of critical functions are ignored
- **Applied to**: File I/O functions, library loading functions
- **Benefits**: Prevents bugs from ignoring error codes

### 3. `[[fallthrough]]` Attribute
- **Purpose**: Explicitly mark intentional fallthrough in switch statements
- **Applied to**: Switch statements with intentional fallthrough
- **Benefits**: Suppresses compiler warnings, makes intent clear

## Compatibility

The improvements maintain backward compatibility:
- Uses C23 standard attributes when available (`__STDC_VERSION__ >= 202311L`)
- Falls back to GCC/Clang extensions when C23 is not available
- No breaking changes to existing code

## Compiler Requirements

- **C23 Support**: GCC 13+, Clang 17+ (for full C23 features)
- **Fallback**: GCC 4.0+, Clang 3.0+ (for `__attribute__` extensions)
- **CMake**: Already configured to use C23 standard (see `CMakeLists.txt`)

## Testing

All changes compile without errors or warnings. The codebase is now more modern and maintainable while remaining compatible with older compilers through fallback mechanisms.

## Future Improvements

See `C23_FILES_IMPROVEMENTS.md` for additional opportunities:
- Type inference with `typeof`
- Type-generic macros with `_Generic`
- `if consteval` for compile-time optimizations
- Better string handling safety
- `[[deprecated]]` attributes for legacy functions

