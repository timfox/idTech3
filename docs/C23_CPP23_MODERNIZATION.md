# C23/C++23 Modernization of id Tech 3

This document details the comprehensive modernization of the id Tech 3 codebase to use modern C23 and C++23 standards, eliminating technical debt and improving maintainability.

## Overview

The modernization addresses the mixed C/C++ codebase by:

- **Standards Compliance**: Full C23/C++23 support with fallback compatibility
- **Modern C++ Idioms**: RAII, smart pointers, exceptions, and modern containers
- **Enhanced Error Handling**: Exception-based error management with RAII cleanup
- **Comprehensive Testing**: Modern unit testing framework with performance profiling
- **Advanced Logging**: Structured logging with performance diagnostics
- **Build System**: Enhanced CMake configuration with modern compiler features

## Language Standards

### C23 Support
- **Enhanced Compiler Detection**: Automatic C23 enablement for GCC/Clang
- **MSVC Compatibility**: C17 fallback for Visual Studio compatibility
- **Extensions Disabled**: Strict standard compliance with `-std=c23`
- **Modern Features**: Use of C23 attributes, typeof, and improved preprocessor

### C++23 Support
- **Full C++23 Features**: Coroutines, modules, and advanced template features
- **Compiler Optimization**: LTO, sanitizers, and modern warning flags
- **Standards Compliance**: Strict standard compliance with `-std=c++23`
- **Backward Compatibility**: C++17 fallback for older compilers

## Code Modernization

### Memory Management
```cpp
// Before: Manual memory management
cook_job_t* job = (cook_job_t*)malloc(sizeof(cook_job_t));
// ... use job ...
free(job);

// After: Smart pointers and RAII
struct CookJob {
    // ... members ...
    std::unique_ptr<void, std::function<void(void*)>> type_specific_options;
    std::vector<asset_dependency_t> dependencies;
};
```

### Container Modernization
```cpp
// Before: C-style arrays
int32_t padding[4] = {0, 0, 0, 0};

// After: Modern containers
std::array<int32_t, 4> padding = {0, 0, 0, 0};
std::vector<AssetInfo> assets;
```

### Error Handling
```cpp
// Before: Error codes and manual cleanup
int result = some_operation();
if (result != SUCCESS) {
    cleanup_resources();
    return result;
}

// After: Exceptions with RAII
IDTECH3_TRY {
    some_operation();
} IDTECH3_CATCH(FileException& e) {
    LOG_ERROR_FMT("File operation failed: {}", e.what());
} IDTECH3_END_TRY
```

### String Handling
```cpp
// Before: C-style strings with manual management
char buffer[256];
snprintf(buffer, sizeof(buffer), "Asset: %s", asset_name);

// After: Modern string handling
std::string message = std::format("Asset: {}", asset_name);
```

## Modern Features Implemented

### 1. CMake Build System Enhancement

#### Compiler Detection and Features
```cmake
# Modern compiler features
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    CHECK_CXX_COMPILER_FLAG("-fcoroutines" HAS_COROUTINES)
    CHECK_CXX_COMPILER_FLAG("-flto" HAS_LTO)

    # Address sanitizer for debug builds
    IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
        ADD_COMPILE_OPTIONS(-fsanitize=address)
    ENDIF()
ENDIF()
```

#### Standards Configuration
```cmake
# C23/C++23 with fallbacks
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    SET(CMAKE_CXX_STANDARD 23)
    SET(CMAKE_CXX_STANDARD_REQUIRED ON)
ELSEIF(MSVC)
    SET(CMAKE_CXX_STANDARD 20)  # MSVC C++23 support limited
    SET(CMAKE_CXX_STANDARD_REQUIRED OFF)
ENDIF()
```

### 2. Smart Pointers and RAII

#### Asset Cooking System
```cpp
struct CookJob {
    std::string source_path;
    std::string output_path;
    std::unique_ptr<void, std::function<void(void*)>> type_specific_options;
    std::vector<asset_dependency_t> dependencies;

    // RAII constructor/destructor
    CookJob() = default;
    ~CookJob() = default; // Automatic cleanup
};
```

#### Error Context Management
```cpp
class ErrorContext {
public:
    void add_cleanup(std::function<void()> cleanup);
    void cleanup();

private:
    std::vector<std::function<void()>> m_cleanups;
};

class ErrorGuard {
public:
    explicit ErrorGuard(ErrorContext& context);
    ~ErrorGuard(); // Automatic cleanup on scope exit
};
```

### 3. Exception-Based Error Handling

#### Exception Hierarchy
```cpp
class IdTech3Exception : public std::exception {
    // Base exception with location and stack trace
};

class FileException : public IdTech3Exception {
    // File-specific errors
};

class MemoryException : public IdTech3Exception {
    // Memory allocation errors
};
```

