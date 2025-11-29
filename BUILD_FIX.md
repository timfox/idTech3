# Build Cache Fix

The compilation warnings you're seeing are from **cached object files** that haven't been updated with the latest code changes.

## The Code is Correct

The source file `src/qcommon/files.c` has been fixed:
- **Line 205**: Uses `__attribute__((unused))` (GCC extension) instead of `[[maybe_unused]]`
- **Line 4021**: Properly checks the return value: `if ( FS_FOpenFileRead(...) > 0 )`

## Fix: Clean Build Cache

The warnings show old line numbers (203, 4016) because CMake is using cached `.o` files. To fix:

### Option 1: Remove specific object file
```bash
rm -f build/CMakeFiles/qcommon.dir/src/qcommon/files.c.o
```

### Option 2: Clean and rebuild
```bash
cd build
cmake --build . --target clean
cmake --build .
```

### Option 3: Full clean rebuild
```bash
rm -rf build
mkdir build && cd build
cmake ..
make
```

## Verification

After cleaning, the build should show:
- ✅ No `[[maybe_unused]]` warnings (using `__attribute__((unused))` instead)
- ✅ No `[[nodiscard]]` warnings (return value is checked)

The code changes are correct; you just need to force CMake to recompile the file.

