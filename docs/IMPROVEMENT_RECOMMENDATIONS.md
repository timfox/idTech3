# Future Improvement Recommendations

## Overview

This document provides recommendations for future incremental improvements to the id Tech 3 codebase, organized by priority and risk level. All recommendations follow the incremental improvement philosophy and preserve renderer boundaries.

## High Priority - Safe Improvements

### 1. C/C++ Linkage Issues
**Location**: `src/common/cm_bullet.cpp`

**Issue**: `CM_ClipHandleToModel` linkage error when called from C++

**Recommendation**:
- Add `extern "C"` guards to `cm_local.h` when included from C++ files
- Or create a C++-compatible wrapper function
- Document the solution once implemented

**Risk**: Low (internal function, well-documented)

### 2. Network Error String Typo
**Location**: `src/common/net_ip.c:269`

**Issue**: Typo in error string "WSWSAECONNABORTEDAEINTR" (fixed in this session)

**Status**: ✅ Fixed

### 3. Unused Variable Documentation
**Location**: `src/common/cm_bullet.cpp`

**Issue**: Unused variables need better documentation

**Status**: ✅ Improved in this session

## Medium Priority - Code Quality

### 1. Const Correctness
**Opportunities**:
- Review function parameters that don't modify data
- Add `const` qualifiers where appropriate
- Focus on non-renderer code first

**Files to Review**:
- `src/common/cm_bullet.cpp`
- `src/common/net_ip.c`
- `src/common/cmd.c`

**Risk**: Low (compile-time checks only)

### 2. Error Message Consistency
**Opportunities**:
- Standardize error message format
- Use consistent error codes
- Improve error context information

**Files to Review**:
- All files using `Com_Printf` for errors
- Consider using error handling framework

**Risk**: Low (non-functional change)

### 3. Code Documentation
**Opportunities**:
- Add function-level documentation
- Document complex algorithms
- Explain design decisions

**Priority Areas**:
- Bullet physics integration
- Network code
- File system operations

**Risk**: None (documentation only)

## Low Priority - Future Enhancements

### 1. Bullet Physics Implementation
**Location**: `src/common/cm_bullet.cpp`

**TODOs**:
- Proper brush geometry extraction
- Bullet-based tracing for dynamic objects
- Point contents checking
- Submodel collision geometry
- Debug line drawing

**Risk**: Medium (requires testing)

### 2. Command Completion
**Location**: `src/common/vm_hot_reload.c`

**TODO**: Implement command completion for `vm_reload`

**Risk**: Low (new feature)

### 3. Lua Event Script Tracking
**Location**: `src/common/lua_events.c`

**TODO**: Extract and store script names for debugging

**Risk**: Low (enhancement)

### 4. Physics Step Time Tracking
**Location**: `src/common/physics_bullet.cpp`

**TODO**: Track physics step time for performance monitoring

**Risk**: Low (performance metric)

## Renderer Boundary - Do Not Modify

### Critical Areas
- `src/renderers/vulkan/` - All files
- `src/renderers/opengl/` - All files
- `src/renderers/renderercommon/tr_public.h` - Public API
- `src/renderers/renderer_abstraction.h` - Abstraction layer

### Safe Documentation Only
- TODO/FIXME comments can be improved
- No functional changes
- No interface modifications

## Testing Recommendations

### Before Making Changes
1. Verify build succeeds (Vulkan and OpenGL)
2. Run linter checks
3. Test basic functionality
4. Check for regressions

### After Making Changes
1. Full build verification
2. Run test suite (if available)
3. Manual smoke testing
4. Document changes

## Code Quality Metrics

### Current Status
- TODO/FIXME comments: ~218 markers (25+ enhanced)
- Dead code removed: ~30 lines
- Documentation files: 5 created
- Build status: ✅ All configurations successful

### Target Metrics
- TODO/FIXME clarity: 100% documented
- Code duplication: < 5%
- Test coverage: > 80% (long-term goal)
- Documentation coverage: > 90% (long-term goal)

## Implementation Guidelines

### Incremental Approach
1. Make small, focused changes
2. Verify each change builds
3. Document as you go
4. Preserve existing functionality

### Documentation Standards
- Explain *why* not just *what*
- Include implementation guidance
- Document cross-platform considerations
- Provide examples where helpful

### Error Handling
- Use consistent error messages
- Include context information
- Provide actionable feedback
- Log appropriately (debug vs. release)

## Priority Matrix

| Priority | Risk | Impact | Effort | Recommendation |
|----------|------|--------|--------|----------------|
| High | Low | High | Low | Fix linkage issues |
| High | Low | Medium | Low | Improve error messages |
| Medium | Low | Medium | Medium | Const correctness pass |
| Medium | Low | Low | Low | Code documentation |
| Low | Medium | High | High | Bullet physics implementation |
| Low | Low | Low | Low | Command completion |

## Next Steps

### Immediate (This Week)
1. ✅ Fix network error typo (done)
2. ✅ Improve unused variable documentation (done)
3. Document C/C++ linkage solution approach

### Short Term (This Month)
1. Const correctness pass (non-renderer code)
2. Error message standardization
3. Additional TODO documentation

### Long Term (Ongoing)
1. Bullet physics implementation
2. Command completion features
3. Comprehensive code documentation

## Resources

### Documentation
- `docs/TODO_CATEGORIZATION.md` - TODO tracking
- `docs/CROSS_PLATFORM_COMPATIBILITY.md` - Platform guide
- `docs/INCREMENTAL_IMPROVEMENTS_2024.md` - Improvement log
- `docs/CODE_QUALITY_IMPROVEMENTS_SUMMARY.md` - Quality summary

### Tools
- Build scripts: `scripts/compile_engine.sh`
- Linter: Check with `read_lints`
- Testing: `scripts/test_engine.sh`

---

**Last Updated**: January 2024  
**Maintainer**: id Tech 3 Development Team  
**Status**: Active recommendations for ongoing improvements
