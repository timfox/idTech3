<?php
/**
 * C23 Overview - id Tech 3 Documentation
 */
$title = 'C23 Overview - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/c23-overview' => 'C23 Overview'
];
?>

<h1>C23 Overview</h1>

<div class="section">
    <h2>Introduction to C23</h2>
    <p>C23 is the latest revision of the C programming language standard, officially published as ISO/IEC 9899:2023. It brings significant improvements to type safety, memory safety, compile-time evaluation, and developer productivity.</p>
    
    <div class="feature-list">
        <h3>Key Features of C23</h3>
        <ul>
            <li><strong>Enhanced Type System:</strong> <code>typeof</code> and <code>typeof_unqual</code> operators, improved type inference</li>
            <li><strong>Attributes:</strong> Standardized attribute syntax with <code>[[attributes]]</code></li>
            <li><strong>Memory Safety:</strong> Bounds-checking interfaces, improved pointer safety</li>
            <li><strong>Standard Library:</strong> New functions for safer string handling, improved math functions</li>
            <li><strong>Compile-Time Features:</strong> <code>constexpr</code> functions, improved preprocessor</li>
            <li><strong>Unicode Support:</strong> Better UTF-8 and UTF-16 handling</li>
            <li><strong>Deprecations:</strong> Removal of unsafe features, cleaner language</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Why C23 for id Tech 3?</h2>
    <p>The id Tech 3 engine codebase benefits significantly from C23's improvements:</p>
    
    <ul>
        <li><strong>Memory Safety:</strong> Better detection of buffer overflows and use-after-free bugs</li>
        <li><strong>Type Safety:</strong> Reduced type-related bugs through improved type checking</li>
        <li><strong>Performance:</strong> Compile-time evaluation opportunities with <code>constexpr</code></li>
        <li><strong>Maintainability:</strong> Cleaner code with standardized attributes and better error handling</li>
        <li><strong>Modern Practices:</strong> Alignment with modern C development standards</li>
    </ul>
</div>

<div class="section">
    <h2>Compiler Support</h2>
    <p>To use C23 features in id Tech 3, ensure your compiler supports C23:</p>
    
    <div class="code-block">
        <pre><code>// CMakeLists.txt - Enable C23
cmake_minimum_required(VERSION 3.20)

project(idtech3 C)

# Set C23 standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)  # Use standard C, not GNU extensions

# Compiler-specific flags
if(CMAKE_C_COMPILER_ID STREQUAL "GCC")
    # GCC 13+ supports C23
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS "13.0")
        message(WARNING "GCC 13+ recommended for full C23 support")
    endif()
    add_compile_options(-std=c23)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    # Clang 17+ supports C23
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS "17.0")
        message(WARNING "Clang 17+ recommended for full C23 support")
    endif()
    add_compile_options(-std=c23)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    # MSVC support varies
    add_compile_options(/std:c23)
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Migration from C99/C11</h2>
    <p>Migrating existing code to C23 is generally straightforward:</p>
    
    <h3>1. Update Compiler Flags</h3>
    <div class="code-block">
        <pre><code>// Old: -std=c99 or -std=c11
// New: -std=c23</code></pre>
    </div>
    
    <h3>2. Replace Non-Standard Extensions</h3>
    <div class="code-block">
        <pre><code>// Old: GNU extension
__attribute__((unused)) static int x;

// New: C23 standard
[[maybe_unused]] static int x;</code></pre>
    </div>
    
    <h3>3. Use Standard Attributes</h3>
    <div class="code-block">
        <pre><code>// Old: Compiler-specific
#ifdef __GNUC__
    __attribute__((deprecated))
#endif
void old_function(void);

// New: C23 standard
[[deprecated("Use new_function() instead")]]
void old_function(void);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Quick Start Examples</h2>
    
    <h3>Example 1: Type Inference with typeof</h3>
    <div class="code-block">
        <pre><code>// C23: Type inference
int x = 42;
typeof(x) y = 10;  // y is int

// Useful for macros
#define MAX(a, b) \
    ({ typeof(a) _a = (a); \
       typeof(b) _b = (b); \
       _a > _b ? _a : _b; })</code></pre>
    </div>
    
    <h3>Example 2: Attributes</h3>
    <div class="code-block">
        <pre><code>// C23: Standard attributes
[[nodiscard]] int allocate_memory(size_t size);
[[deprecated("Use new_api()")]] void old_api(void);
[[maybe_unused]] static int debug_counter;</code></pre>
    </div>
    
    <h3>Example 3: Bounds Checking</h3>
    <div class="code-block">
        <pre><code>// C23: Bounds-checked functions
#include &lt;string.h&gt;

char buffer[256];
strcpy_s(buffer, sizeof(buffer), "Hello, C23!");</code></pre>
    </div>
</div>

<div class="section">
    <h2>Documentation Structure</h2>
    <p>This C23 documentation is organized into the following topics:</p>
    
    <ul>
        <li><a href="/tutorials/c23-attributes">C23 Attributes</a> - Standardized attribute syntax and usage</li>
        <li><a href="/tutorials/c23-type-system">C23 Type System</a> - typeof, typeof_unqual, and type improvements</li>
        <li><a href="/tutorials/c23-memory-safety">C23 Memory Safety</a> - Bounds checking and safer memory operations</li>
        <li><a href="/tutorials/c23-standard-library">C23 Standard Library</a> - New and improved standard library functions</li>
        <li><a href="/tutorials/c23-constexpr">C23 Compile-Time Features</a> - constexpr functions and compile-time evaluation</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    
    <h3>1. Use Standard Attributes</h3>
    <p>Prefer C23 standard attributes over compiler-specific extensions:</p>
    <div class="code-block">
        <pre><code>// Good: Standard C23
[[nodiscard]] int parse_config(const char* path);

// Avoid: Compiler-specific
#ifdef __GNUC__
__attribute__((warn_unused_result))
#endif
int parse_config(const char* path);</code></pre>
    </div>
    
    <h3>2. Enable Bounds Checking in Debug Builds</h3>
    <div class="code-block">
        <pre><code>#ifdef DEBUG
    #define SAFE_COPY(dst, dst_size, src) strcpy_s(dst, dst_size, src)
#else
    #define SAFE_COPY(dst, dst_size, src) strcpy(dst, src)
#endif</code></pre>
    </div>
    
    <h3>3. Use typeof for Generic Macros</h3>
    <div class="code-block">
        <pre><code>// Type-safe macro
#define SWAP(a, b) \
    do { \
        typeof(a) _temp = (a); \
        (a) = (b); \
        (b) = _temp; \
    } while(0)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/tutorials/cpp23-modules">C++23 Migration Highlights</a></li>
        <li><a href="/core/memory-management">Memory Management</a></li>
        <li><a href="/core/memory-safety">Memory Safety Tools</a></li>
        <li><a href="/getting-started/build-instructions">Build Instructions</a></li>
    </ul>
</div>
