# UI Library Dependency Fix

## Problem

The `ui.x86_64.so` library has a dependency on `../vm/game.x86_64.so`, which `dlopen` resolves relative to the current working directory (not relative to the library's location or `$ORIGIN` RPATH). This causes the dependency to fail to resolve.

## Root Cause

When linking `ui.x86_64.so` against `../vm/game.x86_64.so`, the linker embeds the file path `../vm/game.x86_64.so` as the dependency name. When `dlopen` loads the library, it resolves this relative path from the current working directory (`/home/tim/Desktop/idtech3/release`), giving `/home/tim/Desktop/idtech3/vm/game.x86_64.so` (which doesn't exist), instead of `/home/tim/Desktop/idtech3/release/mymod/vm/game.x86_64.so` (which does exist).

The `$ORIGIN` RPATH only helps if the dependency name doesn't contain `../` - it doesn't affect how `dlopen` resolves relative paths in DT_NEEDED entries.

## Solutions

### Solution 1: Use patchelf (Recommended)

Install `patchelf` and run the fix script:

```bash
sudo apt install patchelf
cd mymod/gamesrc
./fix-ui-dependency.sh
```

Or manually:
```bash
cd release/mymod/vm
patchelf --replace-needed ../vm/game.x86_64.so game.x86_64.so ui.x86_64.so
```

This changes the dependency from `../vm/game.x86_64.so` to `game.x86_64.so`, which will resolve correctly using `$ORIGIN` RPATH.

### Solution 2: Fix Makefile Linking

Modify the Makefile to link using the SONAME instead of the file path. However, the linker still embeds the path when linking from a different directory.

### Solution 3: Set LD_LIBRARY_PATH in Engine

Modify `Sys_LoadLibrary` to set `LD_LIBRARY_PATH` before calling `dlopen`. This would require changes to the engine code.

### Solution 4: Copy Game Library (Temporary Workaround)

Copy `game.x86_64.so` to `release/vm/` so that `../vm/game.x86_64.so` resolves correctly from `release/mymod/vm/`:

```bash
cp release/mymod/vm/game.x86_64.so release/vm/game.x86_64.so
```

However, this doesn't work because `dlopen` resolves relative to the working directory, not relative to the library location.

## Current Status

- ✅ `FS_LoadLibrary` updated to check `vm/` subdirectories
- ✅ Libraries in correct location (`release/mymod/vm/`)
- ✅ Game library has correct SONAME (`game.x86_64.so`)
- ❌ UI library dependency name is incorrect (`../vm/game.x86_64.so` instead of `game.x86_64.so`)

## Verification

After applying Solution 1, verify with:
```bash
readelf -d release/mymod/vm/ui.x86_64.so | grep NEEDED
```

Should show: `game.x86_64.so` (not `../vm/game.x86_64.so`)

Then test:
```bash
./release/idtech3.x86_64 +set fs_game mymod +set vm_ui 0
```

Should see: `VM_LoadLib 'ui.x86_64.so' ok`

