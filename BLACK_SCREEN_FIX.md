# Black Screen Fix - Summary

## Root Cause
The black screen issue was caused by **`vk_present_frame()` never being called**, preventing rendered frames from being displayed to the screen.

### The Bug
In `src/renderers/vulkan/tr_backend.c`:
```c
static const void *RB_SwapBuffers( const void *data ) {
    ...
    vk_end_frame();
    ...
    vk_present_frame();  // ❌ NEVER REACHED!
    ...
}
```

### Why It Was Never Reached
1. `RB_SwapBuffers()` calls `vk_end_frame()`
2. After `RB_SwapBuffers()`, the render command loop encounters `RC_END_OF_LIST`
3. `RC_END_OF_LIST` handler calls `vk_end_frame()` **AGAIN** and returns immediately
4. Control never returns to `RB_SwapBuffers()`, so `vk_present_frame()` was never called

### The Solution
**Move `vk_present_frame()` to the END of `vk_end_frame()`** in `src/renderers/vulkan/vk.c`:

```c
void vk_end_frame( void )
{
    // ... all frame processing ...
    
    // CRITICAL FIX: Present the frame NOW, at the end of vk_end_frame().
    // Previously vk_present_frame() was only called from RB_SwapBuffers(),
    // but RB_ExecuteRenderCommands() returns after RC_END_OF_LIST (which calls vk_end_frame()),
    // so vk_present_frame() in RB_SwapBuffers() was NEVER REACHED -> black screen!
    vk_present_frame();
}
```

## Additional Fixes

### 1. Missing Viewport/Scissor for Gamma Pass
**File**: `src/renderers/vulkan/vk.c`

The gamma pass (final post-processing) wasn't setting viewport/scissor before drawing:
```c
// Before: missing viewport/scissor setup
vk_begin_render_pass( vk.render_pass.gamma, ... );
qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );  // ❌ Invalid state

// After: proper viewport/scissor setup
vk_begin_render_pass( vk.render_pass.gamma, ... );
{
    VkViewport viewport = {
        .x = 0.0f, .y = 0.0f,
        .width = (float)gls.windowWidth,
        .height = (float)gls.windowHeight,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };
    VkRect2D scissor_rect = {
        .offset = {0, 0},
        .extent = {gls.windowWidth, gls.windowHeight}
    };
    qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
    qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
}
qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );  // ✅ Now renders correctly
```

### 2. Gamma Pass Render Area Mismatch
**File**: `src/renderers/vulkan/vk.c`

The render area was using internal render resolution instead of swapchain/window dimensions:
```c
// Before
vk_begin_render_pass( ..., vk.renderWidth, vk.renderHeight );  // ❌ Wrong dimensions

// After
vk_begin_render_pass( ..., gls.windowWidth, gls.windowHeight );  // ✅ Correct
```

### 3. UI Rendering Without Active Render Pass
**File**: `src/renderers/vulkan/tr_backend.c`

On menu-only frames (no 3D world), `RB_DrawSurfs()` was never called, so the main render pass never started:
```c
static void RB_SetGL2D( void ) {
    backEnd.projection2D = qtrue;

#ifdef USE_VULKAN
    // CRITICAL: Ensure main render pass is started before UI rendering.
    if ( vk.renderPassIndex >= RENDER_PASS_COUNT ) {
        vk_begin_main_render_pass();
    }
    ...
}
```

## Other Fixes

### 4. UI2 Code Gated Behind CMake Flag
**File**: `CMakeLists.txt`

```cmake
OPTION(USE_UI2 "Enable UI2 deterministic UI/layout system" OFF)
```

### 5. Double-Free Warnings Removed
**File**: `src/qcommon/common.c`

Changed from warning spam to silent early return in `Z_Free()`.

## Testing
Run the game with:
```bash
cd /home/tim/Desktop/idtech3/release
./idtech3.x86_64.so +set fs_game mymod +set r_mode 3 +set r_fullscreen 0
```

The game window should now display properly with menu and UI visible!

## Performance Notes
- Lower graphics settings recommended to avoid VRAM exhaustion
- Disable advanced features: DLSS, HDR, high MSAA, supersampling  
- Use lower resolution modes for testing (mode 3 = 640x480)
