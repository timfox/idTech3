<?php
/**
 * Modern C++ Features for id Tech 3
 */
$title = 'Modern C++ Features - id Tech 3 Documentation';
$breadcrumbs = [
    '/modernization' => 'Modernization',
    '/modernization/modern-cpp' => 'Modern C++ Features'
];
?>

<h1>Modern C++ Features for id Tech 3</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modernizing id Tech 3 with C++17/20 features while maintaining compatibility and performance. This guide covers practical implementations of modern C++ features that can improve code quality, performance, and maintainability.</p>
    
    <div class="feature-list">
        <h3>Key Benefits</h3>
        <ul>
            <li><strong>Type Safety:</strong> Better compile-time error detection</li>
            <li><strong>Performance:</strong> Zero-cost abstractions and optimizations</li>
            <li><strong>Maintainability:</strong> Cleaner, more expressive code</li>
            <li><strong>Memory Safety:</strong> Smart pointers and RAII</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>C++17 Features Implementation</h2>
    
    <h3>std::optional for Safe Value Handling</h3>
    <div class="code-block">
        <pre><code>// Before: Error-prone texture loading
texture_t* R_FindImageFile(const char* name) {
    // Returns nullptr on failure - easy to forget null checks
    for (int i = 0; i < tr.numTextures; i++) {
        if (!Q_stricmp(tr.textures[i].imgName, name)) {
            return &tr.textures[i];
        }
    }
    return nullptr; // Easily forgotten null check
}

// After: Using std::optional
#include <optional>

std::optional<texture_t&> R_FindImageFile(const char* name) {
    for (int i = 0; i < tr.numTextures; i++) {
        if (!Q_stricmp(tr.textures[i].imgName, name)) {
            return tr.textures[i];
        }
    }
    return std::nullopt; // Explicit no-value state
}

// Usage
if (auto tex = R_FindImageFile("textures/common/caulk")) {
    // tex.value() is guaranteed to be valid
    tex->width = 512;
} else {
    Com_Printf("Texture not found\n");
}</code></pre>
    </div>
    
    <h3>Structured Bindings for Multiple Returns</h3>
    <div class="code-block">
        <pre><code>// Before: Multiple output parameters
void R_GetScreenSize(int* width, int* height) {
    *width = glConfig.vidWidth;
    *height = glConfig.vidHeight;
}

// After: Structured bindings with std::pair/tuple
std::pair<int, int> R_GetScreenSize() {
    return {glConfig.vidWidth, glConfig.vidHeight};
}

// Usage
auto [width, height] = R_GetScreenSize();
Com_Printf("Screen: %dx%d\n", width, height);

// More complex example: Parsing command line
std::tuple<bool, std::string, int> ParseCommand(const char* cmd) {
    // Parse command and return success, command name, and argument count
    return {true, "map", 1};
}

auto [success, command, argCount] = ParseCommand(Cmd_Argv(0));</code></pre>
    </div>
    
    <h3>if constexpr for Template Optimization</h3>
    <div class="code-block">
        <pre><code>// Template-based renderer selection
template<RendererType Type>
void R_DrawScene() {
    if constexpr (Type == RendererType::Vulkan) {
        // Vulkan-specific code - compiled only for Vulkan
        VK_BeginRenderPass();
        VK_DrawPrimitives();
        VK_EndRenderPass();
    } else if constexpr (Type == RendererType::OpenGL) {
        // OpenGL-specific code - compiled only for OpenGL
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        GL_DrawPrimitives();
        glFlush();
    }
    // No runtime branching - optimized at compile time
}

// Usage
R_DrawScene<RendererType::Vulkan>();</code></pre>
    </div>
    
    <h3>std::string_view for String Performance</h3>
    <div class="code-block">
        <pre><code>// Before: Lots of string copying
bool Cmd_CheckCommand(const char* command) {
    std::string cmd = command; // Unnecessary copy
    return cmd == "map" || cmd == "vid_restart";
}

// After: std::string_view for zero-copy string operations
#include <string_view>

bool Cmd_CheckCommand(std::string_view command) {
    // No copying, just views into existing memory
    return command == "map" || command == "vid_restart";
}

// Substring operations without allocation
std::string_view GetFileExtension(std::string_view filename) {
    auto pos = filename.find_last_of('.');
    return pos != std::string_view::npos ? filename.substr(pos) : "";
}

// Usage
std::string_view ext = GetFileExtension("textures/wall.tga"); // ".tga"</code></pre>
    </div>
