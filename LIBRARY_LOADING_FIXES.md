# Library Loading Fixes Summary

## Issues Fixed

### 1. Library Naming Format
**Problem**: Engine was looking for `uix86_64.so` but libraries were named `ui.x86_64.so`

**Solution**: Updated `src/qcommon/vm.c` line 1714 to use dot-separated format:
```c
Com_sprintf( filename, sizeof( filename ), "%s." ARCH_STRING DLL_EXT, name );
```

**Files Changed**:
- `src/qcommon/vm.c` - Updated library naming format

### 2. Library Search Path
**Problem**: `FS_LoadLibrary` only searched game directories directly, not `vm/` subdirectories

**Solution**: Updated `FS_LoadLibrary` to check `vm/` subdirectories first (matching QVM file loading behavior):
```c
// First try vm/ subdirectory (where libraries are typically stored)
Com_sprintf( vmPath, sizeof( vmPath ), "vm/%s", name );
const char *fn = FS_BuildOSPath( sp->dir->path, sp->dir->gamedir, vmPath );
libHandle = Sys_LoadLibrary( fn );

// If not found in vm/, try directly in game directory
if ( !libHandle ) {
    fn = FS_BuildOSPath( sp->dir->path, sp->dir->gamedir, name );
    libHandle = Sys_LoadLibrary( fn );
}
```

**Files Changed**:
- `src/qcommon/files.c` - Added `vm/` subdirectory search

### 3. Library Location
**Problem**: Libraries were in wrong locations

**Solution**: Libraries now in correct locations:
- `release/mymod/vm/ui.x86_64.so`
- `release/mymod/vm/game.x86_64.so`
- `release/mymod/vm/cgame.x86_64.so`

### 4. Makefile Updates
**Problem**: Makefile output `uix86_64.so` instead of `ui.x86_64.so`

**Solution**: Updated Makefile to use dot-separated format:
```makefile
UI_LIB := $(OUTDIR)/ui.$(ARCH_SUFFIX).so
```

**Files Changed**:
- `mymod/gamesrc/Makefile` - Updated output naming

## Library Dependency Resolution

The UI library depends on `../vm/game.x86_64.so` which resolves correctly:
- When `ui.x86_64.so` is loaded from `release/mymod/vm/`
- `$ORIGIN` RPATH points to `release/mymod/vm/`
- `../vm/game.x86_64.so` resolves to `release/mymod/vm/game.x86_64.so` ✓

Verified with `ldd release/mymod/vm/ui.x86_64.so` - dependency resolves correctly.

## Search Order

`FS_LoadLibrary` now searches in this order:
1. `{basepath}/{gamedir}/vm/{library}` (e.g., `release/mymod/vm/ui.x86_64.so`)
2. `{basepath}/{gamedir}/{library}` (e.g., `release/mymod/ui.x86_64.so`)
3. Continues through all search paths (homepath, basepath, etc.)

This matches the QVM file loading behavior (`vm/{name}.qvm`).

## Next Steps

1. **Rebuild engine** to pick up `FS_LoadLibrary` changes:
   ```bash
   cd build && cmake --build . && cp idtech3.x86_64 ../release/
   ```

2. **Test**: Run the game - it should now find and load `ui.x86_64.so` from `release/mymod/vm/`

3. **Verify**: Check console output for "VM_LoadLib 'ui.x86_64.so' ok" message

## Files Modified

1. `src/qcommon/vm.c` - Library naming format
2. `src/qcommon/files.c` - Added `vm/` subdirectory search
3. `mymod/gamesrc/Makefile` - Updated output naming

## Status

✅ Library naming format fixed
✅ Library search path updated
✅ Libraries in correct location
✅ Dependencies resolve correctly
⏳ Engine needs rebuild to pick up changes

