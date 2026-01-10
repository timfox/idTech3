# Error Handling Review - Vulkan Renderer

## Error Type Guidelines

### ERR_FATAL
**Usage**: System-level errors that prevent the renderer from functioning at all.
- **When to use**: 
  - Critical initialization failures (Vulkan instance, device, swapchain)
  - Memory allocation failures that prevent core functionality
  - Missing required Vulkan functions/entrypoints
  - System-level errors that cannot be recovered from

**Behavior**: Calls `Sys_Error()` which terminates the program immediately.

### ERR_DROP
**Usage**: Recoverable errors or data corruption that can be handled gracefully.
- **When to use**:
  - Data corruption (bad BSP data, invalid shader data)
  - Programming errors that don't prevent the game from running
  - Invalid parameters that can be logged and ignored
  - Resource limits that can be worked around

**Behavior**: Logs error, shuts down server/client, disconnects, uses setjmp/longjmp for recovery.

## Current Issues Found

### 1. Potentially Too Harsh (ERR_FATAL -> ERR_DROP)
- `tr_cmds.c:624` - Failed to acquire swapchain image
  - **Issue**: This could be recoverable (device lost, out of date)
  - **Recommendation**: Already handled with device lost recovery, but error should be ERR_DROP for non-fatal cases

- `tr_cmds.c:647,651` - Stereo frame mismatch
  - **Issue**: Programming error, but doesn't prevent rendering
  - **Recommendation**: Could be ERR_DROP or just a warning

### 2. Potentially Too Lenient (ERR_DROP -> ERR_FATAL)
- `vk.c:1036,1072` - Unsupported image layout
  - **Issue**: Indicates fundamental programming error
  - **Recommendation**: ERR_DROP is appropriate (recoverable)

- `vk.c:6826,7015` - Unknown shader type
  - **Issue**: Programming error, but renderer can continue with fallback
  - **Recommendation**: ERR_DROP is appropriate

### 3. Correct Usage
- ✅ Memory type not found - ERR_FATAL (correct)
- ✅ Instance creation failed - ERR_FATAL (correct)
- ✅ No physical device - ERR_FATAL (correct)
- ✅ Bad BSP data - ERR_DROP (correct)
- ✅ Invalid surface type - ERR_DROP (correct)
- ✅ Invalid texture unit - ERR_DROP (correct)

## Recommendations

1. **Swapchain acquisition failure**: Already handled gracefully with device lost recovery. The ERR_FATAL is only reached if all recovery attempts fail, which is appropriate.

2. **Stereo frame errors**: These are programming errors that should be ERR_DROP or warnings, not fatal.

3. **Image layout errors**: ERR_DROP is appropriate as these indicate programming errors but renderer can continue.

4. **Shader type errors**: ERR_DROP is appropriate as renderer can use fallback shaders.

## Summary

Most error handling is appropriate. The main issues are:
- Stereo frame errors could be less severe
- Some swapchain errors are already handled gracefully before reaching ERR_FATAL
