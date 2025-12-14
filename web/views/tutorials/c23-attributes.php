<?php
/**
 * C23 Attributes - id Tech 3 Documentation
 */
$title = 'C23 Attributes - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/c23-overview' => 'C23 Overview',
    '/tutorials/c23-attributes' => 'C23 Attributes'
];
?>

<h1>C23 Attributes</h1>

<div class="section">
    <h2>Overview</h2>
    <p>C23 introduces standardized attribute syntax using double square brackets <code>[[attribute]]</code>. This replaces compiler-specific extensions like <code>__attribute__</code> (GCC) and <code>__declspec</code> (MSVC) with a portable standard.</p>
</div>

<div class="section">
    <h2>Standard Attributes</h2>
    
    <h3>[[nodiscard]]</h3>
    <p>Indicates that the return value should not be ignored. The compiler will warn if the return value is discarded.</p>
    <div class="code-block">
        <pre><code>// Function that must check return value
[[nodiscard]] int allocate_buffer(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        return -1;  // Error
    }
    return 0;  // Success
}

// Usage - compiler warns if ignored
int result = allocate_buffer(1024);  // OK
allocate_buffer(1024);  // Warning: ignoring return value</code></pre>
    </div>
    
    <h3>[[deprecated]]</h3>
    <p>Marks a function, variable, or type as deprecated. Can include a message explaining why.</p>
    <div class="code-block">
        <pre><code>// Deprecated function with message
[[deprecated("Use Hunk_Alloc_v2() instead")]]
void* Hunk_Alloc_legacy(int size) {
    // Old implementation
}

// Simple deprecation
[[deprecated]]
static int old_global_var;

// Usage generates warnings
Hunk_Alloc_legacy(1024);  // Warning: deprecated</code></pre>
    </div>
    
    <h3>[[maybe_unused]]</h3>
    <p>Suppresses warnings about unused variables, functions, or types. Useful for debug code, parameters, or intentionally unused items.</p>
    <div class="code-block">
        <pre><code>// Unused parameter (common in callback functions)
void entity_callback([[maybe_unused]] void* data, int event_type) {
    // Only use event_type, data is unused
}

// Debug-only variable
[[maybe_unused]] static int debug_counter;

// Unused function (kept for compatibility)
[[maybe_unused]] static void legacy_function(void) {
    // Not used but kept for API compatibility
}</code></pre>
    </div>
    
    <h3>[[fallthrough]]</h3>
    <p>Indicates intentional fallthrough in switch statements. Suppresses warnings about missing break statements.</p>
    <div class="code-block">
        <pre><code>switch (entity_type) {
    case ET_PLAYER:
        // Process player-specific logic
        [[fallthrough]];  // Intentionally fall through
    case ET_BOT:
        // Process common logic for players and bots
        process_ai(entity);
        break;
    case ET_ITEM:
        // Process items
        break;
}</code></pre>
    </div>
    
    <h3>[[unsequenced]]</h3>
    <p>Indicates that a function has no side effects and can be called multiple times without changing program state.</p>
    <div class="code-block">
        <pre><code>// Pure function - no side effects
[[unsequenced]] int calculate_distance(vec3_t a, vec3_t b) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return (int)sqrt(dx*dx + dy*dy + dz*dz);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Attribute Syntax</h2>
    
    <h3>Basic Syntax</h3>
    <div class="code-block">
        <pre><code>// Function attribute
[[nodiscard]] int function(void);

// Variable attribute
[[maybe_unused]] static int x;

// Type attribute
typedef [[deprecated]] int old_type_t;

// Statement attribute (for fallthrough)
switch (x) {
    case 1:
        [[fallthrough]];
    case 2:
        break;
}</code></pre>
    </div>
    
    <h3>Multiple Attributes</h3>
    <div class="code-block">
        <pre><code>// Multiple attributes on same declaration
[[nodiscard]] [[deprecated("Use new_function()")]]
int old_function(void);

// Or combined
[[nodiscard, deprecated("Use new_function()")]]
int old_function(void);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Practical Examples in id Tech 3</h2>
    
    <h3>Memory Allocation Functions</h3>
    <div class="code-block">
        <pre><code>// Memory allocation - must check return value
[[nodiscard]] void* Hunk_Alloc_v2(size_t size) {
    if (size == 0) {
        return NULL;
    }
    // Allocation logic
    return allocated_ptr;
}

// Usage
void* buffer = Hunk_Alloc_v2(1024);
if (!buffer) {
    Com_Error(ERR_FATAL, "Failed to allocate buffer");
}</code></pre>
    </div>
    
    <h3>Error Handling</h3>
    <div class="code-block">
        <pre><code>// Error codes must be checked
[[nodiscard]] int FS_LoadFile(const char* filename, void** buffer) {
    // Load file logic
    if (error) {
        return -1;
    }
    return 0;
}

// Compiler enforces checking
int result = FS_LoadFile("config.cfg", &data);
if (result != 0) {
    Com_Printf("Failed to load file\n");
}</code></pre>
    </div>
    
    <h3>Deprecated API Migration</h3>
    <div class="code-block">
        <pre><code>// Old API - deprecated
[[deprecated("Use Com_Printf_v2() with format checking")]]
void Com_Printf_legacy(const char* fmt, ...);

// New API
void Com_Printf_v2(const char* fmt, ...) 
    __attribute__((format(printf, 1, 2)));

// Migration path
#define Com_Printf_legacy(...) \
    do { \
        _Pragma("GCC warning \"Com_Printf_legacy is deprecated\"") \
        Com_Printf_v2(__VA_ARGS__); \
    } while(0)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Migration from Compiler Extensions</h2>
    
    <h3>GCC __attribute__</h3>
    <div class="code-block">
        <pre><code>// Old: GCC extension
__attribute__((unused)) static int x;
__attribute__((deprecated)) void old_func(void);
__attribute__((warn_unused_result)) int alloc(void);

// New: C23 standard
[[maybe_unused]] static int x;
[[deprecated]] void old_func(void);
[[nodiscard]] int alloc(void);</code></pre>
    </div>
    
    <h3>MSVC __declspec</h3>
    <div class="code-block">
        <pre><code>// Old: MSVC extension
__declspec(deprecated) void old_func(void);
__declspec(noreturn) void fatal_error(void);

// New: C23 standard
[[deprecated]] void old_func(void);
// Note: noreturn is a function specifier, not an attribute
_Noreturn void fatal_error(void);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Best Practices</h2>
    
    <ul>
        <li><strong>Use [[nodiscard]]</strong> for functions where ignoring the return value is likely a bug (error codes, allocation results)</li>
        <li><strong>Use [[deprecated]]</strong> with messages explaining migration paths</li>
        <li><strong>Use [[maybe_unused]]</strong> sparingly - prefer removing unused code when possible</li>
        <li><strong>Use [[fallthrough]]</strong> to document intentional switch fallthrough</li>
        <li><strong>Avoid compiler-specific attributes</strong> when C23 standard attributes are available</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/tutorials/c23-overview">C23 Overview</a></li>
        <li><a href="/tutorials/c23-type-system">C23 Type System</a></li>
        <li><a href="/core/memory-management">Memory Management</a></li>
    </ul>
</div>
