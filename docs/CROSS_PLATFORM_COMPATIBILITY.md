# Cross-Platform Compatibility Guide

## Overview

This document outlines cross-platform compatibility considerations for the id Tech 3 codebase, covering Windows, Linux, macOS, iOS, Android, and other supported platforms.

## Platform Support Matrix

| Platform | Architecture | Renderer | Status | Notes |
|----------|-------------|----------|--------|-------|
| Linux    | x86_64, ARM64 | Vulkan, OpenGL | ✅ Primary | Main development platform |
| Windows  | x86_64       | Vulkan, OpenGL, D3D12 | ✅ Supported | DirectX 12 support available |
| macOS    | x86_64, ARM64 | Metal, OpenGL | ✅ Supported | Metal renderer available |
| iOS      | ARM64        | Metal    | ✅ Supported | iOS 11.0+ required |
| Android  | ARM64, ARMv7 | Vulkan, OpenGL ES | ✅ Supported | Android 21+ (API level) |
| FreeBSD  | x86_64       | Vulkan, OpenGL | ⚠️ Community | Similar to Linux |

## Platform-Specific Code Patterns

### 1. File System Paths

**Issue**: Path separators and case sensitivity differ across platforms.

**Solution**: Use platform abstraction functions:
```c
// Use Q_FS_PATH_SEP for path separators
// Use Q_FS_CASE_SENSITIVE for case sensitivity checks
// Use Com_BuildPath() for constructing paths
```

**Files to Review**:
- `src/common/files.c`
- `src/common/qfiles.h`

### 2. Threading

**Issue**: Threading APIs differ (pthreads, Windows threads, etc.)

**Solution**: Use platform abstraction layer in `src/common/thread_platform.h`

**Current Implementation**:
- Linux/Unix: pthreads
- Windows: Windows threads
- macOS/iOS: pthreads (POSIX compliant)

### 3. Terminal I/O

**Issue**: Terminal handling varies (termios, Windows console, etc.)

**Current Status**:
- Unix: Uses `termios.h` (POSIX standard)
- Windows: Uses Windows console API
- FIXME markers in `src/unix/unix_main.c` for terminal compatibility

**Recommendations**:
- Add feature detection macros (HAVE_TERMIOS_H)
- Consider ncurses for advanced terminal features
- Document terminal requirements per platform

### 4. Memory Management

**Cross-Platform Considerations**:
- Page size may differ (4KB vs 8KB vs 16KB)
- Memory alignment requirements vary
- Use `sysconf(_SC_PAGESIZE)` or platform equivalents

**Files**:
- `src/common/qcommon.h` - Memory management
- Platform-specific memory code in `src/unix/`, `src/win32/`

### 5. Endianness

**Status**: Handled via `LittleLong()`, `BigLong()` macros

**Platforms**:
- x86/x86_64: Little-endian (primary)
- ARM: Configurable (typically little-endian)
- Some embedded: Big-endian

**Testing**: Use `src/common/cross_platform_test.c` framework

## Platform Abstraction Gaps

### Identified Issues

1. **Direct Platform API Usage** (HIGH Priority)
   - 95 `#ifdef` blocks across 41 files
   - 23 files in `src/unix/`
   - 12 files in `src/win32/`
   - **Recommendation**: Create unified platform abstraction layer (PAL)

2. **Duplicated Platform Code** (MEDIUM Priority)
   - `linux_glimp.c` vs `win_glimp.c`
   - `unix_main.c` vs `win_main.c`
   - **Recommendation**: Extract common code, keep only platform-specific parts

3. **macOS/iOS Objective-C++** (LOW Priority)
   - `.mm` files mixed with C code
   - **Recommendation**: Isolate Objective-C to minimal bridge layer

## Compiler Compatibility

### Supported Compilers

| Compiler | Version | C Standard | C++ Standard | Status |
|----------|---------|------------|--------------|--------|
| GCC      | 15+     | C23        | C++23        | ✅ Primary |
| Clang    | 18+     | C23        | C++23        | ✅ Supported |
| MSVC     | 19.30+  | C23*       | C++23*       | ⚠️ Partial |

*MSVC C23/C++23 support may be limited - verify feature availability

### Compiler-Specific Considerations

1. **GCC/Clang**: Full C23/C++23 support
2. **MSVC**: May require compatibility shims for some features
3. **Feature Detection**: Use `__has_feature()` (Clang) or version checks

## Architecture Support

### Supported Architectures