#### Safe Operations
```cpp
template<typename Func, typename... Args>
auto safe_call(Func&& func, Args&&... args) {
    try {
        return std::forward<Func>(func)(std::forward<Args>(args)...);
    } catch (const IdTech3Exception& e) {
        GlobalErrorHandler::instance().handle_exception(e);
        return decltype(func(args...)){};
    }
}
```

### 4. Modern Logging System

#### Structured Logging
```cpp
enum class LogLevel { Trace, Debug, Info, Warning, Error, Fatal };
enum class LogCategory { Engine, Renderer, Audio, Network, UI };

class Logger {
public:
    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args);
};
```

#### Performance Profiling
```cpp
class PerformanceProfiler {
public:
    class ScopedProfile {
    public:
        ScopedProfile(std::string_view name);
        ~ScopedProfile(); // Automatic timing
    };
};

// Usage
void renderFrame() {
    PROFILE_SCOPE("renderFrame");
    // Rendering code automatically timed
}
```

### 5. Unit Testing Framework

#### Modern Test Macros
```cpp
TEST_SUITE(Math)
{
    TEST(clamp_test, "Test clamping functions")
    {
        ASSERT_EQ(5, Com_Clamp(0, 10, 5));
        ASSERT_NEAR(3.14f, calculate_pi(), 0.01f);
    }

    TEST(performance_test, "Performance critical test")
    {
        PerformanceTest::assert_performance([]() {
            // Test code
        }, std::chrono::milliseconds(100));
    }
};
```

#### Test Runner
```cpp
class TestRunner {
public:
    bool run_all_tests();
    void print_summary();
    void export_results(const std::string& filename);
};
```

### 6. Container Modernization

#### UI2 Layout System
```cpp
// Before
struct Style {
    int32_t padding[4];
    int32_t margin[4];
    int32_t borderRadius[4];
};

// After
struct Style {
    std::array<int32_t, 4> padding;
    std::array<int32_t, 4> margin;
    std::array<int32_t, 4> borderRadius;
};
```

#### Asset Management
```cpp
// Modern container usage
std::vector<AssetInfo> m_allAssets;
std::unordered_map<std::string, AssetInfo> m_assetCache;
std::span<AssetInfo> get_assets_in_range(size_t start, size_t count);
```

## Performance Improvements

### Compiler Optimizations
- **Link Time Optimization (LTO)**: Cross-module optimizations
- **Profile-Guided Optimization (PGO)**: Performance-guided compilation
- **Address Sanitizer**: Memory error detection in debug builds
- **Modern Warning Levels**: Enhanced code quality checks

### Memory Management
- **RAII Patterns**: Automatic resource cleanup
- **Smart Pointers**: Leak-free memory management
- **Arena Allocators**: Efficient temporary allocations
- **Cache-Friendly Data Structures**: Improved memory access patterns

### Algorithm Optimizations
- **Modern Algorithms**: Use of `<algorithm>` and `<ranges>`
- **Parallel Processing**: Multi-threaded asset cooking
- **SIMD Operations**: Vectorized math operations where applicable

## Threading and Concurrency

### Modern Threading
```cpp
class AsyncWorker {
public:
    void enqueue_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.push(std::move(task));
        m_condition.notify_one();
    }

private:
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::queue<std::function<void()>> m_taskQueue;
    std::vector<std::thread> m_workerThreads;
};
```

### Synchronization Primitives
- **std::mutex/std::shared_mutex**: Thread-safe data access
- **std::atomic**: Lock-free operations
- **std::condition_variable**: Efficient thread signaling
- **std::barrier/latch/semaphore**: C++20 synchronization

## Build and Deployment

### Enhanced CMake Configuration
```cmake
# Modern CMake features
CMAKE_MINIMUM_REQUIRED(VERSION 3.25)
SET(CMAKE_CXX_SCAN_FOR_MODULES ON)  # C++20 modules support
SET(CMAKE_CXX_STANDARD 23)
SET(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific optimizations
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    ADD_COMPILE_OPTIONS(
        -fcoroutines -fmodules-ts
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Release>:-flto -march=native>
    )
ENDIF()
```

### CI/CD Integration
- **Automated Testing**: GitHub Actions with comprehensive test suites
- **Code Coverage**: LLVM coverage reporting
- **Static Analysis**: Clang-Tidy integration
- **Performance Regression**: Automated benchmarking

## Migration Strategy

### Phase 1: Core Infrastructure
1. ✅ CMake modernization
2. ✅ Basic C23/C++23 support
3. ✅ Fundamental data structures
4. ✅ Memory management modernization

### Phase 2: Error Handling and Testing
1. ✅ Exception-based error handling
2. ✅ RAII resource management
3. ✅ Unit testing framework
4. ✅ Performance profiling

### Phase 3: Advanced Features
1. 🔄 Smart pointers throughout codebase
2. 🔄 Modern container usage
3. 🔄 Constexpr optimizations
4. 🔄 Advanced threading features