</div>

<div class="section">
    <h2>C++20 Features Implementation</h2>
    
    <h3>Concepts for Type Safety</h3>
    <div class="code-block">
        <pre><code>#include <concepts>

// Define concepts for engine types
template<typename T>
concept Renderable = requires(T t) {
    t.Draw();
    t.GetBounds();
    { t.IsVisible() } -> std::convertible_to<bool>;
};

template<typename T>
concept Resource = requires(T t) {
    { t.Load() } -> std::convertible_to<bool>;
    t.Unload();
    { t.IsLoaded() } -> std::convertible_to<bool>;
};

// Template functions with concept constraints
template<Renderable T>
void R_AddToScene(T& object) {
    if (object.IsVisible()) {
        renderQueue.push_back(&object);
    }
}

template<Resource T>
bool ResourceManager_Load(T& resource) {
    if (!resource.IsLoaded()) {
        return resource.Load();
    }
    return true;
}</code></pre>
    </div>
    
    <h3>Ranges for Cleaner Algorithms</h3>
    <div class="code-block">
        <pre><code>#include <ranges>
#include <algorithm>

// Before: Manual loops for entity processing
void R_CullEntities() {
    for (int i = 0; i < tr.refdef.num_entities; i++) {
        refEntity_t* ent = &tr.refdef.entities[i];
        if (R_CullBox(ent->bounds[0], ent->bounds[1])) {
            ent->flags |= RF_CULLED;
        }
    }
}

// After: Using ranges
void R_CullEntities() {
    auto entities = std::span(tr.refdef.entities, tr.refdef.num_entities);
    
    std::ranges::for_each(entities, [](refEntity_t& ent) {
        if (R_CullBox(ent.bounds[0], ent.bounds[1])) {
            ent.flags |= RF_CULLED;
        }
    });
}

// Complex filtering and transforming
void R_ProcessVisibleEntities() {
    auto entities = std::span(tr.refdef.entities, tr.refdef.num_entities);
    
    auto visibleEntities = entities 
        | std::views::filter([](const refEntity_t& ent) {
            return !(ent.flags & RF_CULLED);
          })
        | std::views::transform([](const refEntity_t& ent) {
            return &ent; // Convert to pointers for rendering
          });
    
    for (const refEntity_t* ent : visibleEntities) {
        R_AddEntityToScene(ent);
    }
}</code></pre>
    </div>
    
    <h3>Coroutines for Async Operations</h3>
    <div class="code-block">
        <pre><code>#include <coroutine>

