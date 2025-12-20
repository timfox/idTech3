# C++23 Migration Opportunities

This document identifies safe, incremental opportunities to adopt C++23 features without breaking existing functionality.

## Overview

The codebase has several areas where C++23 features can provide:
- **Better type safety** (std::expected, std::optional)
- **Improved performance** (std::string_view, std::format)
- **Cleaner code** (std::print, if consteval)
- **Better error handling** (std::expected)

## Safe Migration Areas

### 1. String Formatting (Low Risk, High Impact)

**Current Pattern:**
```cpp
// src/renderervk/vk.c
Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x", ... );
ri.Printf( PRINT_WARNING, "VK: Failed to create gamma compute pipeline: %d\n", result );
```

**C++23 Improvement:**
```cpp
#include <print>  // C++23 std::print

// Type-safe, faster, no buffer overflows
std::print(stderr, "VK: Failed to create gamma compute pipeline: {}\n", result);
std::print("Device: {} {}, 0x{:04x}\n", vendor, model, deviceId);
```

**Files to Update:**
- `src/renderervk/vk.c` (1843 printf/Com_sprintf calls)
- `src/renderervk/vk_dlss.c`
- `src/renderervk/vk_raytracing.c`
- `src/renderervk/vk_virtual_texture.c`

**Migration Strategy:**
1. Start with new code - use `std::print` for all new logging
2. Gradually replace `ri.Printf` calls in non-critical paths
3. Keep `Com_sprintf` for C interface compatibility (C code can't use C++23)

**Benefits:**
- Compile-time format string checking
- No buffer overflow risks
- Better performance (no runtime parsing)
- Type-safe formatting

---

### 2. Optional Return Values (Medium Risk, High Safety)

**Current Pattern:**
```cpp
// src/common/ecs.cpp
ecs_entity_t ECS_CreateEntity(void) {
    if (g_registry == nullptr) {
        return ECS_NULL_ENTITY;  // Magic value indicates failure
    }
    entt::entity entity = g_registry->create();
    return static_cast<ecs_entity_t>(entity);
}
```

**C++23 Improvement:**
```cpp
#include <optional>

std::optional<ecs_entity_t> ECS_CreateEntity(void) {
    if (g_registry == nullptr) {
        return std::nullopt;  // Explicit no-value state
    }
    entt::entity entity = g_registry->create();
    return static_cast<ecs_entity_t>(entity);
}

// Usage - compiler enforces null check
if (auto entity = ECS_CreateEntity()) {
    // entity.value() is guaranteed valid
    ECS_AddComponent(*entity, ...);
} else {
    Com_Error(ERR_FATAL, "Failed to create entity");
}
```

**Files to Update:**
- `src/common/ecs.cpp` - Entity creation/destruction
- `src/common/ecs_systems.cpp` - System queries
- `src/server/sv_ecs.cpp` - Server-side ECS

**Migration Strategy:**
1. Add new functions with `std::optional` return types
2. Keep old functions for compatibility
3. Mark old functions as deprecated
4. Gradually migrate callers

**Benefits:**
- Compiler-enforced null checks
- No magic sentinel values
- Clear intent in function signatures

---

### 3. Error Handling with std::expected (High Safety, Medium Complexity)

**Current Pattern:**
```cpp
// Manual error codes
VkResult result = qvkCreateComputePipelines(...);
if (result != VK_SUCCESS) {
    ri.Printf(PRINT_WARNING, "Failed: %d\n", result);
    return;  // Silent failure
}
```

**C++23 Improvement:**
```cpp
#include <expected>
#include <string>

enum class VkError {
    Success,
    OutOfMemory,
    InvalidHandle,
    // ...
};

std::expected<VkPipeline, VkError> CreateComputePipeline(...) {
    VkPipeline pipeline;
    VkResult result = qvkCreateComputePipelines(...);
    
    if (result == VK_SUCCESS) {
        return pipeline;
    }
    
    return std::unexpected(VkError::OutOfMemory);
}

// Usage - explicit error handling
auto pipeline = CreateComputePipeline(...);
if (!pipeline) {
    std::print(stderr, "Pipeline creation failed: {}\n", 
               static_cast<int>(pipeline.error()));
    return;
}
// pipeline.value() is guaranteed valid
```

**Files to Update:**
- `src/renderervk/vk.c` - Pipeline creation functions
- `src/renderervk/vk_raytracing.c` - RT resource creation
- `src/renderervk/vk_dlss.c` - DLSS initialization

**Migration Strategy:**
1. Start with internal helper functions
2. Create wrapper functions that return `std::expected`
3. Keep C interface functions unchanged
4. Gradually migrate internal code

**Benefits:**
- Explicit error handling (no silent failures)
- Type-safe error codes
- Compiler-enforced error checking
- Better than exceptions for performance-critical code

---

### 4. String Views for Function Parameters (Low Risk, High Performance)

**Current Pattern:**
```cpp
// C-style string parameters
void R_FindImageFile(const char* name) {
    // No length information, potential for buffer overruns
}
```

**C++23 Improvement:**
```cpp
#include <string_view>

void R_FindImageFile(std::string_view name) {
    // Knows length, no allocation, works with string literals
    // Can safely use name.data() and name.length()
}

// Works with all string types
R_FindImageFile("texture.png");           // string literal
R_FindImageFile(std::string("texture"));  // std::string
R_FindImageFile(charArray);               // char array
```

**Files to Update:**
- `src/common/ecs.cpp` - Component lookup functions
- Any function taking `const char*` for read-only string parameters

**Migration Strategy:**
1. Change function signatures to `std::string_view`
2. Update callers (usually no changes needed - implicit conversion)
3. Keep C interface functions with `const char*` for compatibility

**Benefits:**
- Zero-cost abstraction (no allocations)
- Works with all string types
- Length information available
- Better performance than `std::string` for read-only use

---

### 5. Compile-Time Checks with `if consteval` (Zero Risk)

**Current Pattern:**
```cpp
// Runtime checks that could be compile-time
#define SHADER_MODULE(name) SHADER_MODULE(name,sizeof(name))

static VkShaderModule SHADER_MODULE(const uint8_t *bytes, const int count) {
    if (count % 4 != 0) {  // Runtime check
        ri.Error(ERR_FATAL, "SPIR-V size not multiple of 4");
    }
    // ...
}
```

**C++23 Improvement:**
```cpp
#include <source_location>

template<auto& ShaderArray>
constexpr VkShaderModule CreateShaderModule() {
    if consteval {
        // Compile-time check
        static_assert(sizeof(ShaderArray) % 4 == 0, 
                     "SPIR-V size must be multiple of 4");
    }
    
    // Runtime implementation
    return SHADER_MODULE(ShaderArray, sizeof(ShaderArray));
}
```

**Files to Update:**
- `src/renderervk/vk.c` - Shader module creation
- Any template code with runtime checks

**Benefits:**
- Catch errors at compile-time
- Zero runtime overhead for checks
- Better error messages

---

### 6. Multi-Dimensional Arrays with std::mdspan (Medium Risk, High Clarity)

**Current Pattern:**
```cpp
// Manual indexing
vk.modules.frag.ent[1][0][1] = SHADER_MODULE(frag_pbr_tx0_ent_fog);
```

**C++23 Improvement:**
```cpp
#include <mdspan>

// Define layout
using ShaderArray3D = std::mdspan<VkShaderModule, 
    std::extents<size_t, 2, 2, 2>>;

// Safer access with bounds checking in debug builds
ShaderArray3D fragEnt(vk.modules.frag.ent);
fragEnt[1, 0, 1] = SHADER_MODULE(frag_pbr_tx0_ent_fog);
```

**Files to Update:**
- `src/renderervk/vk.c` - Multi-dimensional shader module arrays
- Any code with manual multi-dimensional indexing

**Benefits:**
- Bounds checking in debug builds
- Clearer intent
- Better compiler optimizations

---

## Implementation Priority

### Phase 1: Zero-Risk Wins (Start Here)
1. ✅ **std::print** for new logging code
2. ✅ **std::string_view** for new function parameters
3. ✅ **if consteval** for compile-time checks

### Phase 2: Type Safety Improvements
1. **std::optional** for nullable return values
2. **std::expected** for error handling in new functions
3. **std::mdspan** for multi-dimensional arrays

### Phase 3: Full Migration
1. Replace all `ri.Printf` with `std::print`
2. Migrate all error-prone functions to `std::expected`
3. Update all string parameters to `std::string_view`

## Compatibility Considerations

### C Interface Compatibility
- **Keep C functions unchanged** - C code can't use C++23 features
- Create C++ wrapper functions with C++23 features
- Gradually migrate internal C++ code

### Build System
- Ensure compiler supports C++23 (GCC 13+, Clang 16+, MSVC 19.30+)
- Add feature detection macros:
  ```cpp
  #if __cpp_lib_print >= 202207L
      #define HAS_STD_PRINT 1
  #else
      #define HAS_STD_PRINT 0
  #endif
  ```

### Performance Impact
- **std::print**: Faster than printf (no runtime parsing)
- **std::string_view**: Zero overhead (no allocations)
- **std::optional**: Minimal overhead (one bool + value)
- **std::expected**: Similar to std::optional

## Example: Safe Migration Pattern

```cpp
// Old code (keep for C compatibility)
extern "C" {
    VkPipeline vk_create_pipeline_old(const char* name) {
        // C interface - keep unchanged
    }
}

// New C++23 code (internal use)
#include <expected>
#include <string_view>
#include <print>

std::expected<VkPipeline, std::string> 
vk_create_pipeline(std::string_view name) {
    if (name.empty()) {
        return std::unexpected("Pipeline name cannot be empty");
    }
    
    VkPipeline pipeline;
    VkResult result = qvkCreateGraphicsPipelines(...);
    
    if (result != VK_SUCCESS) {
        std::print(stderr, "Failed to create pipeline '{}': {}\n", 
                   name, vk_result_string(result));
        return std::unexpected(vk_result_string(result));
    }
    
    return pipeline;
}

// Wrapper for C interface
extern "C" {
    VkPipeline vk_create_pipeline_c(const char* name) {
        auto result = vk_create_pipeline(name);
        if (!result) {
            return VK_NULL_HANDLE;
        }
        return result.value();
    }
}
```

## Testing Strategy

1. **Unit Tests**: Test new C++23 functions independently
2. **Integration Tests**: Verify C interface still works
3. **Performance Tests**: Ensure no regressions
4. **Gradual Rollout**: Migrate one module at a time

## Conclusion

C++23 features can be safely adopted incrementally:
- Start with **std::print** and **std::string_view** (lowest risk)
- Add **std::optional** for new functions
- Use **std::expected** for better error handling
- Keep C interfaces unchanged for compatibility

The key is **gradual migration** - don't break existing code, add new code with modern features, and migrate old code incrementally.

