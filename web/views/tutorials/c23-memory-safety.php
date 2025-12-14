<?php
/**
 * C23 Memory Safety - id Tech 3 Documentation
 */
$title = 'C23 Memory Safety - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/c23-overview' => 'C23 Overview',
    '/tutorials/c23-memory-safety' => 'C23 Memory Safety'
];
?>

<h1>C23 Memory Safety Features</h1>

<div class="section">
    <h2>Overview</h2>
    <p>C23 introduces several memory safety features to help prevent common bugs like buffer overflows, use-after-free, and uninitialized memory access. These features are especially important for game engines like id Tech 3 where performance and safety must coexist.</p>
</div>

<div class="section">
    <h2>Bounds-Checked Functions</h2>
    <p>C23 standardizes bounds-checked versions of many standard library functions. These functions take explicit size parameters and validate bounds before operations.</p>
    
    <h3>String Functions</h3>
    <div class="code-block">
        <pre><code>#include &lt;string.h&gt;

// Bounds-checked string copy
char buffer[256];
strcpy_s(buffer, sizeof(buffer), "Hello, C23!");

// Bounds-checked string concatenation
char path[512] = "/baseq3/";
strcat_s(path, sizeof(path), "maps/");

// Bounds-checked string length
size_t len;
strnlen_s(buffer, sizeof(buffer), &len);

// Bounds-checked string comparison
if (strncmp_s(str1, sizeof(str1), str2, sizeof(str2), &result) == 0) {
    // Safe comparison
}</code></pre>
    </div>
    
    <h3>Memory Functions</h3>
    <div class="code-block">
        <pre><code>#include &lt;string.h&gt;

// Bounds-checked memory copy
void* dest = malloc(1024);
void* src = get_data();
memcpy_s(dest, 1024, src, data_size);

// Bounds-checked memory set
memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));

// Bounds-checked memory move
memmove_s(dest, dest_size, src, src_size);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Dynamic Bounds Checking</h2>
    <p>C23 provides runtime bounds checking that can be enabled in debug builds.</p>
    
    <h3>Bounds-Checked Pointers</h3>
    <div class="code-block">
        <pre><code>// Bounds-checked pointer type (conceptual)
#include &lt;stdchecked.h&gt;

// Pointer with known bounds
int* _Checked buffer : count(size);
int size = 256;
buffer = malloc(sizeof(int) * size);

// Access is bounds-checked
for (int i = 0; i &lt; size; i++) {
    buffer[i] = i;  // Bounds checked at runtime
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Practical Examples in id Tech 3</h2>
    
    <h3>Safe String Handling</h3>
    <div class="code-block">
        <pre><code>// Old: Unsafe string operations
void Com_Printf(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);  // Potential buffer overflow!
    va_end(args);
    // ...
}

// New: Bounds-checked version
void Com_Printf_safe(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    // ...
}</code></pre>
    </div>
    
    <h3>Safe File Path Operations</h3>
    <div class="code-block">
        <pre><code>// Safe path concatenation
