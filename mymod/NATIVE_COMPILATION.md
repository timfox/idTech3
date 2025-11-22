# Native C Compilation Guide

This guide explains how to compile your game modules as native shared libraries instead of QVM bytecode, allowing you to use modern C features while maintaining QVM compatibility.

## Why Native Compilation?

The QVM system uses an old C compiler (LCC) with limited features:
- No C11/C17 support
- Limited standard library
- No inline functions
- Restricted memory management
- Older C89/C99 features only

Native compilation allows you to:
- Use **C11/C17** features
- Access full **standard library**
- Use **inline functions** for performance
- Better **debugging** with modern tools
- **Faster execution** (no VM overhead)
- **Easier development** with modern IDEs

## How It Works

The engine supports both QVM and native libraries:

1. **Native libraries** are tried first (if `vm_game`, `vm_cgame`, `vm_ui` = 0)
2. If native loading fails, it falls back to **QVM files**
3. This means you can have both - native for your mod, QVM for compatibility

## File Naming

Native libraries must follow this naming convention:

- **Linux**: `game.x86_64.so`, `cgame.i386.so`, `ui.arm64.so`
- **Windows**: `game.x64.dll`, `cgame.x86.dll`, `ui.x64.dll`

Format: `{module_name}.{architecture}.{extension}`

Architecture strings:
- Linux: `x86_64`, `i386`, `arm64`, `arm`
- Windows: `x64`, `x86`, `arm64`

## Required Exports

Your native library must export two functions:

### 1. dllEntry()
```c
void dllEntry( dllSyscall_t syscallptr );
```
Called when the module loads. Store `syscallptr` for making system calls.

### 2. vmMain()
```c
intptr_t vmMain( int command, int arg0, int arg1, int arg2 );
```
Main entry point. Called by the engine with various commands.

## Building with CMake

1. **Create build directory**:
   ```bash
   cd scripts
   mkdir build && cd build
   ```

2. **Configure**:
   ```bash
   cmake ..
   ```
   Adjust `ENGINE_ROOT` in CMakeLists.txt to point to your engine source.

3. **Build**:
   ```bash
   make
   ```

4. **Output**: Libraries will be in `../vm/` directory.

## Building with Makefile

1. **Edit Makefile**: Adjust `ENGINE_ROOT` to point to your engine source.

2. **Build**:
   ```bash
   cd scripts
   make all          # Build all modules
   make game         # Build only game module
   make cgame        # Build only cgame module
   make ui           # Build only UI module
   ```

3. **Output**: Libraries will be in `../vm/` directory.

## Enabling Native Loading

Set these CVARs to enable native library loading:

```
set vm_game 0    # 0 = native, 1 = bytecode, 2 = compiled
set vm_cgame 0   # Same for cgame
set vm_ui 0      # Same for UI
```

Or add to your config file:
```
set vm_game "0"
set vm_cgame "0"
set vm_ui "0"
```

## Directory Structure

```
mymod/
├── gamesrc/
│   ├── CMakeLists.txt      # CMake build file
│   ├── Makefile            # Make build file
│   ├── game/
│   │   └── g_main.c        # Game module source
│   ├── cgame/
│   │   └── cg_main.c       # CGame module source
│   └── ui/
│       └── ui_main.c       # UI module source
└── vm/
    ├── game.x86_64.so      # Compiled native libraries
    ├── cgame.x86_64.so
    └── ui.x86_64.so
```

## Example: Adding Source Files

### CMakeLists.txt
```cmake
add_library(game_${ARCH_SUFFIX} SHARED
    game/g_main.c
    game/g_weapon.c      # Add your files
    game/g_client.c
    game/g_combat.c
)
```

### Makefile
```makefile
GAME_SRCS := $(GAMEDIR)/g_main.c
GAME_SRCS += $(GAMEDIR)/g_weapon.c
GAME_SRCS += $(GAMEDIR)/g_client.c
```

## Modern C Features You Can Use

### C11 Features
- Variable-length arrays
- `_Generic` for type-generic macros
- `_Static_assert` for compile-time assertions
- Anonymous structs/unions
- `_Alignas` and `_Alignof`

### Standard Library
- Full `<stdlib.h>` (malloc, free, etc.)
- Full `<string.h>` (strcpy, strcat, etc.)
- Full `<math.h>` (sin, cos, sqrt, etc.)
- `<stdint.h>` for fixed-width types
- `<stdbool.h>` for boolean types
- And much more!

### Other Features
- Inline functions
- Function pointers
- Struct initialization
- Designated initializers
- Compound literals

## Example: Modern C Code

```c
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// C11 features
void modern_function(void) {
    // Variable-length array
    int n = 10;
    int arr[n];
    
    // Standard library
    char *str = malloc(256);
    strcpy(str, "Hello, World!");
    
    // C11 _Generic
    #define print_type(x) _Generic((x), \
        int: printf("int: %d\n", x), \
        float: printf("float: %f\n", x), \
        default: printf("unknown\n"))
    
    // Fixed-width types
    uint32_t value = 42;
    
    // Boolean type
    bool is_valid = true;
    
    free(str);
}

// Inline function (not available in QVM)
static inline float fast_sqrt(float x) {
    // Fast implementation
    return x * x;
}
```

## Debugging Native Modules

### GDB
```bash
gdb ./quake3e
(gdb) set environment LD_LIBRARY_PATH=./mymod/vm
(gdb) break dllEntry
(gdb) run +set fs_game mymod
```

### Valgrind
```bash
valgrind --leak-check=full ./quake3e +set fs_game mymod
```

### Printf Debugging
You can use `printf()` directly - no need for engine system calls!

## QVM Compatibility

**Important**: Native libraries and QVM files can coexist:

- If native library exists and loads → uses native
- If native library fails → falls back to QVM
- Other mods can still use QVM files
- Your mod uses native libraries

This means:
- ✅ You get modern C features
- ✅ QVM support remains intact
- ✅ Other mods still work
- ✅ Best of both worlds!

## Troubleshooting

### Library Not Loading
- Check file naming: `game.x86_64.so` (not `game.so`)
- Verify architecture matches your system
- Check console for error messages
- Ensure `vm_game 0` is set

### Missing Symbols
- Ensure `dllEntry` and `vmMain` are exported
- Check linking against required libraries
- Verify include paths are correct

### Wrong Architecture
- Check `ARCH_SUFFIX` in build files
- Verify your system architecture
- Build for correct target platform

### Engine Headers Not Found
- Adjust `ENGINE_ROOT` in build files
- Verify header paths are correct
- Check include directories

## Performance Benefits

Native compilation provides:
- **Faster execution** (no VM interpretation)
- **Better optimization** (modern compiler optimizations)
- **Smaller binaries** (in some cases)
- **Better debugging** (native debuggers work)

## Summary

1. Write your code using modern C features
2. Export `dllEntry` and `vmMain` functions
3. Build as shared library with correct naming
4. Place in `vm/` directory
5. Set `vm_game 0` (or `vm_cgame 0`, `vm_ui 0`)
6. Launch game - native library loads automatically
7. QVM fallback ensures compatibility

You now have modern C development while maintaining QVM compatibility!

