<?php
/**
 * C23 Standard Library - id Tech 3 Documentation
 */
$title = 'C23 Standard Library - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/c23-overview' => 'C23 Overview',
    '/tutorials/c23-standard-library' => 'C23 Standard Library'
];
?>

<h1>C23 Standard Library Improvements</h1>

<div class="section">
    <h2>Overview</h2>
    <p>C23 introduces significant improvements to the standard library, including new functions, improved existing functions, and better Unicode support. These improvements help make C code safer, more efficient, and easier to work with.</p>
</div>

<div class="section">
    <h2>String Handling Improvements</h2>
    
    <h3>Bounds-Checked String Functions</h3>
    <p>C23 standardizes bounds-checked string functions that were previously optional extensions.</p>
    
    <div class="code-block">
        <pre><code>#include &lt;string.h&gt;

// String copy with bounds checking
char buffer[256];
errno_t result = strcpy_s(buffer, sizeof(buffer), "Hello");

// String concatenation with bounds checking
char path[512] = "/baseq3/";
strcat_s(path, sizeof(path), "maps/");

// Bounded string length
size_t len;
strnlen_s(buffer, sizeof(buffer), &len);

// Bounded string comparison
int cmp_result;
strncmp_s(str1, sizeof(str1), str2, sizeof(str2), &cmp_result);

// Bounded string search
char* found = strstr_s(haystack, haystack_size, needle, needle_size);</code></pre>
    </div>
    
    <h3>Improved String Functions</h3>
    <div class="code-block">
        <pre><code>// memccpy - copy until character found
char* dest = buffer;
const char* src = "Hello, World!";
char* end = memccpy(dest, src, ',', strlen(src));
if (end) {
    *end = '\0';  // "Hello"
}

// strdup - duplicate string (now standard)
char* copy = strdup(original_string);
if (!copy) {
    // Handle allocation failure
}

// strndup - duplicate with length limit
char* limited = strndup(long_string, 100);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Functions</h2>
    
    <h3>Bounds-Checked Memory Operations</h3>
    <div class="code-block">
        <pre><code>#include &lt;string.h&gt;

// Bounds-checked memory copy
void* dest = malloc(1024);
void* src = get_data();
errno_t result = memcpy_s(dest, 1024, src, data_size);

// Bounds-checked memory move
memmove_s(dest, dest_size, src, src_size);

// Bounds-checked memory set
memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));

// Bounds-checked memory comparison
int cmp;
memcmp_s(ptr1, size1, ptr2, size2, &cmp);</code></pre>
    </div>
    
    <h3>Aligned Memory Allocation</h3>
    <div class="code-block">
        <pre><code>#include &lt;stdlib.h&gt;

// Aligned allocation (improved in C23)
void* aligned_ptr = aligned_alloc(16, 1024);  // 16-byte aligned

// Check alignment
#include &lt;stdalign.h&gt;
if ((uintptr_t)aligned_ptr % 16 == 0) {
    // Properly aligned
}

// Free aligned memory
free(aligned_ptr);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Mathematical Functions</h2>
    
    <h3>New Math Functions</h3>
    <div class="code-block">
        <pre><code>#include &lt;math.h&gt;

// fmaximum - maximum of two values
float max_val = fmaximum(a, b);

// fminimum - minimum of two values
float min_val = fminimum(a, b);

// fmaximum_num - maximum, treating NaN as missing
float max_no_nan = fmaximum_num(a, b);

// fminimum_num - minimum, treating NaN as missing
float min_no_nan = fminimum_num(a, b);

// fdim - positive difference
float diff = fdim(a, b);  // max(a - b, 0)

// fma - fused multiply-add (more accurate)
float result = fma(a, b, c);  // (a * b) + c with single rounding</code></pre>
    </div>
    
    <h3>Improved Math Constants</h3>
    <div class="code-block">
        <pre><code>#include &lt;math.h&gt;

// Mathematical constants (C23)
#define M_PI        3.14159265358979323846
#define M_E         2.71828182845904523536
#define M_SQRT2     1.41421356237309504880
#define M_SQRT1_2   0.70710678118654752440

// Usage
float circumference = 2.0f * M_PI * radius;
float area = M_PI * radius * radius;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Unicode and Wide Character Support</h2>
    
    <h3>UTF-8 String Functions</h3>
    <div class="code-block">
        <pre><code>#include &lt;uchar.h&gt;

// UTF-8 to UTF-16 conversion
char8_t utf8_str[] = u8"Hello, 世界!";
char16_t utf16_buf[256];
size_t utf16_len;

mbstate_t state = {0};
const char8_t* src = utf8_str;
char16_t* dst = utf16_buf;

size_t result = mbrtoc16(dst, src, strlen((char*)src), &state);
if (result != (size_t)-1 && result != (size_t)-2) {
    // Conversion successful
}

// UTF-16 to UTF-8 conversion
char16_t utf16_str[] = u"Hello, 世界!";
char8_t utf8_buf[256];
char* dst8 = (char*)utf8_buf;

result = c16rtomb(dst8, utf16_str[0], &state);
if (result != (size_t)-1) {
    // Conversion successful
}</code></pre>
    </div>
    
    <h3>Character Classification</h3>
    <div class="code-block">
        <pre><code>#include &lt;ctype.h&gt;

// Improved character classification
if (isalnum_l(ch, locale)) {
    // Alphanumeric in locale
}

if (isblank(ch)) {
    // Blank character (space or tab)
}

