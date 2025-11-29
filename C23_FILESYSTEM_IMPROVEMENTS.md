# C23 Filesystem Improvements

This document outlines the C23 standard improvements applied to `src/qcommon/files.c` to modernize the codebase and improve code quality, safety, and maintainability.

## ✅ Implemented Improvements

### 1. Standard Attributes

#### `[[maybe_unused]]` Attribute
- **Replaced**: `__attribute__((unused))` with standard C23 `[[maybe_unused]]`
- **Location**: `pak_checksums` array (line 205)
- **Benefits**: 
  - Standard C23 syntax
  - Better portability
  - More readable

```c
// Before:
static const unsigned pak_checksums[] __attribute__((unused)) = { ... };

// After (C23):
#if __STDC_VERSION__ >= 202311L
static const unsigned pak_checksums[] [[maybe_unused]] = { ... };
#elif defined(__GNUC__) || defined(__clang__)
static const unsigned pak_checksums[] __attribute__((unused)) = { ... };
#else
static const unsigned pak_checksums[] = { ... };
#endif
```

#### `[[nodiscard]]` Attribute
- **Applied to**: Critical filesystem functions that return error codes or handles
- **Functions**:
  - `FS_FOpenFileRead()` - Returns file handle or -1 on error
  - `FS_FOpenFileWrite()` - Returns file handle or FS_INVALID_HANDLE
  - `FS_FOpenFileAppend()` - Returns file handle or FS_INVALID_HANDLE
  - `FS_Home_FOpenFileRead()` - Returns file size or -1 on error
  - `FS_Read()` - Returns bytes read (may be less than requested)
  - `FS_Write()` - Returns bytes written (may be less than requested)
  - `FS_Seek()` - Returns 0 on success, -1 on error
  - `FS_FileForHandle()` - Returns FILE pointer (must not be NULL)

- **Benefits**:
  - Compiler warns when return values are ignored
  - Prevents bugs from ignoring error codes
  - Forces explicit handling of return values

```c
// Example:
[[nodiscard]] int FS_Read( void *buffer, int len, fileHandle_t f ) {
    // ... implementation
}

// Usage now requires explicit handling:
int bytes = FS_Read(buffer, len, f);  // ✓ Good
FS_Read(buffer, len, f);              // ⚠ Warning: ignoring return value
(void)FS_Read(buffer, len, f);        // ✓ Explicitly ignoring (for logging, etc.)
```

#### `[[fallthrough]]` Attribute
- **Location**: `FS_Seek()` function (line 2322)
- **Purpose**: Explicitly mark intentional fallthrough in switch statements
- **Benefits**: Suppresses compiler warnings, makes intent clear

```c
switch( origin ) {
    case FS_SEEK_SET:
        // ... setup code
        [[fallthrough]];  // Explicitly continue to next case
    case FS_SEEK_END:
    case FS_SEEK_CUR:
        // ... common code
        break;
}
```

### 2. Compile-Time Safety

#### Static Assertions (`_Static_assert`)
- **MAX_FILE_HANDLES validation**: Ensures handle limit is reasonable (1-4096)
- **Cache size validation**: Ensures cache sizes are powers of 2 for efficient hashing
- **Benefits**:
  - Catches configuration errors at compile-time
  - Prevents runtime bugs from invalid sizes
  - Documents requirements clearly

```c
// Validate MAX_FILE_HANDLES
_Static_assert( MAX_FILE_HANDLES > 0 && MAX_FILE_HANDLES <= 4096, 
    "MAX_FILE_HANDLES must be between 1 and 4096" );

// Validate cache sizes are powers of 2
_Static_assert( (FS_PATH_CACHE_SIZE_DEFAULT & (FS_PATH_CACHE_SIZE_DEFAULT - 1)) == 0,
    "FS_PATH_CACHE_SIZE_DEFAULT must be a power of 2 for efficient hashing" );
```

### 3. Error Handling Improvements

#### Explicit Return Value Handling
- **Added**: Explicit `(void)` casts for intentionally ignored return values
- **Locations**: 
  - Logging functions (`FS_Write()` in `FS_Printf()`)
  - PK3 seeking operations (`FS_Read()` in `FS_Seek()`)
  - Journal file operations
- **Benefits**:
  - Makes intent explicit
  - Suppresses `[[nodiscard]]` warnings where appropriate
  - Documents that ignoring return value is intentional

```c
// Example: Logging function - errors are non-critical
(void)FS_Write(msg, strlen(msg), h);  // Explicitly ignoring return value

// Example: PK3 seeking - we're just advancing position
(void)FS_Read(buffer, PK3_SEEK_BUFFER_SIZE, f);  // Position advancement, not data reading
```

### 4. Type Safety

#### Better Const Correctness
- **Improved**: Function parameters marked `const` where appropriate
- **Cache functions**: Use `const` pointers for read-only access
- **Benefits**: 
  - Prevents accidental modifications
  - Better compiler optimizations
  - Clearer function contracts

## 📊 Summary of Changes

### Attributes Added
- **8 functions** with `[[nodiscard]]` attribute
- **1 array** with `[[maybe_unused]]` attribute
- **1 switch** with `[[fallthrough]]` attribute

### Static Assertions Added
- **3 compile-time checks** for configuration validation

### Error Handling
- **10+ locations** with explicit return value handling

## 🚀 Additional C23 Opportunities

### Future Improvements

1. **Type-Generic Macros (`_Generic`)**
   - Create type-safe wrappers for file operations
   - Better error handling based on return types

2. **`if consteval` for Compile-Time Optimizations**
   - Optimize constant expressions at compile-time
   - Reduce runtime overhead for known values

3. **Better String Handling**
   - Use bounds-checked string functions where available
   - Improve buffer overflow protection

4. **`[[deprecated]]` Attributes**
   - Mark legacy functions for removal
   - Guide developers to newer APIs

5. **More Constexpr Optimizations**
   - Mark constant expressions appropriately
   - Enable more compile-time evaluation

6. **`typeof` and `typeof_unqual`**
   - Better type inference in complex scenarios
   - Reduce code duplication

## 🔧 Compiler Requirements

- **C23 Support**: GCC 13+, Clang 17+ (for full C23 features)
- **Fallback**: GCC 4.0+, Clang 3.0+ (for `__attribute__` extensions)
- **CMake**: Already configured to use C23 standard

## 📝 Files Modified

- `src/qcommon/files.c` - Main filesystem implementation
- `src/qcommon/qcommon.h` - Header definitions

## ✅ Testing

All changes compile successfully with:
- No compilation errors
- No linter errors
- Warnings properly handled with explicit casts
- Backward compatibility maintained

## 🎯 Benefits

1. **Better Code Quality**: Standard attributes improve readability
2. **Safer Code**: `[[nodiscard]]` prevents ignored error codes
3. **Compile-Time Safety**: Static assertions catch errors early
4. **Modern Standards**: Using C23 features keeps codebase current
5. **Better Maintainability**: Clearer intent and better documentation

