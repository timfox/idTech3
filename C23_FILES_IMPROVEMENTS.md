# C23 Improvements for files.c

This document outlines opportunities to modernize `src/qcommon/files.c` using C23 standard features.

## 1. Standard Attributes (Replace GCC-specific)

### Current:
```c
static const unsigned pak_checksums[] __attribute__((unused)) = {
    // ...
};
```

### C23:
```c
static const unsigned pak_checksums[] [[maybe_unused]] = {
    // ...
};
```

**Benefits:**
- Standard C23 attribute syntax
- More readable
- Better compiler support

## 2. Type Inference with `typeof`

### Current:
```c
char *FS_BuildOSPath( const char *base, const char *game, const char *qpath ) {
    char	temp[MAX_OSPATH*2+1];
    static char ospath[2][sizeof(temp)+MAX_OSPATH];
    // ...
}
```

### C23:
```c
char *FS_BuildOSPath( const char *base, const char *game, const char *qpath ) {
    typeof(char[MAX_OSPATH*2+1]) temp;
    static typeof(temp) ospath[2][sizeof(temp)+MAX_OSPATH];
    // ...
}
```

**Benefits:**
- Better type safety
- Easier refactoring
- More maintainable

## 3. `[[nodiscard]]` for Critical Functions

### Current:
```c
fileHandle_t FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE );
```

### C23:
```c
[[nodiscard]] fileHandle_t FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE );
```

**Benefits:**
- Compiler warns if return value is ignored
- Prevents bugs from ignoring error codes

## 4. `[[fallthrough]]` in Switch Statements

### Current:
```c
switch( origin ) {
    case FS_SEEK_END:
        // ...
    case FS_SEEK_CUR:
        // ...
    case FS_SEEK_SET:
    default:
        // ...
}
```

### C23:
```c
switch( origin ) {
    case FS_SEEK_END:
        // ...
        [[fallthrough]];
    case FS_SEEK_CUR:
        // ...
        [[fallthrough]];
    case FS_SEEK_SET:
    default:
        // ...
}
```

**Benefits:**
- Explicit fallthrough intent
- Suppresses compiler warnings
- More readable

## 5. Better Null Handling

### Current:
```c
void *FS_LoadLibrary( const char *name )
{
    const searchpath_t *sp = fs_searchpaths;
    void *libHandle = NULL;
    // ...
    if ( !libHandle ) {
        return NULL;
    }
    return libHandle;
}
```

### C23:
```c
void *FS_LoadLibrary( const char *name )
{
    const searchpath_t *sp = fs_searchpaths;
    void *libHandle = nullptr;  // If C23 supports nullptr
    // Or use better null checks with typeof_unqual
    // ...
    if ( libHandle == nullptr ) {
        return nullptr;
    }
    return libHandle;
}
```

**Note:** C23 doesn't have `nullptr` like C++, but we can use better null handling patterns.

## 6. Type-Generic Macros with `_Generic`

### Current:
```c
// Manual type checking
if ( sp->pack ) {
    // handle pack
} else if ( sp->dir ) {
    // handle dir
}
```

### C23:
```c
// Could use _Generic for type-safe dispatch
#define FS_GetPathType(sp) _Generic((sp)->pack, \
    pack_t*: FS_PATH_PACK, \
    default: FS_PATH_DIR \
)
```

## 7. `if consteval` for Compile-Time Checks

### Current:
```c
#define MAX_ZPATH 256
// Runtime checks
if ( strlen( path ) >= MAX_ZPATH ) {
    Com_Error( ERR_DROP, "Path too long" );
}
```

### C23:
```c
#define MAX_ZPATH 256
// Compile-time validation where possible
if consteval {
    // Can validate at compile time
}
```

## 8. Better String Handling

### Current:
```c
strcpy( search->dir->path, curpath );
strcpy( search->dir->gamedir, pakdirs[ pakdirsi ] );
```

### C23:
```c
// Use strncpy_s or similar safer alternatives
// Or use typeof to ensure buffer sizes match
strncpy( search->dir->path, curpath, sizeof(search->dir->path) - 1 );
search->dir->path[sizeof(search->dir->path) - 1] = '\0';
```

## 9. `[[deprecated]]` for Legacy Functions

### Current:
```c
// Old function, should be deprecated
static void FS_OldFunction( void ) {
    // ...
}
```

### C23:
```c
[[deprecated("Use FS_NewFunction instead")]]
static void FS_OldFunction( void ) {
    // ...
}
```

## 10. Improved Error Handling with `typeof_unqual`

### Current:
```c
pack_t *pak;
pak = FS_LoadZipFile( pakfile );
if ( pak == NULL ) {
    // error handling
}
```

### C23:
```c
typeof_unqual(*pak) *pak = FS_LoadZipFile( pakfile );
if ( pak == nullptr ) {
    // error handling
}
```

## Implementation Priority

1. **High Priority:**
   - Replace `__attribute__((unused))` with `[[maybe_unused]]`
   - Add `[[nodiscard]]` to critical functions
   - Add `[[fallthrough]]` to switch statements

2. **Medium Priority:**
   - Use `typeof` for better type safety
   - Improve string handling safety
   - Add `[[deprecated]]` where appropriate

3. **Low Priority:**
   - Type-generic macros
   - `if consteval` optimizations
   - Advanced type inference

## Compatibility Notes

- C23 features require compiler support (GCC 13+, Clang 17+)
- Some features may need feature detection macros
- Consider using `#if __STDC_VERSION__ >= 202311L` guards

