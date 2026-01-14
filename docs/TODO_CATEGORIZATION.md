# TODO/FIXME Categorization and Status

This document categorizes TODO and FIXME markers found throughout the codebase for systematic tracking and prioritization.

## Categories

### 🔴 Critical (Renderer Boundary - Do Not Touch)
**Location**: `src/renderers/`  
**Status**: Preserve interfaces, only fix bugs with extreme caution

- `src/renderers/vulkan/tr_shader.c:2628` - Shader collapse logic verification
- `src/renderers/vulkan/tr_backend.c` - Multiple RF_DEPTHHACK handling locations
- `src/renderers/vulkan/tr_surface.c:1447` - Vertex layout optimization
- `src/renderers/vulkan/vk_dlss.c` - DLSS SDK integration (future feature)
- `src/renderers/vulkan/rtx/vk_rtx_main.cpp` - Compute ray tracing (future feature)
- `src/renderers/vulkan/vk_ultra_post_process.cpp` - Compute shader loading (future feature)

**Action**: Document only, do not modify without extensive testing.

---

### 🟡 Medium Priority (Non-Renderer, Safe to Improve)
**Location**: `src/common/`, `src/client/`, `src/server/`  
**Status**: Safe for incremental improvements

#### Code Organization
- `src/client/cl_ui.c:790` - Move hardcoded extensions to syscall_registry.c
- `src/client/cl_cgame.c:443` - Move extensions to syscall_registry.c
- `src/common/q_shared.h:566` - Consider making qhandle_t opaque (future major version)

#### Feature Implementation
- `src/common/ecs_systems.cpp` - Multiple ECS system TODOs (gameplay features)
  - Collision callbacks (line 328)
  - Entity visibility/rendering (line 570)
  - Pickup system (line 576)
  - Door unlocking (line 597)
  - Backpack effects (line 624)
  - Completion events (line 654)
  - Progress tracking (line 657)
  - Fade alpha rendering (line 692)
  - Class-specific bonuses (line 719)
  - Movement modifiers (line 721)
  - Fireteam communication (line 788)

- `src/common/cm_bullet.cpp` - Physics integration
  - Proper brush geometry extraction (line 121)
  - Submodel collision geometry (line 222)
  - Bullet-based tracing (line 311)
  - Point contents checking (line 328)

#### Debug/Development
- `src/client/cl_main.c:4860,4934` - Debug command functions
- `src/client/cl_main.c.backup` - Same (backup file, can be ignored)

---

### 🟢 Low Priority (Documentation/Clarification)
**Location**: Various  
**Status**: Documentation improvements, code comments

#### Commented-Out Code
- `src/common/cvar.c:352` - FIXME: values with backslash validation (disabled)
- `src/common/cvar.c:628` - FIXME: Disabled code block
- `src/common/cvar.c:1246` - FIXME: Cvar creation restriction

#### Legacy/Compatibility Notes
- `src/unix/unix_main.c:63` - TTimo comment about *nix compliance
- `src/unix/unix_main.c:156` - TTimo comment about relevance
- `src/unix/unix_main.c:208` - TTimo comment about cursor positioning
- `src/common/cm_load.c:541` - FIXME: Non-colliding patches check
- `src/common/q_shared.h:712` - FIXME: Print levels from renderer

#### Known Issues
- `src/client/cl_cgame.c:162` - FIXME: Configstring changes and server commands
- `src/client/cl_cgame.c:654` - FIXME: Server restart timing issue
- `src/client/cl_cgame.c:1201` - FIXME: oldServerTime for cgame

---

### 🔵 Future Features (Not Blocking)
**Location**: Various  
**Status**: Planned enhancements, not critical

- DLSS SDK dynamic loading
- Mesh shader implementation
- Enhanced post-processing compute shaders
- Advanced physics integration
- OOP entity architecture migration

---

## Recommended Action Plan

### Phase 1: Safe Improvements (Current)
1. ✅ Create this categorization document
2. 🔄 Add clarifying comments to FIXME markers
3. 🔄 Improve code organization (move syscall extensions)
4. 🔄 Add documentation for opaque type migration path

### Phase 2: Code Organization
1. Move hardcoded syscall extensions to syscall_registry.c
2. Clean up backup files
3. Document disabled code blocks with rationale

### Phase 3: Feature Implementation (Future)
1. ECS system gameplay features
2. Bullet physics integration
3. Debug command system

### Phase 4: Renderer Improvements (Requires Testing)
1. Shader collapse logic verification
2. Vertex layout optimization
3. DLSS/FSR integration

---

## Notes

- **Renderer Boundary**: The renderer interface (`rendererInterface_t`, `tr_public.h`) must remain stable. Changes to renderer internals require extensive testing.
- **Incremental Approach**: Prefer small, focused changes over large refactors.
- **Testing**: All changes should be validated with both Vulkan and OpenGL renderers.
- **Documentation**: When addressing TODOs, update this document and add inline comments explaining the change.

---

## Statistics

- **Total TODOs/FIXMEs**: ~218 markers across 97 files
- **Renderer Code**: ~15 markers (preserve interfaces)
- **Safe to Improve**: ~50 markers (non-renderer, well-scoped)
- **Documentation**: ~30 markers (comments, clarifications)
- **Future Features**: ~20 markers (planned enhancements)
- **Legacy Notes**: ~10 markers (historical comments)

Last Updated: 2024
