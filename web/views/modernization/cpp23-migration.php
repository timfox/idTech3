<?php
$title = "C++23 Migration Opportunities";
?>

<h1>C++23 Migration Opportunities</h1>

<p>This document identifies safe, incremental opportunities to adopt C++23 features without breaking existing functionality.</p>

<h2>Overview</h2>

<p>The codebase has several areas where C++23 features can provide:</p>
<ul>
    <li><strong>Better type safety</strong> (std::expected, std::optional)</li>
    <li><strong>Improved performance</strong> (std::string_view, std::format)</li>
    <li><strong>Cleaner code</strong> (std::print, if consteval)</li>
    <li><strong>Better error handling</strong> (std::expected)</li>
</ul>

<h2>Safe Migration Areas</h2>

<h3>1. String Formatting (Low Risk, High Impact)</h3>

<h4>Current Pattern:</h4>
<pre><code>// src/renderervk/vk.c
Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x", ... );
ri.Printf( PRINT_WARNING, "VK: Failed to create gamma compute pipeline: %d\n", result );
</code></pre>

<h4>C++23 Improvement:</h4>
<pre><code>#include &lt;print&gt;  // C++23 std::print

// Type-safe, faster, no buffer overflows
std::print(stderr, "VK: Failed to create gamma compute pipeline: {}\n", result);
std::print("Device: {} {}, 0x{:04x}\n", vendor, model, deviceId);
</code></pre>

<h4>Files to Update:</h4>
<ul>
    <li><code>src/renderervk/vk.c</code> (1843 printf/Com_sprintf calls)</li>
    <li><code>src/renderervk/vk_dlss.c</code></li>
    <li><code>src/renderervk/vk_raytracing.c</code></li>
    <li><code>src/renderervk/vk_virtual_texture.c</code></li>
</ul>

<h4>Migration Strategy:</h4>
<ol>
    <li>Start with new code - use <code>std::print</code> for all new logging</li>
    <li>Gradually replace <code>ri.Printf</code> calls in non-critical paths</li>
    <li>Keep <code>Com_sprintf</code> for C interface compatibility (C code can't use C++23)</li>
</ol>

<h4>Benefits:</h4>
<ul>
    <li>Compile-time format string checking</li>
    <li>No buffer overflow risks</li>
    <li>Better performance (no runtime parsing)</li>
    <li>Type-safe formatting</li>
</ul>

<h3>2. Optional Return Values (Medium Risk, High Safety)</h3>

<h4>Current Pattern:</h4>
<pre><code>// src/qcommon/ecs.cpp
ecs_entity_t ECS_CreateEntity(void) {
    if (g_registry == nullptr) {
        return ECS_NULL_ENTITY;  // Magic value indicates failure
    }
    entt::entity entity = g_registry->create();
    return static_cast&lt;ecs_entity_t&gt;(entity);
}
</code></pre>

<h4>C++23 Improvement:</h4>
<pre><code>#include &lt;optional&gt;

std::optional&lt;ecs_entity_t&gt; ECS_CreateEntity(void) {
    if (g_registry == nullptr) {
        return std::nullopt;  // Explicit no-value state
    }
    entt::entity entity = g_registry->create();
    return static_cast&lt;ecs_entity_t&gt;(entity);
}

// Usage - compiler enforces null check
if (auto entity = ECS_CreateEntity()) {
    // entity.value() is guaranteed valid
    ECS_AddComponent(*entity, ...);
} else {
    Com_Error(ERR_FATAL, "Failed to create entity");
}
</code></pre>

<h4>Files to Update:</h4>
<ul>
    <li><code>src/qcommon/ecs.cpp</code> - Entity creation/destruction</li>
    <li><code>src/qcommon/ecs_systems.cpp</code> - System queries</li>
    <li><code>src/server/sv_ecs.cpp</code> - Server-side ECS</li>
</ul>

<h4>Migration Strategy:</h4>
<ol>
    <li>Add new functions with <code>std::optional</code> return types</li>
    <li>Keep old functions for compatibility</li>
    <li>Mark old functions as deprecated</li>
    <li>Gradually migrate callers</li>
</ol>

<h4>Benefits:</h4>
<ul>
    <li>Compiler-enforced null checks</li>
    <li>No magic sentinel values</li>
    <li>Clear intent in function signatures</li>
</ul>

<h3>3. Error Handling with std::expected (High Safety, Medium Complexity)</h3>

<h4>Current Pattern:</h4>
<pre><code>// Manual error codes
VkResult result = qvkCreateComputePipelines(...);
if (result != VK_SUCCESS) {
    ri.Printf(PRINT_WARNING, "Failed: %d\n", result);
    return;  // Silent failure
}
</code></pre>

<h4>C++23 Improvement:</h4>
<pre><code>#include &lt;expected&gt;
#include &lt;string&gt;

enum class VkError {
    Success,
    OutOfMemory,
    InvalidHandle,
    // ...
};

std::expected&lt;VkPipeline, VkError&gt; CreateComputePipeline(...) {
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
               static_cast&lt;int&gt;(pipeline.error()));
    return;
}
// pipeline.value() is guaranteed valid
</code></pre>

<h2>Migration Priority</h2>

<h3>High Priority (Low Risk)</h3>
<ol>
    <li>String formatting with <code>std::print</code> in new code</li>
    <li>Use <code>std::string_view</code> for function parameters</li>
    <li>Add <code>std::optional</code> return types to new functions</li>
</ol>

<h3>Medium Priority (Medium Risk)</h3>
<ol>
    <li>Migrate existing functions to <code>std::optional</code></li>
    <li>Add <code>std::expected</code> for error handling in new code</li>
    <li>Use <code>if consteval</code> for compile-time optimizations</li>
</ol>

<h3>Low Priority (Higher Risk)</h3>
<ol>
    <li>Migrate existing error handling to <code>std::expected</code></li>
    <li>Replace all <code>Com_sprintf</code> calls with <code>std::format</code></li>
    <li>Use C++23 ranges for container operations</li>
</ol>

<h2>Compatibility Considerations</h2>

<ul>
    <li>C++23 features require C++23 compiler support</li>
    <li>Some features may not be available in all compilers yet</li>
    <li>C code cannot use C++23 features - maintain C compatibility where needed</li>
    <li>Gradual migration allows testing and validation</li>
</ul>

<h2>See Also</h2>

<ul>
    <li><a href="modernization/modern-cpp">Modern C++</a></li>
    <li><a href="modernization/build-systems">Build Systems</a></li>
</ul>

