# C/C++ Boundary Rules

## Overview

The id Tech 3 engine uses a hybrid C/C++ architecture where:
- **Engine core** is primarily C
- **New gamecode** can use C++ (OOP entities, ECS systems)
- **QVM compatibility** requires a stable C ABI
- **Network/save serialization** must use C-compatible layouts

This document defines the rules and patterns for safely crossing the C/C++ boundary.

## Table of Contents

1. [Core Principles](#core-principles)
2. [Function Boundaries](#function-boundaries)
3. [Data Structure Boundaries](#data-structure-boundaries)
4. [Memory Management](#memory-management)
5. [Exception Handling](#exception-handling)
6. [RTTI and Virtual Functions](#rtti-and-virtual-functions)
7. [Examples](#examples)
8. [Best Practices](#best-practices)
9. [Common Pitfalls](#common-pitfalls)

## Core Principles

### 1. Stable C ABI
All functions called from C code (including QVM) must be declared with `extern "C"` and use C-compatible calling conventions.

### 2. POD Structs for Network/Save
Structures used for network snapshots, save games, or shared between C and C++ must be **Plain Old Data (POD)**:
- No virtual functions
- No constructors/destructors (or trivial ones)
- No non-POD members
- Standard layout (no access specifiers affecting layout)
- Trivially copyable

### 3. No Exceptions Across Boundaries
C++ exceptions must never propagate across C boundaries. Catch all exceptions at the boundary and convert to error codes or logging.

### 4. No RTTI Across Boundaries
Runtime Type Information (RTTI) cannot be used across C boundaries. Use explicit type tags or function pointers instead.

## Function Boundaries

### Header Pattern

All headers that may be included from both C and C++ must use this pattern:

```c
#ifndef __MODULE_NAME_H__
#define __MODULE_NAME_H__

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

// C-compatible function declarations
void Module_Init(void);
void Module_Shutdown(void);
qboolean Module_DoSomething(int param);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __MODULE_NAME_H__
```

### Implementation Pattern

In C++ implementation files:

```cpp
#include "module_name.h"

#ifdef __cplusplus
extern "C" {
#endif

void Module_Init(void) {
    // Implementation can use C++ features internally
    // but must be callable from C
}

#ifdef __cplusplus
}
#endif
```

### Example: OOP Bridge

See `mymod/gamesrc/game/g_oop.h`:

```c
#ifdef __cplusplus
extern "C" {
#endif

// C-callable functions for C++ entity system
qboolean G_OOP_CallSpawn(gentity_t *ent, const char *classname);
void G_OOP_RunFrame(int msec);

#ifdef __cplusplus
} // extern "C"
#endif
```

## Data Structure Boundaries

### POD Requirements

Structures used across boundaries must be POD. Examples:

**✅ Valid POD Struct:**
```c
typedef struct {
    int number;
    int eType;
    vec3_t origin;
    vec3_t angles;
    int modelindex;
} entityState_t;  // Used in network snapshots
```

**❌ Invalid (Non-POD):**
```cpp
class EntityState {  // Cannot cross C boundary
    virtual void Serialize();  // Virtual function
    std::string name;  // Non-POD member
};
```

### Network/Save Structs

Critical structures that must remain POD:
- `gentity_t` - Game entity structure
- `playerState_t` - Player state for networking
- `entityState_t` - Entity state for snapshots
- `usercmd_t` - User commands
- `snapshot_t` - Network snapshots

**Rule**: These structures must never contain:
- Virtual functions
- C++ classes with constructors/destructors
- `std::string`, `std::vector`, or other STL containers
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- References

### C++ Wrapper Pattern

When you need C++ features, wrap POD structs:

```cpp
// POD struct (C-compatible)
struct DamageInfo {
    float amount;
    int mod;
    vec3_t direction;
    int sourceEntity;
};

// C++ wrapper (internal use only)
class DamageInfoWrapper {
    DamageInfo info;
    std::string sourceName;  // OK - internal only
    
public:
    DamageInfoWrapper(const DamageInfo& base) : info(base) {}
    const DamageInfo& GetPOD() const { return info; }
};
```

## Memory Management

### Allocator Boundaries

**Rule**: Use the engine's zone allocator (`Z_TagMalloc`/`Z_Free`) for memory that crosses boundaries.

```c
// C side
void* ptr = Z_TagMalloc(size, TAG_GENERAL);

// C++ side (wrapped)
extern "C" void* AllocateEntity(int size) {
    return Z_TagMalloc(size, TAG_ENTITY);
}
```

**Avoid**:
- Mixing `new`/`delete` with `Z_TagMalloc`/`Z_Free` for the same object
- Using `malloc`/`free` for boundary-crossing memory
- STL allocators that don't use zone allocator

### Smart Pointers

Smart pointers (`std::unique_ptr`, `std::shared_ptr`) are fine for **internal C++ code** but cannot cross C boundaries:

```cpp
// ✅ OK - Internal C++ use
class EntityManager {
    std::vector<std::unique_ptr<BaseEntity>> entities;
};

// ❌ BAD - Cannot return to C
extern "C" std::unique_ptr<BaseEntity> CreateEntity();  // WRONG

// ✅ OK - Return raw pointer, manage internally
extern "C" BaseEntity* CreateEntity() {
    auto entity = std::make_unique<BaseEntity>();
    // Store in internal registry
    return entity.release();  // Transfer ownership to registry
}
```

## Exception Handling

### Boundary Exception Handling

**Rule**: Catch all exceptions at C/C++ boundaries and convert to error codes:

```cpp
extern "C" qboolean G_OOP_CallSpawn(gentity_t *ent, const char *classname) {
    try {
        // C++ code that might throw
        return EntityRegistry::Spawn(ent, classname) ? qtrue : qfalse;
    }
    catch (const std::exception& e) {
        Com_Printf("ERROR: Exception in G_OOP_CallSpawn: %s\n", e.what());
        return qfalse;
    }
    catch (...) {
        Com_Printf("ERROR: Unknown exception in G_OOP_CallSpawn\n");
        return qfalse;
    }
}
```

### No-Exception Builds

For gamecode, consider compiling with `-fno-exceptions`:

```cmake
# In CMakeLists.txt
if(BUILD_GAMECODE_CPP)
    target_compile_options(gamecode PRIVATE -fno-exceptions -fno-rtti)
endif()
```

This ensures exceptions cannot be thrown, making boundary safety easier to verify.

## RTTI and Virtual Functions

### Virtual Functions

Virtual functions are **allowed** in C++ code but cannot be called across C boundaries:

```cpp
// ✅ OK - Internal C++ use
class BaseEntity {
public:
    virtual void Think(float dt) = 0;
    virtual void Spawn() = 0;
};

// ❌ BAD - Cannot call virtual from C
extern "C" void Entity_Think(void* entity) {
    static_cast<BaseEntity*>(entity)->Think(0.016f);  // WRONG - virtual call
}

// ✅ OK - Use function pointer or type tag
extern "C" void Entity_Think(void* entity, int entityType) {
    switch (entityType) {
        case ENTITY_DOOR:
            static_cast<DoorEntity*>(entity)->Think(0.016f);
            break;
        // ...
    }
}
```

### Type Information

Use explicit type tags instead of RTTI:

```cpp
enum EntityType {
    ENTITY_DOOR,
    ENTITY_TRIGGER,
    ENTITY_NPC,
    // ...
};

struct EntityHeader {
    EntityType type;
    // ... POD fields
};
```

## Examples

### Example 1: C++ Entity System with C Bridge

**Header (`g_oop.h`):**
```c
#ifdef __cplusplus
extern "C" {
#endif

qboolean G_OOP_CallSpawn(gentity_t *ent, const char *classname);
void G_OOP_RunFrame(int msec);

#ifdef __cplusplus
}
#endif
```

**Implementation (`g_oop.cpp`):**
```cpp
#include "g_oop.h"
#include "g_oop.hpp"  // C++ headers

extern "C" {
    qboolean G_OOP_CallSpawn(gentity_t *ent, const char *classname) {
        try {
            return EntityRegistry::Spawn(ent, classname) ? qtrue : qfalse;
        }
        catch (...) {
            Com_Printf("ERROR: Exception in G_OOP_CallSpawn\n");
            return qfalse;
        }
    }
    
    void G_OOP_RunFrame(int msec) {
        EntitySystem::Update(msec);
    }
}
```

### Example 2: POD Struct for Network

**Network struct (must be POD):**
```c
typedef struct {
    int number;
    vec3_t origin;
    vec3_t angles;
    int modelindex;
    // ... only POD types
} entityState_t;
```

**C++ wrapper (internal use):**
```cpp
class EntityStateWrapper {
    entityState_t state;
    
public:
    EntityStateWrapper(const entityState_t& s) : state(s) {}
    
    // C++ convenience methods
    vec3_t GetOrigin() const { return state.origin; }
    void SetOrigin(const vec3_t& origin) {
        VectorCopy(origin, state.origin);
    }
    
    // Get POD for network serialization
    const entityState_t& GetPOD() const { return state; }
};
```

### Example 3: Error Handling

```cpp
extern "C" qboolean LoadEntityData(const char* filename, void* buffer, int size) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            return qfalse;
        }
        
        file.read(static_cast<char*>(buffer), size);
        return file.good() ? qtrue : qfalse;
    }
    catch (const std::ios_base::failure& e) {
        Com_Printf("ERROR: File I/O error: %s\n", e.what());
        return qfalse;
    }
    catch (...) {
        Com_Printf("ERROR: Unknown error loading entity data\n");
        return qfalse;
    }
}
```

## Best Practices

### 1. Header Guards

Always use include guards and `extern "C"`:

```c
#ifndef __MODULE_H__
#define __MODULE_H__

#ifdef __cplusplus
extern "C" {
#endif

// ... declarations

#ifdef __cplusplus
}
#endif

#endif
```

### 2. Separate C and C++ Headers

For complex modules, consider separate headers:

- `module.h` - C-compatible API
- `module.hpp` - C++-only API (internal use)

### 3. Type Safety

Use strong types for boundary parameters:

```c
// ✅ Good - explicit types
typedef struct {
    int x, y, z;
} vec3i_t;

void SetPosition(vec3i_t pos);

// ❌ Bad - ambiguous
void SetPosition(int x, int y, int z);  // Easy to mix up parameters
```

### 4. Documentation

Document boundary-crossing functions:

```c
/**
 * Spawns an entity using the C++ OOP system.
 * 
 * @param ent Pointer to gentity_t (POD struct, C-compatible)
 * @param classname Entity classname string
 * @return qtrue if spawned successfully, qfalse otherwise
 * 
 * @note This function catches all C++ exceptions and converts to qfalse.
 * @note The gentity_t structure must remain POD-compatible.
 */
extern "C" qboolean G_OOP_CallSpawn(gentity_t *ent, const char *classname);
```

### 5. Testing

Test boundary functions from both C and C++:

```cpp
// C++ test
TEST(cpp_boundary_test) {
    gentity_t ent;
    ASSERT_TRUE(G_OOP_CallSpawn(&ent, "func_door"));
}

// C test (if possible)
void test_c_boundary(void) {
    gentity_t ent;
    qboolean result = G_OOP_CallSpawn(&ent, "func_door");
    assert(result == qtrue);
}
```

## Common Pitfalls

### ❌ Pitfall 1: Virtual Functions in POD Structs

```cpp
// WRONG
struct EntityState {
    virtual void Serialize();  // Makes struct non-POD
};
```

**Fix**: Use function pointers or separate serialization functions.

### ❌ Pitfall 2: Exceptions Crossing Boundaries

```cpp
// WRONG
extern "C" void DoSomething() {
    throw std::runtime_error("error");  // Exception crosses boundary
}
```

**Fix**: Catch exceptions at boundary:

```cpp
extern "C" void DoSomething() {
    try {
        // ... code that might throw
    }
    catch (...) {
        Com_Printf("ERROR: Exception caught\n");
    }
}
```

### ❌ Pitfall 3: STL Containers in POD Structs

```cpp
// WRONG
struct NetworkPacket {
    std::vector<char> data;  // Non-POD member
};
```

**Fix**: Use C arrays or pointers:

```c
struct NetworkPacket {
    char data[MAX_PACKET_SIZE];
    int size;
};
```

### ❌ Pitfall 4: Mixing Allocators

```cpp
// WRONG
extern "C" void* CreateEntity() {
    return new Entity();  // Uses new/delete, not zone allocator
}
```

**Fix**: Use zone allocator:

```cpp
extern "C" void* CreateEntity() {
    return Z_TagMalloc(sizeof(Entity), TAG_ENTITY);
}
```

### ❌ Pitfall 5: Missing extern "C"

```cpp
// WRONG - Missing extern "C"
void Module_Init(void);  // C++ name mangling breaks C calls
```

**Fix**: Use extern "C":

```cpp
#ifdef __cplusplus
extern "C" {
#endif
void Module_Init(void);
#ifdef __cplusplus
}
#endif
```

## Compiler Flags

Recommended flags for C++ gamecode:

```cmake
# Disable exceptions and RTTI for boundary safety
target_compile_options(gamecode PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
)

# Ensure standard layout
target_compile_definitions(gamecode PRIVATE
    -DUSE_STD_LAYOUT=1
)
```

## Verification

### Static Analysis

Use `static_assert` to verify POD requirements:

```cpp
#include <type_traits>

static_assert(std::is_pod_v<entityState_t>, 
    "entityState_t must be POD for network compatibility");
static_assert(std::is_trivially_copyable_v<gentity_t>,
    "gentity_t must be trivially copyable");
```

### Runtime Checks

Add runtime validation in debug builds:

```cpp
#ifdef _DEBUG
void ValidatePODStruct(const void* ptr, size_t size) {
    // Check alignment, padding, etc.
    assert(reinterpret_cast<uintptr_t>(ptr) % alignof(max_align_t) == 0);
}
#endif
```

## Related Documentation

- [Entity OOP Plan](entity_oop_plan.md) - OOP entity system design
- [QVM Compatibility](QVM_COMPATIBILITY.md) - QVM-specific considerations
- [Modern C++ Features](MODERN_CPP_FEATURES.md) - C++ features usage guidelines

## Summary

**Key Rules:**
1. ✅ Use `extern "C"` for all C-callable functions
2. ✅ Keep network/save structs as POD
3. ✅ Catch exceptions at boundaries
4. ✅ Use zone allocator for boundary memory
5. ✅ Avoid RTTI/virtual functions across boundaries
6. ✅ Document boundary functions clearly
7. ✅ Test from both C and C++ sides

Following these rules ensures safe, maintainable C/C++ interoperation while preserving QVM compatibility and network protocol stability.
