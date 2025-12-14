<?php
/**
 * C23 Compile-Time Features - id Tech 3 Documentation
 */
$title = 'C23 Compile-Time Features - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/c23-overview' => 'C23 Overview',
    '/tutorials/c23-constexpr' => 'C23 Compile-Time Features'
];
?>

<h1>C23 Compile-Time Features</h1>

<div class="section">
    <h2>Overview</h2>
    <p>C23 introduces <code>constexpr</code> functions and improved compile-time evaluation capabilities. These features allow computations to be performed at compile time, improving runtime performance and enabling better optimizations.</p>
</div>

<div class="section">
    <h2>constexpr Functions</h2>
    <p><code>constexpr</code> functions can be evaluated at compile time when their arguments are constant expressions. This enables compile-time computation and better optimization.</p>
    
    <h3>Basic constexpr Functions</h3>
    <div class="code-block">
        <pre><code>// Simple constexpr function
constexpr int square(int x) {
    return x * x;
}

// Can be used at compile time
const int result = square(5);  // Evaluated at compile time: 25

// Or at runtime
int value = 10;
int runtime_result = square(value);  // Evaluated at runtime</code></pre>
    </div>
    
    <h3>constexpr with Complex Logic</h3>
    <div class="code-block">
        <pre><code>// constexpr with conditionals
constexpr int max(int a, int b) {
    return a > b ? a : b;
}

// constexpr with loops (C23 allows this)
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

// Compile-time constant
const int fact_5 = factorial(5);  // 120, computed at compile time</code></pre>
    </div>
</div>

<div class="section">
    <h2>Compile-Time Constants</h2>
    
    <h3>Using constexpr for Constants</h3>
    <div class="code-block">
        <pre><code>// Compile-time computed constants
constexpr int ARRAY_SIZE = 256;
constexpr int BUFFER_SIZE = square(16);  // 256

// Compile-time string length
constexpr size_t string_length(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

constexpr size_t MAX_PATH_LEN = string_length("/baseq3/maps/");
// MAX_PATH_LEN is computed at compile time</code></pre>
    </div>
    
    <h3>Array Sizes and Bounds</h3>
    <div class="code-block">
        <pre><code>// Compile-time array sizing
constexpr int calculate_buffer_size(int element_size, int count) {
    return element_size * count;
}

#define ELEMENT_SIZE 64
#define ELEMENT_COUNT 100
char buffer[calculate_buffer_size(ELEMENT_SIZE, ELEMENT_COUNT)];

// Compile-time validation
constexpr int validate_size(int size) {
    return size > 0 && size <= 1024 ? size : 0;
}

int valid_size = validate_size(512);  // 512
int invalid_size = validate_size(2048);  // 0</code></pre>
    </div>
</div>

<div class="section">
    <h2>Static Assertions</h2>
    <p>C23 improves static assertions, allowing compile-time checks with better error messages.</p>
    
    <div class="code-block">
        <pre><code>// Static assertion syntax
_Static_assert(sizeof(int) == 4, "int must be 4 bytes");
_Static_assert(sizeof(void*) == 8, "64-bit pointers required");

// With constexpr
constexpr int MAX_ENTITIES = 1024;
_Static_assert(MAX_ENTITIES > 0, "Must have at least one entity");
_Static_assert(MAX_ENTITIES <= 65535, "Entity count too large");

// Type checking
_Static_assert(sizeof(float) == 4, "float must be 32-bit");
_Static_assert(sizeof(double) == 8, "double must be 64-bit");</code></pre>
    </div>
</div>

<div class="section">
    <h2>Practical Examples in id Tech 3</h2>
    
    <h3>Compile-Time Math Functions</h3>
    <div class="code-block">
        <pre><code>// Compile-time math utilities
constexpr float deg_to_rad(float degrees) {
    return degrees * 3.14159265358979323846f / 180.0f;
}

constexpr float rad_to_deg(float radians) {
    return radians * 180.0f / 3.14159265358979323846f;
}

// Usage
const float FOV_RADIANS = deg_to_rad(90.0f);  // Computed at compile time

// Compile-time vector operations
constexpr float vec3_length_sq(float x, float y, float z) {
    return x*x + y*y + z*z;
}

constexpr float vec3_length(float x, float y, float z) {
    // Simplified for constexpr (no sqrt at compile time in C23)
    return vec3_length_sq(x, y, z);
}

// Can be used for compile-time validation
constexpr float MAX_SPEED_SQ = vec3_length_sq(100.0f, 100.0f, 100.0f);</code></pre>
    </div>
    
    <h3>Compile-Time Configuration</h3>
    <div class="code-block">
        <pre><code>// Compile-time configuration validation
constexpr int validate_config(int max_players, int max_entities) {
    if (max_players < 1 || max_players > 64) {
        return 0;  // Invalid
    }
    if (max_entities < max_players || max_entities > 8192) {
        return 0;  // Invalid
    }
    return 1;  // Valid
}

#define MAX_PLAYERS 32
#define MAX_ENTITIES 1024

// Compile-time check
_Static_assert(validate_config(MAX_PLAYERS, MAX_ENTITIES),
               "Invalid configuration values");

// Compile-time computed sizes
constexpr size_t player_data_size(int max_players) {
    return sizeof(player_t) * max_players;
}

constexpr size_t entity_data_size(int max_entities) {
    return sizeof(entity_t) * max_entities;
}

// Total memory requirement computed at compile time
constexpr size_t TOTAL_MEMORY = 
    player_data_size(MAX_PLAYERS) + 
    entity_data_size(MAX_ENTITIES);</code></pre>
    </div>
    
    <h3>Compile-Time String Processing</h3>
    <div class="code-block">
        <pre><code>// Compile-time string utilities
constexpr int string_compare(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return a[i] - b[i];
        }
        i++;
    }
    return a[i] - b[i];
}