| Architecture | JIT VM | Status | Notes |
|--------------|--------|---------|-------|
| x86_64       | ✅     | Primary | Full JIT support |
| ARM64        | ✅     | Supported | AArch64 JIT available |
| ARMv7        | ⚠️     | Limited | ARMv7l JIT exists but may need updates |
| RISC-V       | ❌     | Future  | No JIT support yet |

### Architecture-Specific Code

- `src/common/vm_x86.c` - x86_64 JIT
- `src/common/vm_aarch64.c` - ARM64 JIT
- `src/common/vm_armv7l.c` - ARMv7 JIT

**Recommendations**:
- Complete ARM64 support (especially for Apple Silicon)
- Consider removing x86 32-bit JIT (legacy)
- Add RISC-V support for future platforms

## Testing Across Platforms

### Automated Testing

The codebase includes cross-platform test framework:
- `src/common/cross_platform_test.h`
- `src/common/cross_platform_test.c`
- `src/common/compatibility_test.h`
- `src/common/compatibility_test.c`

### Manual Testing Checklist

For each platform:
- [ ] Build succeeds (Release and Debug)
- [ ] Vulkan renderer initializes
- [ ] OpenGL renderer initializes (where applicable)
- [ ] Network code works
- [ ] File system operations work
- [ ] Audio system works
- [ ] Input handling works
- [ ] Memory management works correctly

### CI/CD Platform Testing

GitHub Actions workflows should test:
- Linux (x86_64, ARM64)
- Windows (x86_64)
- macOS (x86_64, ARM64) - if available

## Common Cross-Platform Issues

### 1. Path Separators

**Problem**: `/` vs `\` vs platform-specific

**Solution**: Always use `Q_FS_PATH_SEP` or `Com_BuildPath()`

### 2. Case Sensitivity

**Problem**: Linux is case-sensitive, Windows/macOS are case-insensitive (by default)

**Solution**: Use case-insensitive comparisons for file operations

### 3. Line Endings

**Problem**: `\n` vs `\r\n` vs `\r`

**Solution**: Use text mode for file I/O, or normalize line endings

### 4. Shared Libraries

**Problem**: `.so` (Linux) vs `.dll` (Windows) vs `.dylib` (macOS)

**Solution**: CMake handles this automatically via `CMAKE_SHARED_LIBRARY_SUFFIX`

### 5. Thread Local Storage

**Problem**: Different TLS implementations

**Solution**: Use platform abstraction or compiler TLS keywords (`__thread`, `thread_local`)

## Platform-Specific Build Notes

### Linux

- Requires Vulkan SDK
- SDL2 development libraries
- OpenGL development libraries
- Standard build: `./scripts/compile_engine.sh opengl`

### Windows

- Visual Studio 2019+ or MinGW-w64
- Vulkan SDK
- DirectX SDK (for D3D12 renderer)
- CMake handles most dependencies

### macOS

- Xcode with command-line tools
- Metal framework (system)
- Vulkan via MoltenVK (optional)
- Standard build via CMake

### iOS

- Xcode required
- iOS 11.0+ deployment target
- Metal renderer only
- See `docs/IOS_BUILD_STATUS.md`

### Android

- Android NDK 25.1+
- Android API level 21+
- See `platform/android-app/README_ANDROID.md`

## Recommendations for Future Improvements

### High Priority

1. **Create Platform Abstraction Layer (PAL)**
   - Unified API for file system, threading, memory
   - Reduce `#ifdef` blocks
   - Easier to add new platforms

2. **Improve Terminal Handling**
   - Add feature detection
   - Better cross-platform terminal support
   - Consider ncurses for advanced features

3. **Complete ARM64 Support**
   - Full JIT support for Apple Silicon
   - Test on ARM64 Linux
   - Verify Android ARM64

### Medium Priority

1. **Reduce Code Duplication**
   - Extract common platform code
   - Keep only truly platform-specific code separate

2. **Improve Build System**
   - Better cross-compilation support
   - Platform-specific toolchain files
   - Automated platform testing

### Low Priority

1. **Isolate Objective-C Code**
   - Separate `.mm` files from C code
   - Minimal bridge layer for macOS/iOS

2. **Add RISC-V Support**
   - For future embedded/console platforms
   - Requires JIT compiler work

## Resources

- Platform detection: `src/common/cross_platform_test.h`
- Compatibility tests: `src/common/compatibility_test.c`
- Build documentation: `docs/BUILD_PLATFORM.md`
- iOS/macOS: `docs/IOS_BUILD_STATUS.md`
- Android: `platform/android-app/README_ANDROID.md`

---

**Last Updated**: January 2024  
**Maintainer**: id Tech 3 Development Team
