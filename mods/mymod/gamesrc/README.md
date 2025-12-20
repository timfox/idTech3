# Gamesrc Directory

This directory contains the source code for game modules. You can compile them as **native shared libraries** (recommended) or traditional **QVM bytecode**.

## Structure

```
gamesrc/
├── CMakeLists.txt    # CMake build configuration
├── Makefile          # Make build configuration
├── game/             # Server-side game logic
├── cgame/            # Client-side game logic
└── ui/               # User interface code
```

## Compilation Options

### Option 1: Native C Compilation (Recommended)

Compile as native shared libraries to use modern C features:

**Benefits**:
- ✅ Modern C23 features
- ✅ Full standard library access
- ✅ Better performance (no VM overhead)
- ✅ Easier debugging with modern tools
- ✅ Better IDE support

**Build Process**:

1. **Using CMake**:
   ```bash
   cd gamesrc
   mkdir build && cd build
   cmake ..
   make
   ```

2. **Using Makefile**:
   ```bash
   cd gamesrc
   make all
   ```

3. **Output**: Libraries in `../vm/` directory:
   - `game.x86_64.so` (Linux) or `game.x64.dll` (Windows)
   - `cgame.x86_64.so` / `cgame.x64.dll`
   - `ui.x86_64.so` / `ui.x64.dll`

4. **Enable**: Set CVARs to use native libraries:
   ```
   set vm_game 0
   set vm_cgame 0
   set vm_ui 0
   ```

**See `NATIVE_COMPILATION.md` for detailed guide.**

### Option 2: QVM Compilation (Legacy)

Compile as QVM bytecode for compatibility:

**Limitations**:
- ❌ Old C compiler (limited features)
- ❌ Restricted standard library
- ❌ No inline functions
- ❌ VM interpretation overhead

**Build Process**:

1. Use a Quake III compiler (q3asm, q3lcc, etc.)
2. Compile to `.qvm` files
3. Place in `../vm/` directory:
   - `game.qvm`
   - `cgame.qvm`
   - `ui.qvm`

**Example**:
```bash
q3asm -o ../vm/game.qvm game/*.c
q3asm -o ../vm/cgame.qvm cgame/*.c
q3asm -o ../vm/ui.qvm ui/*.c
```

## Module Types

### Game Module (`game/`)
- **Purpose**: Server-side game logic
- **Handles**: Game rules, entity management, player interactions, weapons
- **API**: Server-side system calls (see `g_public.h`)

### CGame Module (`cgame/`)
- **Purpose**: Client-side game logic
- **Handles**: Client prediction, visual effects, HUD, view rendering
- **API**: Client-side system calls (see `cg_public.h`)

### UI Module (`ui/`)
- **Purpose**: User interface
- **Handles**: Menus, HUD elements, settings interface
- **API**: UI system calls (see `ui_public.h`)

## Required Exports

Both native and QVM modules need:

### Native Libraries
- `dllEntry(dllSyscall_t syscallptr)` - Called on load
- `vmMain(int command, int arg0, int arg1, int arg2)` - Main entry point

### QVM Files
- Standard QVM format with `vmMain` function

See example files in each subdirectory for templates.

## Development Workflow

1. **Edit Source**: Modify C files in appropriate subdirectory
2. **Build**: 
   - Native: `make` or `cmake`
   - QVM: Use QVM compiler
3. **Test**: Launch game: `./quake3e +set fs_game mymod`
4. **Debug**: Check console for errors
5. **Iterate**: Repeat as needed

## Compatibility

**Important**: Native and QVM can coexist!

- Your mod uses native libraries (if available)
- Other mods can still use QVM files
- Engine falls back to QVM if native fails
- QVM support remains fully intact

## Modern C Features (Native Only)

With native compilation, you can use:

- **C11/C17** features (variable-length arrays, `_Generic`, etc.)
- **Full standard library** (`malloc`, `free`, `strcpy`, etc.)
- **Inline functions** for performance
- **Function pointers** and advanced features
- **Better debugging** with GDB, Valgrind, etc.

## Notes

- Native compilation is **recommended** for new development
- QVM compilation is for **legacy compatibility**
- Source code structure is the same for both
- See individual subdirectory README files for module details
- Adjust `ENGINE_ROOT` in build files to point to engine source

## Resources

- `NATIVE_COMPILATION.md` - Detailed native compilation guide
- [ioquake3 Modding Documentation](https://ioquake3.org/)
- [Quake III Arena Modding Guides](https://www.quake3world.com/)

