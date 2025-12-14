<?php
/**
 * C23 Type System - id Tech 3 Documentation
 */
$title = 'C23 Type System - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/c23-overview' => 'C23 Overview',
    '/tutorials/c23-type-system' => 'C23 Type System'
];
?>

<h1>C23 Type System Improvements</h1>

<div class="section">
    <h2>Overview</h2>
    <p>C23 introduces powerful type system enhancements including the <code>typeof</code> and <code>typeof_unqual</code> operators, improved type inference, and better type safety features.</p>
</div>

<div class="section">
    <h2>typeof Operator</h2>
    <p>The <code>typeof</code> operator allows you to refer to the type of an expression or variable. This enables type-safe generic programming in C.</p>
    
    <h3>Basic Usage</h3>
    <div class="code-block">
        <pre><code>// Get type of a variable
int x = 42;
typeof(x) y = 10;  // y is int

// Get type of an expression
typeof(x + 1.0) z;  // z is double (promoted type)

// Use in variable declarations
int* ptr = malloc(sizeof(typeof(*ptr)) * 10);</code></pre>
    </div>
    
    <h3>Type-Safe Macros</h3>
    <div class="code-block">
        <pre><code>// Type-safe MAX macro
#define MAX(a, b) \
    ({ typeof(a) _a = (a); \
       typeof(b) _b = (b); \
       _a > _b ? _a : _b; })

// Usage - works with any numeric type
int i = MAX(10, 20);
float f = MAX(3.14f, 2.71f);
unsigned u = MAX(100u, 200u);</code></pre>
    </div>
    
    <h3>Generic Container Macros</h3>
    <div class="code-block">
        <pre><code>// Type-safe swap macro
#define SWAP(a, b) \
    do { \
        typeof(a) _temp = (a); \
        (a) = (b); \
        (b) = _temp; \
    } while(0)

// Usage
int x = 10, y = 20;
SWAP(x, y);  // x=20, y=10

float a = 1.5f, b = 2.5f;
SWAP(a, b);  // Works with any type</code></pre>
    </div>
</div>

<div class="section">
    <h2>typeof_unqual Operator</h2>
    <p><code>typeof_unqual</code> returns the unqualified version of a type, removing <code>const</code>, <code>volatile</code>, and <code>restrict</code> qualifiers.</p>
    
    <div class="code-block">
        <pre><code>// typeof_unqual removes qualifiers
const int x = 42;
typeof(x) y;           // y is const int
typeof_unqual(x) z;     // z is int (unqualified)

volatile int* ptr;
typeof(ptr) p1;                // p1 is volatile int*
typeof_unqual(ptr) p2;         // p2 is int*

// Useful for creating unqualified copies
const char* str = "Hello";
typeof_unqual(*str) buffer[256];  // buffer is char[256], not const char[256]</code></pre>
    </div>
</div>

<div class="section">
    <h2>Type Inference in Function Declarations</h2>
    <p>C23 allows using <code>typeof</code> in function parameter and return type declarations.</p>
    
    <div class="code-block">
        <pre><code>// Function that returns same type as input
#define DECLARE_IDENTITY(type) \
    typeof(type) identity(typeof(type) x) { return x; }

// Or more directly
typeof(42) identity_int(typeof(42) x) { return x; }

// Generic comparison function
#define COMPARE_FUNC(name, type) \
    int name(typeof(type) a, typeof(type) b) { \
        return a < b ? -1 : (a > b ? 1 : 0); \
    }

COMPARE_FUNC(compare_int, int)
COMPARE_FUNC(compare_float, float)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Practical Examples in id Tech 3</h2>
    
    <h3>Generic Math Functions</h3>
    <div class="code-block">
        <pre><code>// Type-safe clamp function
#define CLAMP(val, min, max) \
    ({ typeof(val) _val = (val); \
       typeof(min) _min = (min); \
       typeof(max) _max = (max); \
       _val < _min ? _min : (_val > _max ? _max : _val); })