// Compile-time string length
constexpr size_t compile_time_strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Usage
constexpr size_t VERSION_LEN = compile_time_strlen("1.0.0");
char version_buffer[VERSION_LEN + 1];</code></pre>
    </div>
</div>

<div class="section">
    <h2>Template-Like Behavior with Macros</h2>
    <p>Combining <code>constexpr</code> with macros can provide template-like functionality.</p>
    
    <div class="code-block">
        <pre><code>// Generic compile-time max function
#define MAX(a, b) \
    ({ typeof(a) _a = (a); \
       typeof(b) _b = (b); \
       _a > _b ? _a : _b; })

// Compile-time version
constexpr int constexpr_max(int a, int b) {
    return a > b ? a : b;
}

// Usage
const int max_value = constexpr_max(100, 200);  // 200, compile-time

// Generic clamp
#define CLAMP(val, min, max) \
    ({ typeof(val) _val = (val); \
       typeof(min) _min = (min); \
       typeof(max) _max = (max); \
       _val < _min ? _min : (_val > _max ? _max : _val); })

constexpr int constexpr_clamp(int val, int min, int max) {
    return val < min ? min : (val > max ? max : val);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Limitations</h2>
    <p>C23 <code>constexpr</code> has some limitations compared to C++:</p>
    
    <ul>
        <li><strong>No floating-point sqrt/pow</strong> at compile time in standard C23</li>
        <li><strong>Limited recursion depth</strong> - compiler-dependent</li>
        <li><strong>No dynamic memory</strong> - cannot allocate memory in constexpr</li>
        <li><strong>No I/O operations</strong> - cannot perform I/O in constexpr functions</li>
        <li><strong>Compiler support varies</strong> - check your compiler's C23 support</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    
    <ul>
        <li><strong>Use constexpr for pure functions</strong> that can be computed at compile time</li>
        <li><strong>Mark constants as constexpr</strong> when they're computed from other constants</li>
        <li><strong>Use static assertions</strong> to validate compile-time constants</li>
        <li><strong>Prefer constexpr over macros</strong> for type-safe compile-time computation</li>
        <li><strong>Document constexpr functions</strong> that are intended for compile-time use</li>
        <li><strong>Test constexpr functions</strong> both at compile time and runtime</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Benefits</h2>
    <p>Using <code>constexpr</code> can provide significant performance benefits:</p>
    
    <ul>
        <li><strong>Zero runtime cost</strong> - computations happen at compile time</li>
        <li><strong>Better optimization</strong> - compiler can inline and optimize better</li>
        <li><strong>Smaller binaries</strong> - constants are embedded directly</li>
        <li><strong>Type safety</strong> - compile-time type checking</li>
    </ul>
    
    <div class="code-block">
        <pre><code>// Example: Compile-time vs runtime
// Without constexpr
int calculate() {
    return 5 * 5;  // Computed at runtime
}

// With constexpr
constexpr int calculate() {
    return 5 * 5;  // Computed at compile time
}

const int result = calculate();  // No runtime computation</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/tutorials/c23-overview">C23 Overview</a></li>
        <li><a href="/tutorials/c23-type-system">C23 Type System</a></li>
        <li><a href="/performance/optimization">Performance Optimization</a></li>
    </ul>
</div>