qboolean FS_BuildPath(char* dest, size_t dest_size, 
                      const char* base, const char* file) {
    if (strcpy_s(dest, dest_size, base) != 0) {
        return qfalse;
    }
    
    size_t base_len;
    strnlen_s(dest, dest_size, &base_len);
    
    if (base_len + strlen(file) + 1 >= dest_size) {
        return qfalse;  // Path too long
    }
    
    if (strcat_s(dest, dest_size, file) != 0) {
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
    
    <h3>Safe Memory Allocation Wrappers</h3>
    <div class="code-block">
        <pre><code>// Safe allocation with bounds checking
void* Hunk_Alloc_safe(size_t size) {
    if (size == 0 || size > MAX_HUNK_SIZE) {
        Com_Error(ERR_FATAL, "Invalid allocation size: %zu", size);
        return NULL;
    }
    
    void* ptr = Hunk_Alloc_v2(size);
    if (ptr) {
        // Zero-initialize for safety
        memset_s(ptr, size, 0, size);
    }
    
    return ptr;
}

// Safe array allocation
void* Hunk_AllocArray_safe(size_t count, size_t element_size) {
    if (count == 0 || element_size == 0) {
        return NULL;
    }
    
    // Check for overflow
    if (count > SIZE_MAX / element_size) {
        Com_Error(ERR_FATAL, "Allocation overflow: %zu * %zu", 
                 count, element_size);
        return NULL;
    }
    
    return Hunk_Alloc_safe(count * element_size);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Initialization and Zeroing</h2>
    <p>C23 improves initialization guarantees and provides better ways to ensure memory is properly initialized.</p>
    
    <h3>Automatic Zero Initialization</h3>
    <div class="code-block">
        <pre><code>// C23: Static and thread-local variables are zero-initialized
static int counter;           // Automatically 0
static char buffer[256];       // Automatically all zeros

// Explicit zero initialization
int array[100] = {0};  // All elements zero

// Designated initializers (C23 improved)
struct player_s {
    int health;
    int armor;
    vec3_t origin;
};

struct player_s player = {
    .health = 100,
    .armor = 0,
    .origin = {0.0f, 0.0f, 0.0f}
};</code></pre>
    </div>
    
    <h3>Safe Memory Clearing</h3>
    <div class="code-block">
        <pre><code>// Safe memory clearing before free
void Hunk_Free_safe(void* ptr, size_t size) {
    if (!ptr) {
        return;
    }
    
    // Clear sensitive data before freeing
    memset_s(ptr, size, 0, size);
    
    // Mark as freed (helps detect use-after-free)
    memset_s(ptr, size, 0xDE, size);
    
    Hunk_Free(ptr);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Use-After-Free Detection</h2>
    <p>While C23 doesn't provide automatic use-after-free detection, we can implement patterns to help detect these bugs.</p>
    
    <div class="code-block">
        <pre><code>// Memory tracking for use-after-free detection
#ifdef DEBUG
typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    uint32_t magic;
} mem_track_t;

#define MEM_MAGIC 0xDEADBEEF

void* Track_Alloc(size_t size, const char* file, int line) {
    mem_track_t* track = malloc(sizeof(mem_track_t) + size);
    track->ptr = (char*)track + sizeof(mem_track_t);
    track->size = size;
    track->file = file;
    track->line = line;
    track->magic = MEM_MAGIC;
    
    return track->ptr;
}

void Track_Free(void* ptr) {
    if (!ptr) return;
    
    mem_track_t* track = (mem_track_t*)((char*)ptr - sizeof(mem_track_t));
    
    if (track->magic != MEM_MAGIC) {
        Com_Error(ERR_FATAL, "Use-after-free detected at %s:%d",
                 track->file, track->line);
    }
    
    // Invalidate magic
    track->magic = 0;
    free(track);
}

#define SAFE_ALLOC(size) Track_Alloc(size, __FILE__, __LINE__)
#define SAFE_FREE(ptr) Track_Free(ptr)
#else
#define SAFE_ALLOC(size) malloc(size)
#define SAFE_FREE(ptr) free(ptr)
#endif</code></pre>
    </div>
</div>

<div class="section">
    <h2>Buffer Overflow Prevention</h2>
    
    <h3>Safe String Operations</h3>
    <div class="code-block">
        <pre><code>// Safe string copy with validation
qboolean Safe_StringCopy(char* dest, size_t dest_size, 
                        const char* src) {
    if (!dest || !src || dest_size == 0) {
        return qfalse;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        // Truncate safely
        strncpy_s(dest, dest_size, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
        return qfalse;  // Indicate truncation
    }
    
    strcpy_s(dest, dest_size, src);
    return qtrue;
}

// Safe string formatting
int Safe_Snprintf(char* buffer, size_t buffer_size, 
                  const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vsnprintf_s(buffer, buffer_size, fmt, args);
    va_end(args);
    
    if (result < 0) {
        // Error occurred
        buffer[0] = '\0';
    } else if ((size_t)result >= buffer_size) {
        // Truncation occurred
        buffer[buffer_size - 1] = '\0';
    }
    
    return result;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Best Practices</h2>
    
    <ul>
        <li><strong>Always use bounds-checked functions</strong> in security-critical code paths</li>
        <li><strong>Validate sizes</strong> before memory operations</li>
        <li><strong>Zero-initialize</strong> sensitive data structures</li>
        <li><strong>Use static analysis tools</strong> to find potential memory safety issues</li>
        <li><strong>Enable bounds checking</strong> in debug builds, optimize in release builds</li>
        <li><strong>Document memory ownership</strong> - who allocates, who frees</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    <p>Bounds checking has performance overhead. Use these strategies:</p>
    
    <ul>
        <li>Enable bounds checking in debug builds only</li>
        <li>Use compile-time size checks where possible</li>
        <li>Profile critical paths to measure overhead</li>
        <li>Consider using static analysis instead of runtime checks in release builds</li>
    </ul>
    
    <div class="code-block">
        <pre><code>// Conditional bounds checking
#ifdef DEBUG
    #define SAFE_COPY(dst, dst_size, src) strcpy_s(dst, dst_size, src)
    #define SAFE_CAT(dst, dst_size, src) strcat_s(dst, dst_size, src)
#else
    #define SAFE_COPY(dst, dst_size, src) strcpy(dst, src)
    #define SAFE_CAT(dst, dst_size, src) strcat(dst, src)
#endif</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/tutorials/c23-overview">C23 Overview</a></li>
        <li><a href="/tutorials/c23-standard-library">C23 Standard Library</a></li>
        <li><a href="/core/memory-management">Memory Management</a></li>
        <li><a href="/core/memory-safety">Memory Safety Tools</a></li>
    </ul>
</div>