// Unicode-aware character functions
#include &lt;wctype.h&gt;

if (iswalpha(ch)) {
    // Wide character is alphabetic
}

wchar_t upper = towupper(ch);  // Convert to uppercase</code></pre>
    </div>
</div>

<div class="section">
    <h2>Time Functions</h2>
    
    <h3>Improved Time Handling</h3>
    <div class="code-block">
        <pre><code>#include &lt;time.h&gt;

// Timespec for nanosecond precision
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);

// Convert timespec to milliseconds
long long ms = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;

// High-resolution timer
clock_gettime(CLOCK_MONOTONIC, &ts);

// Format time with better control
char time_str[64];
struct tm* tm_info = localtime(&ts.tv_sec);
strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Practical Examples in id Tech 3</h2>
    
    <h3>Safe String Formatting</h3>
    <div class="code-block">
        <pre><code>// Safe printf-style formatting
int Com_Printf_safe(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    
    int result = vsnprintf_s(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    if (result < 0) {
        buffer[0] = '\0';
        return -1;
    }
    
    // Output buffer
    Sys_Print(buffer);
    return result;
}</code></pre>
    </div>
    
    <h3>Safe Path Operations</h3>
    <div class="code-block">
        <pre><code>// Safe path building
qboolean FS_BuildPath(char* dest, size_t dest_size,
                      const char* base, const char* file) {
    if (strcpy_s(dest, dest_size, base) != 0) {
        return qfalse;
    }
    
    size_t base_len;
    strnlen_s(dest, dest_size, &base_len);
    
    // Check if we need a separator
    if (base[base_len - 1] != '/') {
        if (strcat_s(dest, dest_size, "/") != 0) {
            return qfalse;
        }
    }
    
    if (strcat_s(dest, dest_size, file) != 0) {
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
    
    <h3>Aligned Memory for SIMD</h3>
    <div class="code-block">
        <pre><code>// Allocate aligned memory for SIMD operations
vec4_t* AllocAlignedVectors(size_t count) {
    // Align to 16 bytes for SSE/NEON
    void* ptr = aligned_alloc(16, sizeof(vec4_t) * count);
    
    if (!ptr) {
        return NULL;
    }
    
    // Zero-initialize
    memset_s(ptr, sizeof(vec4_t) * count, 0, sizeof(vec4_t) * count);
    
    return (vec4_t*)ptr;
}</code></pre>
    </div>
    
    <h3>High-Resolution Timing</h3>
    <div class="code-block">
        <pre><code>// High-resolution frame timing
#include &lt;time.h&gt;

static struct timespec frame_start;

void Frame_Start(void) {
    clock_gettime(CLOCK_MONOTONIC, &frame_start);
}

double Frame_End(void) {
    struct timespec frame_end;
    clock_gettime(CLOCK_MONOTONIC, &frame_end);
    
    long long delta_ns = (frame_end.tv_sec - frame_start.tv_sec) * 1000000000LL +
                         (frame_end.tv_nsec - frame_start.tv_nsec);
    
    return delta_ns / 1000000000.0;  // Return seconds
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Error Handling</h2>
    
    <h3>errno_t Type</h3>
    <div class="code-block">
        <pre><code>#include &lt;errno.h&gt;

// errno_t for error codes
errno_t result = strcpy_s(buffer, size, source);

if (result != 0) {
    switch (result) {
        case EINVAL:
            // Invalid argument
            break;
        case ERANGE:
            // Buffer too small
            break;
        case EOVERFLOW:
            // Overflow occurred
            break;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Best Practices</h2>
    
    <ul>
        <li><strong>Use bounds-checked functions</strong> for all string and memory operations in security-critical code</li>
        <li><strong>Always check return values</strong> from standard library functions</li>
        <li><strong>Use aligned_alloc</strong> for SIMD operations and cache-aligned data structures</li>
        <li><strong>Prefer fma</strong> for floating-point operations requiring precision</li>
        <li><strong>Use UTF-8</strong> for internal string representation, convert to UTF-16/32 only when necessary</li>
        <li><strong>Use CLOCK_MONOTONIC</strong> for timing measurements, not CLOCK_REALTIME</li>
    </ul>
</div>

<div class="section">
    <h2>Migration Guide</h2>
    
    <h3>Replacing Unsafe Functions</h3>
    <div class="code-block">
        <pre><code>// Old: Unsafe
strcpy(buffer, source);
strcat(path, file);
sprintf(buffer, fmt, ...);

// New: Safe
strcpy_s(buffer, sizeof(buffer), source);
strcat_s(path, sizeof(path), file);
snprintf_s(buffer, sizeof(buffer), fmt, ...);</code></pre>
    </div>
    
    <h3>Adding Bounds Checking</h3>
    <div class="code-block">
        <pre><code>// Wrap unsafe functions with bounds checking
#ifdef DEBUG
    #define SAFE_STRCPY(dst, src) \
        do { \
            if (strcpy_s(dst, sizeof(dst), src) != 0) { \
                Com_Error(ERR_FATAL, "String copy failed"); \
            } \
        } while(0)
#else
    #define SAFE_STRCPY(dst, src) strcpy(dst, src)
#endif</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/tutorials/c23-overview">C23 Overview</a></li>
        <li><a href="/tutorials/c23-memory-safety">C23 Memory Safety</a></li>
        <li><a href="/core/memory-management">Memory Management</a></li>
    </ul>
</div>
