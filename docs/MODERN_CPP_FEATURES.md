# Modern C/C++ Features Adoption

This document outlines the adoption of modern C/C++ language features in the id Tech 3 codebase, improving code quality, safety, and maintainability.

## Adopted Features

### C23 Features

#### 1. `nullptr` instead of `NULL`
- **Files Modified**: `src/qcommon/common.c`
- **Rationale**: `nullptr` provides better type safety than `NULL`
- **Compatibility**: C23 supports `nullptr` natively
- **Examples**:
  ```c
  // Before
  rd_buffer = NULL;
  return NULL;

  // After
  rd_buffer = nullptr;
  return nullptr;
  ```

#### 2. Designated Initializers
- **Files Modified**: `src/qcommon/vm.c`
- **Rationale**: Designated initializers improve code readability and maintainability
- **Examples**:
  ```c
  // Before
  { 4, -8, 2, JUMP }, // eq

  // After
  { .size = 4, .stack = -8, .nargs = 2, .flags = JUMP }, // eq
  ```

#### 3. Static Assertions
- **Files Modified**: `src/qcommon/q_shared.h`, `src/qcommon/qcommon.h`
- **Rationale**: Compile-time verification of critical assumptions
- **Examples**:
  ```c
  // Structure size verification
  static_assert(sizeof(vec3_t) == 12, "vec3_t must be 12 bytes for network compatibility");

  // Protocol version ordering
  static_assert(PROTOCOL_VERSION_66 < PROTOCOL_VERSION_67, "Protocol versions must be sequential");
  ```

### C++23 Features

#### 1. `auto` Type Deduction
- **Status**: Already in use throughout Qt code
- **Examples**:
  ```cpp
  auto* layout = new QVBoxLayout(&dlg);
  auto* button = new QPushButton("OK", &dlg);
  ```

#### 2. Range-based Initialization
- **Status**: Already in use throughout Qt code
- **Examples**:
  ```cpp
  const QStringList exts = {QStringLiteral("*.tga"), QStringLiteral("*.jpg")};
  const char* modeNames[] = {"None", "Box", "Gizmo"};
  ```

#### 3. Standard Library Usage
- **Status**: Already using modern STL features
- **Examples**:
  ```cpp
  const int w = std::max(1, width());
  snapped.setX(std::round(snapped.x() / grid) * grid);
  ```

## Planned Features

### C23 Features to Adopt

#### 1. `_Generic` Selections
- **Use Case**: Type-generic programming for math utilities
- **Example**:
  ```c
  #define min(a, b) _Generic((a), \
      int: min_int, \
      float: min_float, \
      double: min_double)(a, b)
  ```

#### 2. `typeof` Operator
- **Use Case**: Generic macro definitions
- **Example**:
  ```c
  #define SWAP(a, b) do { \
      typeof(a) temp = a; \
      a = b; \
      b = temp; \
  } while(0)
  ```

#### 3. Binary Literals
- **Use Case**: Bit flag definitions
- **Example**:
  ```c
  #define FLAG_READ    0b0001
  #define FLAG_WRITE   0b0010
  #define FLAG_EXECUTE 0b0100
  ```

### C++23 Features to Adopt

#### 1. `constexpr` Functions
- **Use Case**: Compile-time computation for configuration
- **Example**:
  ```cpp
  constexpr int CalculateBufferSize(int elements, int elementSize) {
      return elements * elementSize + 16; // Header overhead
  }
  ```

#### 2. Smart Pointers
- **Use Case**: Automatic memory management in Qt code
- **Example**:
  ```cpp
  auto dialog = std::make_unique<QDialog>(this);
  auto layout = std::make_unique<QVBoxLayout>(dialog.get());
  ```

#### 3. Range-based `for` Loops
- **Use Case**: Container iteration
- **Example**:
  ```cpp
  for (const auto& item : itemList) {
      processItem(item);
  }
  ```

#### 4. Lambda Expressions
- **Use Case**: Event handlers and algorithms
- **Example**:
  ```cpp
  connect(button, &QPushButton::clicked, [this]() {
      handleButtonClick();
  });
  ```

## Compiler Requirements

### Current Requirements
- **C Standard**: C23 (ISO/IEC 9899:2023)
- **C++ Standard**: C++23 (ISO/IEC 14882:2023)
- **Compiler Support**:
  - GCC 15.0+ (C23, C++23)
  - Clang 18.0+ (C23, C++23)
  - MSVC 19.40+ (partial C++23)

### Feature Detection
```cmake
# C23 feature detection
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-std=c2x)  # C23 draft
endif()

# C++23 feature detection
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

## Migration Strategy

### Phase 1: Core Adoption (Completed)
- [x] `nullptr` in C code
- [x] Designated initializers
- [x] Static assertions
- [x] Modern Qt/C++ features

### Phase 2: Advanced Features (Planned)
- [ ] `_Generic` selections for type-generic utilities
- [ ] `constexpr` functions where applicable
- [ ] Smart pointers in Qt code
- [ ] Enhanced lambda usage

### Phase 3: Optimization (Future)
- [ ] Compile-time computation for configuration
- [ ] Template metaprogramming where beneficial
- [ ] Advanced C++23 features as compiler support matures

## Compatibility Considerations

### Backward Compatibility
- **QVM Interface**: Maintains C ABI compatibility
- **Network Protocol**: No changes to wire formats
- **Save Files**: No changes to persistent data structures

### Compiler Compatibility
- **Legacy Builds**: CMake options to disable modern features if needed
- **Fallback Code**: Provide C89/C++11 fallbacks where critical
- **Feature Detection**: Runtime detection of available features

## Benefits

### Code Quality
- **Type Safety**: `nullptr` prevents NULL pointer confusion
- **Clarity**: Designated initializers self-document structure fields
- **Correctness**: Static assertions catch structural issues at compile time

### Maintainability
- **Readability**: Modern syntax reduces boilerplate
- **Consistency**: Standardized initialization patterns
- **Future-Proofing**: Foundation for further modernization

### Performance
- **Compile-time Verification**: Static assertions prevent runtime errors
- **Optimization**: Modern features enable better compiler optimizations
- **Debugging**: Better error messages and debugging information

## Testing and Validation

### Feature Testing
```c
// Test nullptr support
void *ptr = nullptr;
assert(ptr == NULL);  // Should work in C23

// Test designated initializers
struct Test { int a, b, c; };
struct Test t = { .a = 1, .b = 2, .c = 3 };

// Test static assertions
static_assert(sizeof(int) >= 4, "int must be at least 32-bit");
```

### CI Integration
```yaml
# Test modern features
- name: Test C23 features
  run: |
    gcc -std=c2x -c test_nullptr.c
    gcc -std=c2x -c test_designated.c

- name: Test C++23 features
  run: |
    g++ -std=c++23 -c test_auto.cpp
    g++ -std=c++23 -c test_constexpr.cpp
```

## Documentation

### Developer Guidelines
1. **Use `nullptr`** instead of `NULL` in new C code
2. **Use designated initializers** for structure initialization
3. **Add static assertions** for critical size/alignment requirements
4. **Use modern C++ features** in Qt code where appropriate

### Code Review Checklist
- [ ] `nullptr` used instead of `NULL` in C code
- [ ] Designated initializers used for clarity
- [ ] Static assertions added for critical constraints
- [ ] Modern C++ features used appropriately
- [ ] Compiler compatibility maintained

This modernization effort brings id Tech 3's codebase in line with contemporary C/C++ best practices while maintaining the stability and compatibility required for a mature game engine.