// Usage
int x = CLAMP(player->health, 0, 100);
float speed = CLAMP(entity->velocity[0], -MAX_SPEED, MAX_SPEED);</code></pre>
    </div>
    
    <h3>Type-Safe Array Operations</h3>
    <div class="code-block">
        <pre><code>// Get array size with type checking
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(typeof(*arr)))

// Usage
int numbers[] = {1, 2, 3, 4, 5};
size_t count = ARRAY_SIZE(numbers);  // count = 5

// Type-safe array initialization
#define ZERO_ARRAY(arr) \
    memset((arr), 0, sizeof(typeof(arr)))

int buffer[256];
ZERO_ARRAY(buffer);</code></pre>
    </div>
    
    <h3>Generic Memory Operations</h3>
    <div class="code-block">
        <pre><code>// Type-safe memory allocation
#define ALLOC_TYPE(type) \
    ((typeof(type)*)malloc(sizeof(typeof(type))))

#define ALLOC_ARRAY(type, count) \
    ((typeof(type)*)malloc(sizeof(typeof(type)) * (count)))

// Usage
typedef struct {
    vec3_t origin;
    vec3_t angles;
} entity_state_t;

entity_state_t* state = ALLOC_TYPE(entity_state_t);
entity_state_t* states = ALLOC_ARRAY(entity_state_t, 100);</code></pre>
    </div>
    
    <h3>Type-Safe Vector Operations</h3>
    <div class="code-block">
        <pre><code>// Generic vector dot product
#define DOT_PRODUCT(a, b) \
    ({ typeof(*a) _a0 = (a)[0], _a1 = (a)[1], _a2 = (a)[2]; \
       typeof(*b) _b0 = (b)[0], _b1 = (b)[1], _b2 = (b)[2]; \
       _a0 * _b0 + _a1 * _b1 + _a2 * _b2; })

// Usage
vec3_t v1 = {1.0f, 2.0f, 3.0f};
vec3_t v2 = {4.0f, 5.0f, 6.0f};
float dot = DOT_PRODUCT(v1, v2);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Improved Type Checking</h2>
    
    <h3>Static Assertions with Types</h3>
    <div class="code-block">
        <pre><code>// Ensure types match
#define STATIC_ASSERT_SAME_TYPE(a, b) \
    _Static_assert(_Generic((a), typeof(b): 1, default: 0), \
                   "Types must match")

// Usage
int x = 10;
long y = 20;
// STATIC_ASSERT_SAME_TYPE(x, y);  // Compile error if types differ</code></pre>
    </div>
    
    <h3>Type-Safe Function Pointers</h3>
    <div class="code-block">
        <pre><code>// Type-safe callback registration
typedef struct {
    void (*callback)(void* data);
    typeof(data) data;
} callback_t;

// Ensure callback matches data type
#define REGISTER_CALLBACK(cb, data) \
    ({ typeof(data) _data = (data); \
       callback_t _cb = {.callback = (cb), .data = _data}; \
       register_callback(&_cb); })</code></pre>
    </div>
</div>

<div class="section">
    <h2>Limitations and Considerations</h2>
    
    <ul>
        <li><strong>typeof is evaluated at compile-time</strong> - cannot be used with runtime values</li>
        <li><strong>Type preservation</strong> - <code>typeof</code> preserves qualifiers, use <code>typeof_unqual</code> to remove them</li>
        <li><strong>Macro complexity</strong> - Complex macros using <code>typeof</code> can be harder to debug</li>
        <li><strong>Compiler support</strong> - Ensure your compiler fully supports C23 <code>typeof</code></li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    
    <ul>
        <li>Use <code>typeof</code> for type-safe generic macros</li>
        <li>Prefer <code>typeof_unqual</code> when you need an unqualified type</li>
        <li>Document complex macros that use <code>typeof</code></li>
        <li>Use static assertions to verify type compatibility</li>
        <li>Avoid overusing <code>typeof</code> - sometimes explicit types are clearer</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/tutorials/c23-overview">C23 Overview</a></li>
        <li><a href="/tutorials/c23-attributes">C23 Attributes</a></li>
        <li><a href="/core/memory-management">Memory Management</a></li>
    </ul>
</div>