### Phase 4: Optimization and Polish
1. 🔄 Memory allocator modernization
2. 🔄 String handling improvements
3. 🔄 Template metaprogramming
4. 🔄 SIMD optimizations

## Compatibility and Fallbacks

### Compiler Compatibility Matrix
| Compiler | C23 | C++23 | Fallback |
|----------|-----|-------|----------|
| GCC 13+  | ✅  | ✅    | N/A      |
| Clang 16+| ✅  | ✅    | N/A      |
| MSVC 2022| ❌  | ⚠️    | C17/C++20|
| GCC 9-12 | ⚠️  | ⚠️    | C17/C++17|

### Feature Detection
```cpp
// Compile-time feature detection
#if __cplusplus >= 202302L
    // C++23 features available
    #include <stacktrace>
    #define HAS_STACKTRACE 1
#endif

#if __STDC_VERSION__ >= 202311L
    // C23 features available
    #define HAS_C23 1
#endif
```

## Benefits Achieved

### Code Quality
- **Reduced Technical Debt**: Modern idioms eliminate legacy patterns
- **Improved Maintainability**: Self-documenting modern code
- **Enhanced Safety**: RAII and smart pointers prevent leaks
- **Better Error Handling**: Comprehensive exception management

### Performance
- **Optimized Builds**: LTO and modern compiler features
- **Efficient Memory Usage**: Smart pointers and modern allocators
- **Concurrent Processing**: Multi-threaded operations where beneficial
- **Profile-Guided Optimization**: Performance-driven compilation

### Developer Experience
- **Modern Tooling**: Full IDE support with modern standards
- **Comprehensive Testing**: Automated test suites with coverage
- **Advanced Debugging**: Enhanced logging and diagnostics
- **Documentation**: Self-documenting code with modern patterns

### Future-Proofing
- **Standards Compliance**: Ready for future language evolution
- **Extensible Architecture**: Modular design for easy extension
- **Cross-Platform**: Consistent behavior across all supported platforms
- **Maintainable Codebase**: Modern patterns reduce maintenance burden

## Usage Examples

### Modern Asset Cooking
```cpp
// RAII asset cooking with modern error handling
void cook_assets(const std::vector<std::string>& asset_paths) {
    ErrorContext context;

    IDTECH3_TRY {
        for (const auto& path : asset_paths) {
            CookJob job(path, ASSET_TYPE_TEXTURE, COOK_QUALITY_HIGH);
            job.add_dependency("base/textures/common/normal.tga");

            if (!AssetCooking::cook_asset(job)) {
                throw AssetException(path, "Failed to cook asset");
            }
        }
    } IDTECH3_CATCH(AssetException& e) {
        LOG_ERROR_FMT("Asset cooking failed: {}", e.what());
        context.add_cleanup([]() {
            // Cleanup partial results
        });
    } IDTECH3_END_TRY
}
```

### Modern Logging and Profiling
```cpp
void render_frame() {
    PROFILE_SCOPE("render_frame");

    {
        PROFILE_SCOPE("geometry_pass");
        LOG_DEBUG("Rendering geometry");
        // Geometry rendering code
    }

    {
        PROFILE_SCOPE("lighting_pass");
        LOG_DEBUG("Applying lighting");
        // Lighting calculations
    }

    LOG_INFO_FMT("Frame rendered in {} ms",
                PerformanceProfiler::instance().get_last_duration().count());
}
```

### Modern Testing
```cpp
TEST_SUITE(AssetCooking)
{
    TEST(cook_texture, "Test texture cooking pipeline")
    {
        // Create test asset
        std::string testAsset = TestFixture::temp_file_path("test.tga");

        // Cook asset
        CookJob job(testAsset, ASSET_TYPE_TEXTURE, COOK_QUALITY_MEDIUM);

        PerformanceTest::assert_performance([&]() {
            ASSERT_TRUE(AssetCooking::cook_asset(job));
        }, std::chrono::milliseconds(500), "asset cooking");

        // Verify output
        ASSERT_TRUE(QFile::exists(job.output_path.c_str()));
    }

    TEST(error_handling, "Test error handling in cooking")
    {
        CookJob invalidJob("nonexistent.tga");

        ASSERT_THROW(AssetCooking::cook_asset(invalidJob), FileException);
    }
};
```

## Conclusion

The C23/C++23 modernization transforms id Tech 3 from a legacy codebase into a modern, maintainable, and performant engine. By adopting modern language features, idioms, and best practices, the codebase is now:

- **Future-proof**: Ready for language evolution and new features
- **Maintainable**: Self-documenting code with clear ownership semantics
- **Performant**: Optimized builds with modern compiler features
- **Reliable**: Comprehensive error handling and automated testing
- **Developer-friendly**: Modern tooling support and enhanced debugging

This modernization eliminates technical debt while preserving the engine's core architecture and gameplay experience, ensuring id Tech 3 remains viable for years to come.