# Engine Improvements Implementation Plan

This document tracks the systematic implementation of improvements identified in the architecture analysis.

## Phase 1: Critical Fixes (Week 1-2)

### 1.1 Fix Dynamic Renderer Loading ✓ IN PROGRESS
**Problem:** SDL backend Vulkan functions not properly exported to dynamically loaded renderers

**Root Cause:**
- Functions defined in `sdl_glimp.c`, compiled into `q3ui` OBJECT library
- Main executable needs `-rdynamic` (present) BUT functions must be actually linked into executable
- Current: q3ui is OBJECT library, its objects linked into executable
- Issue: Conditional compilation means functions may not exist at all

**Solution:**
1. Ensure `USE_VULKAN` and `USE_SDL` are both defined for client target ✓ (already done)
2. Verify q3ui objects are properly linked into main executable
3. Add explicit symbol export macros for Vulkan functions
4. Add runtime validation that pointers are set correctly

**Files to Modify:**
- `src/sdl/sdl_glw.h` - Add export macros
- `src/sdl/sdl_glimp.c` - Mark functions for export
- `src/client/cl_main.c` - Add validation
- `CMakeLists.txt` - Verify linking

### 1.2 Standardize Feature Flags
**Problem:** USE_VULKAN vs USE_VULKAN_API confusion

**Solution:**
1. Audit all uses of both flags
2. Choose ONE canonical flag (recommend: `USE_VULKAN`)
3. Replace all instances consistently
4. Update CMake to only set one flag

**Impact:** ~50 files to update

### 1.3 Add Critical Validation
**Problem:** NULL function pointers cause crashes

**Solution:**
1. Add validation macro for refimport_t assignment
2. Check all function pointers before GetRefAPI call
3. Provide clear error messages

## Phase 2: Build System Modernization (Week 3-4)

### 2.1 Split CMakeLists.txt
**Current:** 2,707 lines in one file
**Target:** Modular structure

**New Structure:**
```
cmake/
  ├── PlatformDetection.cmake
  ├── CompilerFlags.cmake
  ├── Dependencies.cmake
  ├── Renderers.cmake
  ├── Server.cmake
  ├── Client.cmake
  └── Tests.cmake
```

### 2.2 Simplify Feature Flags
**Actions:**
- Document each flag purpose
- Remove redundant flags
- Create flag dependency graph

## Phase 3: Memory Safety (Week 5-6)

### 3.1 String Operation Audit
**Scope:** 1,540 instances

**Priority Files:**
1. Network code (msg.c, net_chan.c)
2. File parsing (files.c, unzip.c)
3. Command handling (cmd.c, cvar.c)

**Actions:**
- Add Q_strncpyz_s with validation
- Replace unsafe operations
- Add compile-time size checks

### 3.2 Network Buffer Validation
**Scope:** 556 MSG_* calls

**Actions:**
- Add bounds checking macros
- Implement overflow detection
- Add fuzzing tests

## Phase 4: Code Organization (Week 7-10)

### 4.1 Refactor files.c
**Current:** 6,851 lines
**Target:** 4-5 modules ~1,500 lines each

**Modules:**
- `vfs_core.c` - Core VFS API
- `vfs_pak.c` - PK3/PAK loading
- `vfs_mount.c` - Mount table management
- `vfs_cache.c` - Path/existence caching
- `vfs_search.c` - File search operations

### 4.2 Standardize Error Handling
**Current:** 4 different error patterns
**Target:** Unified error handling

**New Pattern:**
```c
typedef enum {
    ERR_LEVEL_INFO,
    ERR_LEVEL_WARNING,
    ERR_LEVEL_RECOVERABLE,
    ERR_LEVEL_FATAL
} errorLevel_t;

qboolean Err_Handle(errorLevel_t level, const char *fmt, ...);
```

## Phase 5: Platform Abstraction (Week 11-12)

### 5.1 Create Platform Abstraction Layer
**Target:** Reduce 95 #ifdef blocks to ~20

**Structure:**
```
src/platform/
  ├── platform.h          - Common interface
  ├── platform_linux.c
  ├── platform_windows.c
  ├── platform_macos.c
  └── platform_ios.c
```

**Abstracted Functions:**
- Window management
- Input handling  
- File system ops
- Threading primitives

## Phase 6: Testing Infrastructure (Week 13-14)

### 6.1 Increase Test Coverage
**Current:** <5% (7 test files)
**Target:** 20%+ critical path coverage

**New Tests:**
- `test_msg_parsing.c` - Network protocol
- `test_renderer_init.c` - Renderer loading
- `test_vfs.c` - File system operations
- `test_string_safety.c` - String operations
- `test_allocators.c` - Memory management

### 6.2 Add Integration Tests
- Renderer initialization sequence
- Network packet handling
- File loading workflows

### 6.3 Add Fuzzing
- Network packet fuzzer
- PK3 file fuzzer
- Console command fuzzer

## Success Metrics

### Critical (Must Have)
- [ ] Zero NULL function pointer crashes
- [ ] All network inputs bounds-checked
- [ ] CMake builds complete under 5 minutes

### High Priority (Should Have)
- [ ] Test coverage >20%
- [ ] files.c split into modules
- [ ] Platform #ifdefs reduced by 50%

### Medium Priority (Nice to Have)
- [ ] All files <2000 lines
- [ ] Unified error handling
- [ ] Documentation updated

## Timeline Summary

| Phase | Duration | Priority | Status |
|-------|----------|----------|--------|
| 1. Critical Fixes | 2 weeks | P0 | IN PROGRESS |
| 2. Build System | 2 weeks | P1 | PLANNED |
| 3. Memory Safety | 2 weeks | P0 | PLANNED |
| 4. Code Organization | 4 weeks | P1 | PLANNED |
| 5. Platform Abstraction | 2 weeks | P2 | PLANNED |
| 6. Testing | 2 weeks | P1 | PLANNED |

**Total Estimated Duration:** 14 weeks (3.5 months)

## Next Actions

1. Complete renderer loading fix
2. Validate with test compilation
3. Move to feature flag standardization
4. Begin CMake refactoring

---

*Updated: 2024*