// Async texture loading coroutine
struct TextureLoader {
    struct promise_type {
        TextureLoader get_return_object() { 
            return TextureLoader{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
    
    std::coroutine_handle<promise_type> coro;
    TextureLoader(std::coroutine_handle<promise_type> h) : coro(h) {}
};

// Async texture loading
TextureLoader LoadTextureAsync(const char* filename) {
    // Start loading in background thread
    auto future = std::async(std::launch::async, [filename]() {
        return R_LoadImageFile(filename);
    });
    
    // Yield control while loading
    while (future.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
        co_await std::suspend_always{};
    }
    
    // Texture loaded, update GPU
    texture_t* tex = future.get();
    R_UploadTexture(tex);
    
    co_return;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Smart Pointers and RAII</h2>
    
    <h3>Resource Management</h3>
    <div class="code-block">
        <pre><code>#include <memory>

// Before: Manual resource management
typedef struct {
    byte* data;
    int size;
} buffer_t;

buffer_t* Buffer_Alloc(int size) {
    buffer_t* buf = malloc(sizeof(buffer_t));
    buf->data = malloc(size);
    buf->size = size;
    return buf; // Caller must remember to free
}

void Buffer_Free(buffer_t* buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

// After: RAII with smart pointers
class Buffer {
private:
    std::unique_ptr<byte[]> data_;
    size_t size_;
    
public:
    Buffer(size_t size) : size_(size), data_(std::make_unique<byte[]>(size)) {}
    
    // Move semantics for efficient transfers
    Buffer(Buffer&& other) noexcept 
        : size_(other.size_), data_(std::move(other.data_)) {
        other.size_ = 0;
    }
    
    byte* data() { return data_.get(); }
    size_t size() const { return size_; }
    
    // Automatic cleanup in destructor
    ~Buffer() = default; // smart pointer handles cleanup
};

// Factory function
std::unique_ptr<Buffer> CreateBuffer(size_t size) {
    return std::make_unique<Buffer>(size);
}</code></pre>
    </div>
    
    <h3>Shared Resources</h3>
    <div class="code-block">
        <pre><code>// Shared texture management
class Texture {
private:
    GLuint textureId_;
    int width_, height_;
    
public:
    Texture(GLuint id, int w, int h) : textureId_(id), width_(w), height_(h) {}
    
    ~Texture() {
        if (textureId_) {
            glDeleteTextures(1, &textureId_);
        }
    }
    
    GLuint id() const { return textureId_; }
    int width() const { return width_; }
    int height() const { return height_; }
};

// Shared ownership for textures used by multiple materials
using TexturePtr = std::shared_ptr<Texture>;

class Material {
private:
    TexturePtr diffuseTexture_;
    TexturePtr normalTexture_;
    
public:
    void SetDiffuseTexture(TexturePtr tex) { diffuseTexture_ = tex; }
    void SetNormalTexture(TexturePtr tex) { normalTexture_ = tex; }
    
    // Textures automatically cleaned up when no materials reference them
};</code></pre>
    </div>
</div>

<div class="section">
    <h2>Compiler Requirements</h2>
    
    <h3>Minimum Compiler Versions</h3>
    <div class="code-block">
        <pre><code># CMakeLists.txt - Set C++ standard
cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler version requirements:
# GCC 10+     - Full C++20 support
# Clang 12+   - Full C++20 support  
# MSVC 19.29+ - Visual Studio 2019 16.11+

# Feature detection
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++20" COMPILER_SUPPORTS_CXX20)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)

if(COMPILER_SUPPORTS_CXX20)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
    add_definitions(-DUSE_CXX20=1)
elseif(COMPILER_SUPPORTS_CXX17)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    add_definitions(-DUSE_CXX17=1)
else()
    message(FATAL_ERROR "Compiler lacks C++17 support")
endif()</code></pre>
    </div>
    
    <h3>Conditional Compilation</h3>
    <div class="code-block">
        <pre><code>// Feature detection macros
#ifdef __cpp_lib_optional
    #define HAS_STD_OPTIONAL 1
#else
    #define HAS_STD_OPTIONAL 0
#endif

#ifdef __cpp_concepts
    #define HAS_CONCEPTS 1
#else
    #define HAS_CONCEPTS 0
#endif

// Conditional implementation
#if HAS_STD_OPTIONAL
    #include <optional>
    using OptionalTexture = std::optional<texture_t>;
#else
    // Fallback implementation
    class OptionalTexture {
        bool hasValue_;
        texture_t value_;
    public:
        // Custom optional implementation
    };
#endif</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    
    <h3>Zero-Cost Abstractions</h3>
    <div class="troubleshooting">
        <h4>Template Instantiation</h4>
        <ul>
            <li>Use extern templates to reduce compilation time</li>
            <li>Prefer constexpr over runtime branching</li>
            <li>Use concepts to provide better error messages</li>
        </ul>
        
        <h4>Memory Allocation</h4>
        <ul>
            <li>std::string_view avoids unnecessary copying</li>
            <li>Move semantics eliminate temporary objects</li>
            <li>Smart pointers have zero overhead over raw pointers</li>
        </ul>
        
        <h4>Optimization Guidelines</h4>
        <ul>
            <li>Profile before and after modernization</li>
            <li>Use compiler explorer to verify optimizations</li>
            <li>Enable link-time optimization (LTO)</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Migration Strategy</h2>
    
    <h3>Gradual Adoption</h3>
    <div class="code-block">
        <pre><code>// Phase 1: Replace problematic patterns
// - Raw pointers -> smart pointers in new code
// - C-style casts -> static_cast/dynamic_cast
// - Manual memory management -> RAII

// Phase 2: Add type safety
// - Function parameters: const char* -> std::string_view
// - Return values: Add std::optional for nullable returns
// - Error handling: Exceptions or std::expected (C++23)

// Phase 3: Performance improvements
// - Hot paths: Use templates and constexpr
// - Algorithms: Replace loops with std::ranges
// - String processing: Use std::string_view

// Phase 4: Advanced features
// - Async operations: Coroutines
// - Type safety: Concepts
// - Meta programming: Advanced templates</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/modernization/build-systems">Modern Build Systems</a></li>
        <li><a href="/modernization/profiling-tools">Performance Profiling</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
        <li><a href="/external/libraries">External Libraries</a></li>
    </ul>
</div>