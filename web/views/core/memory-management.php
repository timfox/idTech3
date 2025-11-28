<?php
/**
 * Memory Management - id Tech 3 Engine
 */
$title = 'Memory Management - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/memory-management' => 'Memory Management'
];
?>

<h1>Memory Management in id Tech 3</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine uses a sophisticated memory management system designed for real-time performance and minimal fragmentation. Understanding these systems is crucial for optimization and preventing memory-related crashes.</p>
    
    <div class="feature-list">
        <h3>Memory Management Systems</h3>
        <ul>
            <li><strong>Hunk Allocator:</strong> Large block allocator for persistent game data</li>
            <li><strong>Zone Memory:</strong> General-purpose allocator for temporary data</li>
            <li><strong>Stack Allocator:</strong> Fast allocation for frame-temporary data</li>
            <li><strong>Static Memory:</strong> Compile-time allocated global structures</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Hunk Memory System</h2>
    
    <h3>Hunk Architecture</h3>
    <div class="code-block">
        <pre><code>// q_shared.h - Hunk memory allocator
// Large contiguous memory block for game data

#define HUNK_MAGIC 0x89537892
#define HUNKBLOCK_MAGIC 0x89537893

typedef struct {
    int magic;
    int size;
} hunkHeader_t;

typedef struct hunkblock_s {
    int size;
    byte printed;
    struct hunkblock_s* next;
    char* label;
    char* file;
    int line;
} hunkblock_t;

// Hunk state
typedef struct {
    byte* base;          // Base memory address
    int size;            // Total hunk size
    int used;            // Bytes currently used
    int tempHighwater;   // High water mark for temp allocations
    int permanent;       // Permanent allocation boundary
    int temp;            // Temporary allocation start
    hunkblock_t* blocks; // Block tracking for debugging
} hunk_t;

static hunk_t hunk_main;
static hunk_t* hunk = &hunk_main;

// Initialize hunk memory system
void Hunk_Init(void) {
    cvar_t* cv = Cvar_Get("com_hunkMegs", "56", CVAR_LATCH | CVAR_ARCHIVE);
    
    // Allocate main hunk
    hunk->size = cv->integer * 1024 * 1024;
    hunk->base = malloc(hunk->size);
    
    if (!hunk->base) {
        Sys_Error("Hunk_Init: failed to allocate %d MB", cv->integer);
    }
    
    hunk->used = 0;
    hunk->tempHighwater = 0;
    hunk->permanent = 0;
    hunk->temp = hunk->size;
    hunk->blocks = NULL;
    
    Com_Printf("Hunk_Init: %d MB allocated\n", cv->integer);
}

// Allocate permanent memory from bottom of hunk
void* Hunk_Alloc(int size, ha_pref preference) {
    void* buf;
    
    if (hunk->temp - hunk->permanent < size) {
        Sys_Error("Hunk_Alloc failed on %i bytes", size);
    }
    
    // Align to pointer boundary
    size = (size + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);
    
    buf = hunk->base + hunk->permanent;
    hunk->permanent += size;
    hunk->used += size;
    
    // Clear allocated memory
    memset(buf, 0, size);
    
    return buf;
}</code></pre>
    </div>
    
    <h3>Temporary Hunk Allocations</h3>
    <div class="code-block">
        <pre><code>// Temporary allocations from top of hunk (grow downward)
void* Hunk_AllocateTempMemory(int size) {
    void* buf;
    hunkHeader_t* hdr;
    
    // Round up to multiple of sizeof(intptr_t)
    size = (size + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);
    size += sizeof(hunkHeader_t);
    
    if (hunk->temp - hunk->permanent < size) {
        Sys_Error("Hunk_AllocateTempMemory: failed on %d bytes", size);
    }
    
    hunk->temp -= size;
    hunk->tempHighwater += size;
    
    hdr = (hunkHeader_t*)(hunk->base + hunk->temp);
    buf = (void*)(hdr + 1);
    
    hdr->magic = HUNK_MAGIC;
    hdr->size = size;
    
    // Clear allocated memory
    memset(buf, 0, size - sizeof(hunkHeader_t));
    
    return buf;
}

void Hunk_FreeTempMemory(void* buf) {
    hunkHeader_t* hdr;
    
    if (!buf) {
        return;
    }
    
    hdr = ((hunkHeader_t*)buf) - 1;
    if (hdr->magic != HUNK_MAGIC) {
        Sys_Error("Hunk_FreeTempMemory: bad magic");
    }
    
    hdr->magic = 0; // Mark as freed
    
    // Free from top of stack
    if ((byte*)hdr == hunk->base + hunk->temp) {
        hunk->temp += hdr->size;
        hunk->tempHighwater -= hdr->size;
    } else {
        // Not top of stack - just mark as freed
        // Will be reclaimed on next clear
    }
}

// Clear all temporary allocations
void Hunk_ClearTempMemory(void) {
    hunk->temp = hunk->size;
    hunk->tempHighwater = 0;
}

// Clear to a previous high water mark
void Hunk_ClearToMark(void) {
    hunk->used = hunk->permanent;
}</code></pre>
    </div>
    
    <h3>Hunk Debug and Analysis</h3>
    <div class="code-block">
        <pre><code>// Memory debugging and leak detection
void Hunk_Log(void) {
    hunkblock_t* block;
    char buf[4096];
    int size, numBlocks;
    
    if (!logfile) {
        return;
    }
    
    size = 0;
    numBlocks = 0;
    
    Com_sprintf(buf, sizeof(buf), "\r\n================\r\n");
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "Hunk log\r\n");
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "================\r\n");
    FS_Write(buf, strlen(buf), logfile);
    
    for (block = hunk->blocks; block; block = block->next) {
        Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", 
                   block->size, block->file, block->line, block->label);
        FS_Write(buf, strlen(buf), logfile);
        size += block->size;
        numBlocks++;
    }
    
    Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
    FS_Write(buf, strlen(buf), logfile);
}

void Hunk_SmallLog(void) {
    hunkblock_t* block, *block2;
    char buf[4096];
    int size, locsize, numBlocks;
    
    if (!logfile) {
        return;
    }
    
    for (block = hunk->blocks; block; block = block->next) {
        block->printed = qfalse;
    }
    
    size = 0;
    numBlocks = 0;
    
    for (block = hunk->blocks; block; block = block->next) {
        if (block->printed) {
            continue;
        }
        
        locsize = block->size;
        for (block2 = block->next; block2; block2 = block2->next) {
            if (block->line != block2->line) {
                continue;
            }
            if (Q_stricmp(block->file, block2->file)) {
                continue;
            }
            
            size += block2->size;
            locsize += block2->size;
            block2->printed = qtrue;
        }
        
        Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d\r\n", 
                   locsize, block->file, block->line);
        FS_Write(buf, strlen(buf), logfile);
        
        size += block->size;
        numBlocks++;
        block->printed = qtrue;
    }
    
    Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
    FS_Write(buf, strlen(buf), logfile);
}

// Memory status reporting
int Hunk_MemoryRemaining(void) {
    return hunk->size - hunk->permanent - hunk->tempHighwater;
}

void Hunk_Meminfo_f(void) {
    Com_Printf("Hunk memory info:\n");
    Com_Printf("  Total: %d MB\n", hunk->size / (1024 * 1024));
    Com_Printf("  Used: %d MB\n", hunk->used / (1024 * 1024));
    Com_Printf("  Permanent: %d MB\n", hunk->permanent / (1024 * 1024));
    Com_Printf("  Temp high water: %d KB\n", hunk->tempHighwater / 1024);
    Com_Printf("  Remaining: %d MB\n", Hunk_MemoryRemaining() / (1024 * 1024));
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Zone Memory System</h2>
    
    <h3>Zone Allocator Implementation</h3>
    <div class="code-block">
        <pre><code>// Zone memory - general purpose malloc/free replacement
#define ZONE_MAGIC 0x1d4a11
#define ZONEID 0x1d4a11

typedef struct zonedebug_s {
    char* label;
    char* file;
    int line;
    int allocSize;
    struct zonedebug_s* next;
} zonedebug_t;

typedef struct memblock_s {
    int size;           // Including the header and possibly tiny fragments
    int tag;            // A tag of 0 is a free block
    struct memblock_s* next, *prev;
    int id;             // Should be ZONEID
#ifdef ZONE_DEBUG
    zonedebug_t* d;
#endif
} memblock_t;

typedef struct {
    int size;           // Total bytes malloced, including header
    int used;           // Total bytes used
    memblock_t blocklist; // Start/end cap for linked list
    memblock_t* rover;
} memzone_t;

static memzone_t* mainzone;

void Z_InitZone(void) {
    memblock_t* block;
    int size = 4 * 1024 * 1024; // 4MB default zone
    
    mainzone = malloc(sizeof(memzone_t));
    mainzone->size = size;
    mainzone->used = 0;
    
    // Set up the blocklist as a loop going both ways
    mainzone->blocklist.next = mainzone->blocklist.prev = 
        block = (memblock_t*)((byte*)mainzone + sizeof(memzone_t));
    
    mainzone->blocklist.tag = 1; // in use block
    mainzone->blocklist.id = 0;
    mainzone->blocklist.size = 0;
    mainzone->rover = block;
    
    block->prev = block->next = &mainzone->blocklist;
    block->tag = 0; // free block
    block->id = ZONEID;
    block->size = size - sizeof(memzone_t);
}

void* Z_Malloc(int size) {
    void* buf;
    Z_CheckHeap();
    buf = Z_TagMalloc(size, TAG_GENERAL);
    memset(buf, 0, size);
    return buf;
}

void* Z_TagMalloc(int size, int tag) {
    memblock_t* start, *rover, *new, *base;
    int extra;
    
    if (!tag) {
        Com_Error(ERR_FATAL, "Z_TagMalloc: tried to use a 0 tag");
    }
    
    if (!size) {
        return NULL;
    }
    
    // Scan through the block list looking for the first free block
    // of sufficient size, throwing out any purgable blocks along the way
    
    // Account for size of block header
    size += sizeof(memblock_t);
    
    // If there is a free block behind the rover, back up over them
    base = rover = mainzone->rover;
    
    if (!rover->prev->tag) {
        rover = rover->prev;
    }
    
    start = rover;
    do {
        if (rover == start) {
            // Scanned all the way around the list
            base = mainzone->rover = &mainzone->blocklist;
            break;
        }
        
        if (rover->tag) {
            base = rover = rover->next;
        } else {
            rover = rover->next;
        }
    } while (base->tag || base->size < size);
    
    // Found a block big enough
    extra = base->size - size;
    if (extra > MINFRAGMENT) {
        // There will be a free fragment after the allocated block
        new = (memblock_t*)((byte*)base + size);
        new->size = extra;
        new->tag = 0; // free block
        new->prev = base;
        new->id = ZONEID;
        new->next = base->next;
        new->next->prev = new;
        base->next = new;
        base->size = size;
    }
    
    base->tag = tag; // no longer a free block
    mainzone->rover = base->next; // next allocation will start looking here
    mainzone->used += base->size;
    
    base->id = ZONEID;
    
    // Return pointer to data portion
    return (void*)((byte*)base + sizeof(memblock_t));
}</code></pre>
    </div>
    
    <h3>Zone Memory Debugging</h3>
    <div class="code-block">
        <pre><code>void Z_Free(void* ptr) {
    memblock_t* block, *other;
    
    if (!ptr) {
        Com_Error(ERR_DROP, "Z_Free: NULL pointer");
    }
    
    block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));
    if (block->id != ZONEID) {
        Com_Error(ERR_FATAL, "Z_Free: freed a pointer without ZONEID");
    }
    
    if (block->tag == 0) {
        Com_Error(ERR_FATAL, "Z_Free: freed a freed pointer");
    }
    
    // Clear the user data to trap references to freed memory
    memset(ptr, 0x50, block->size - sizeof(memblock_t));
    
    mainzone->used -= block->size;
    
    // Set the block to something that should cause problems
    // if it is referenced...
    block->tag = 0; // mark as free
    block->id = 0;
    
    other = block->prev;
    if (!other->tag) {
        // Merge with previous free block
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;
        
        if (block == mainzone->rover) {
            mainzone->rover = other;
        }
        block = other;
    }
    
    other = block->next;
    if (!other->tag) {
        // Merge the next free block onto the end
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;
        
        if (other == mainzone->rover) {
            mainzone->rover = block;
        }
    }
}

void Z_CheckHeap(void) {
    memblock_t* block;
    
    for (block = mainzone->blocklist.next; block->next != &mainzone->blocklist; 
         block = block->next) {
        if ((byte*)block + block->size != (byte*)block->next) {
            Com_Error(ERR_FATAL, "Z_CheckHeap: block size does not touch the next block");
        }
        if (block->next->prev != block) {
            Com_Error(ERR_FATAL, "Z_CheckHeap: next block doesn't have proper back link");
        }
        if (!block->tag && !block->next->tag) {
            Com_Error(ERR_FATAL, "Z_CheckHeap: two consecutive free blocks");
        }
    }
}

void Z_LogZoneHeap(void) {
    memblock_t* block;
    char buf[4096];
    int size, numBlocks;
    
    if (!logfile) {
        return;
    }
    
    size = numBlocks = 0;
    Com_sprintf(buf, sizeof(buf), "\r\n================\r\n");
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "Zone log\r\n");
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "================\r\n");
    FS_Write(buf, strlen(buf), logfile);
    
    for (block = mainzone->blocklist.next; block->next != &mainzone->blocklist; 
         block = block->next) {
        if (block->tag) {
#ifdef ZONE_DEBUG
            Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) *(MEMINFO)\r\n", 
                       block->size, block->d->file, block->d->line, block->d->label);
            FS_Write(buf, strlen(buf), logfile);
#endif
            size += block->size;
            numBlocks++;
        }
    }
    
    Com_sprintf(buf, sizeof(buf), "%d zone memory\r\n", size);
    FS_Write(buf, strlen(buf), logfile);
    Com_sprintf(buf, sizeof(buf), "%d zone blocks\r\n", numBlocks);
    FS_Write(buf, strlen(buf), logfile);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Stack Memory System</h2>
    
    <h3>Fast Stack Allocator</h3>
    <div class="code-block">
        <pre><code>// Stack allocator for very fast frame-temporary allocations
#define STACK_SIZE (512 * 1024) // 512KB stack

typedef struct {
    byte* base;
    byte* top;
    int size;
    int mark;
} memstack_t;

static memstack_t memstack;

void Stack_Init(void) {
    memstack.size = STACK_SIZE;
    memstack.base = malloc(memstack.size);
    memstack.top = memstack.base;
    memstack.mark = 0;
    
    if (!memstack.base) {
        Sys_Error("Stack_Init: failed to allocate %d KB", STACK_SIZE / 1024);
    }
    
    Com_Printf("Stack_Init: %d KB allocated\n", STACK_SIZE / 1024);
}

void* Stack_Alloc(int size) {
    void* ptr;
    
    // Align to pointer boundary
    size = (size + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);
    
    if (memstack.top + size > memstack.base + memstack.size) {
        Com_Error(ERR_DROP, "Stack_Alloc: overflow (%d bytes)", size);
    }
    
    ptr = memstack.top;
    memstack.top += size;
    
    return ptr;
}

int Stack_GetMark(void) {
    return memstack.top - memstack.base;
}

void Stack_FreeToMark(int mark) {
    if (mark < 0 || mark > memstack.size) {
        Com_Error(ERR_DROP, "Stack_FreeToMark: bad mark %d", mark);
    }
    
    memstack.top = memstack.base + mark;
}

void Stack_Clear(void) {
    memstack.top = memstack.base;
}

// Automatic scope-based stack management
typedef struct stackScope_s {
    int mark;
    struct stackScope_s* prev;
} stackScope_t;

static stackScope_t* currentScope = NULL;

void Stack_PushScope(void) {
    stackScope_t* scope = Stack_Alloc(sizeof(stackScope_t));
    scope->mark = Stack_GetMark() - sizeof(stackScope_t);
    scope->prev = currentScope;
    currentScope = scope;
}

void Stack_PopScope(void) {
    if (!currentScope) {
        Com_Error(ERR_DROP, "Stack_PopScope: no scope to pop");
    }
    
    int mark = currentScope->mark;
    currentScope = currentScope->prev;
    Stack_FreeToMark(mark);
}

// RAII-style scope management (C++ style)
#define STACK_SCOPE() \
    Stack_PushScope(); \
    defer(Stack_PopScope())

// Usage example:
void SomeFunction(void) {
    STACK_SCOPE();
    
    char* tempBuffer = Stack_Alloc(1024);
    int* tempArray = Stack_Alloc(sizeof(int) * 100);
    
    // Do work with temporary allocations
    
    // Automatic cleanup when function exits
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Tags and Tracking</h2>
    
    <h3>Memory Tag System</h3>
    <div class="code-block">
        <pre><code>// Memory tagging for tracking different allocation types
typedef enum {
    TAG_FREE,           // Should never be used
    TAG_GENERAL,        // General purpose
    TAG_BOTLIB,         // Bot library
    TAG_RENDERER,       // Renderer allocations
    TAG_SMALL,          // Small allocations
    TAG_STATIC,         // Static game data
    TAG_CLIENTS,        // Client-specific data
    TAG_SERVERGAME,     // Server game data
    TAG_CGAME,          // Client game data
    TAG_UI,             // User interface
    TAG_SOUND,          // Sound system
    TAG_CLIPMAP,        // Collision map
    TAG_BSP,            // BSP world data
    TAG_SHADERS,        // Shader system
    TAG_TEXTURES,       // Texture data
    TAG_MODELS,         // Model data
    TAG_ANIMATIONS,     // Animation data
    TAG_FILES,          // File system
    TAG_COUNT           // Total number of tags
} memtag_t;

static const char* tagNames[TAG_COUNT] = {
    "FREE", "GENERAL", "BOTLIB", "RENDERER", "SMALL", "STATIC",
    "CLIENTS", "SERVERGAME", "CGAME", "UI", "SOUND", "CLIPMAP",
    "BSP", "SHADERS", "TEXTURES", "MODELS", "ANIMATIONS", "FILES"
};

// Memory statistics per tag
typedef struct {
    int allocations;    // Number of allocations
    int size;          // Total bytes allocated
    int peak;          // Peak memory usage
    int blocks;        // Number of blocks
} memstat_t;

static memstat_t memstats[TAG_COUNT];

void* Z_TagMallocDebug(int size, int tag, const char* label, 
                       const char* file, int line) {
    void* ptr = Z_TagMalloc(size, tag);
    
    // Update statistics
    memstats[tag].allocations++;
    memstats[tag].size += size;
    memstats[tag].blocks++;
    
    if (memstats[tag].size > memstats[tag].peak) {
        memstats[tag].peak = memstats[tag].size;
    }
    
#ifdef ZONE_DEBUG
    // Store debug information
    zonedebug_t* d = malloc(sizeof(zonedebug_t));
    d->label = CopyString(label);
    d->file = CopyString(file);
    d->line = line;
    d->allocSize = size;
    
    memblock_t* block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));
    block->d = d;
#endif
    
    return ptr;
}

void Z_TagFree(void* ptr) {
    if (!ptr) {
        return;
    }
    
    memblock_t* block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));
    int tag = block->tag;
    int size = block->size - sizeof(memblock_t);
    
    // Update statistics
    memstats[tag].size -= size;
    memstats[tag].blocks--;
    
#ifdef ZONE_DEBUG
    if (block->d) {
        free(block->d->label);
        free(block->d->file);
        free(block->d);
    }
#endif
    
    Z_Free(ptr);
}

// Memory usage reporting by tag
void Z_DisplayMemoryUsage_f(void) {
    Com_Printf("Memory usage by tag:\n");
    Com_Printf("%-12s %8s %8s %8s %8s\n", 
              "Tag", "Allocs", "Blocks", "Size", "Peak");
    Com_Printf("%-12s %8s %8s %8s %8s\n", 
              "---", "------", "------", "----", "----");
    
    int totalSize = 0, totalBlocks = 0, totalAllocs = 0;
    
    for (int i = 1; i < TAG_COUNT; i++) {
        if (memstats[i].allocations > 0) {
            Com_Printf("%-12s %8d %8d %7dK %7dK\n",
                      tagNames[i],
                      memstats[i].allocations,
                      memstats[i].blocks,
                      memstats[i].size / 1024,
                      memstats[i].peak / 1024);
            
            totalSize += memstats[i].size;
            totalBlocks += memstats[i].blocks;
            totalAllocs += memstats[i].allocations;
        }
    }
    
    Com_Printf("%-12s %8s %8s %8s %8s\n", 
              "---", "------", "------", "----", "----");
    Com_Printf("%-12s %8d %8d %7dK\n",
              "TOTAL", totalAllocs, totalBlocks, totalSize / 1024);
}</code></pre>
    </div>
    
    <h3>Memory Leak Detection</h3>
    <div class="code-block">
        <pre><code>// Memory leak detection and reporting
typedef struct memleak_s {
    void* ptr;
    int size;
    int tag;
    char* file;
    int line;
    char* label;
    int allocTime;
    struct memleak_s* next;
} memleak_t;

static memleak_t* memleaks = NULL;
static int memleakCount = 0;

void Z_TrackAllocation(void* ptr, int size, int tag, 
                      const char* file, int line, const char* label) {
    memleak_t* leak = malloc(sizeof(memleak_t));
    leak->ptr = ptr;
    leak->size = size;
    leak->tag = tag;
    leak->file = CopyString(file);
    leak->line = line;
    leak->label = CopyString(label);
    leak->allocTime = Sys_Milliseconds();
    
    leak->next = memleaks;
    memleaks = leak;
    memleakCount++;
}

void Z_UntrackAllocation(void* ptr) {
    memleak_t** current = &memleaks;
    
    while (*current) {
        if ((*current)->ptr == ptr) {
            memleak_t* leak = *current;
            *current = leak->next;
            
            free(leak->file);
            free(leak->label);
            free(leak);
            memleakCount--;
            return;
        }
        current = &(*current)->next;
    }
}

void Z_CheckForLeaks(void) {
    if (memleakCount == 0) {
        Com_Printf("No memory leaks detected\n");
        return;
    }
    
    Com_Printf("^1WARNING: %d memory leaks detected!\n", memleakCount);
    
    memleak_t* leak = memleaks;
    while (leak) {
        int age = Sys_Milliseconds() - leak->allocTime;
        Com_Printf("  %s:%d - %d bytes (%s) - %d ms old\n",
                  leak->file, leak->line, leak->size, 
                  tagNames[leak->tag], age);
        leak = leak->next;
    }
}

// Periodic leak checking
void Z_PeriodicLeakCheck(void) {
    static int lastCheck = 0;
    int currentTime = Sys_Milliseconds();
    
    if (currentTime - lastCheck > 30000) { // Check every 30 seconds
        Z_CheckForLeaks();
        lastCheck = currentTime;
    }
}

// Memory validation
qboolean Z_ValidatePointer(void* ptr) {
    if (!ptr) {
        return qfalse;
    }
    
    memblock_t* block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));
    
    // Check magic number
    if (block->id != ZONEID) {
        return qfalse;
    }
    
    // Check if it's a free block
    if (block->tag == 0) {
        return qfalse;
    }
    
    // Verify block is within zone bounds
    if ((byte*)block < (byte*)mainzone || 
        (byte*)block >= (byte*)mainzone + mainzone->size) {
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Optimization Strategies</h2>
    
    <h3>Memory Pool Allocators</h3>
    <div class="code-block">
        <pre><code>// Specialized pool allocators for frequent small allocations
typedef struct mempool_s {
    char name[32];
    void* blocks;
    int blockSize;
    int numBlocks;
    int used;
    int peak;
    struct mempool_s* next;
} mempool_t;

static mempool_t* pools = NULL;

mempool_t* Pool_Create(const char* name, int blockSize, int numBlocks) {
    mempool_t* pool = Z_Malloc(sizeof(mempool_t));
    
    Q_strncpyz(pool->name, name, sizeof(pool->name));
    pool->blockSize = blockSize;
    pool->numBlocks = numBlocks;
    pool->used = 0;
    pool->peak = 0;
    
    // Allocate block storage + free list pointers
    int totalSize = (blockSize + sizeof(void*)) * numBlocks;
    pool->blocks = Z_Malloc(totalSize);
    
    // Initialize free list
    byte* block = (byte*)pool->blocks;
    for (int i = 0; i < numBlocks - 1; i++) {
        *(void**)block = block + blockSize + sizeof(void*);
        block += blockSize + sizeof(void*);
    }
    *(void**)block = NULL; // Last block
    
    // Add to global pool list
    pool->next = pools;
    pools = pool;
    
    Com_Printf("Pool_Create: %s - %d blocks of %d bytes\n", 
              name, numBlocks, blockSize);
    
    return pool;
}

void* Pool_Alloc(mempool_t* pool) {
    if (!pool->blocks) {
        return NULL; // Pool exhausted
    }
    
    // Get next free block
    void* block = pool->blocks;
    pool->blocks = *(void**)block;
    
    pool->used++;
    if (pool->used > pool->peak) {
        pool->peak = pool->used;
    }
    
    // Clear block data (skip free list pointer)
    memset((byte*)block + sizeof(void*), 0, pool->blockSize);
    
    return (byte*)block + sizeof(void*);
}

void Pool_Free(mempool_t* pool, void* ptr) {
    if (!ptr) {
        return;
    }
    
    // Get actual block start (before user data)
    void* block = (byte*)ptr - sizeof(void*);
    
    // Add to free list
    *(void**)block = pool->blocks;
    pool->blocks = block;
    
    pool->used--;
}

// Specialized pools for common allocations
static mempool_t* entityPool;
static mempool_t* tempEntityPool;
static mempool_t* stringPool;

void Mem_InitPools(void) {
    entityPool = Pool_Create("entities", sizeof(gentity_t), MAX_GENTITIES);
    tempEntityPool = Pool_Create("temp_entities", sizeof(localEntity_t), MAX_LOCAL_ENTITIES);
    stringPool = Pool_Create("strings", 64, 1024);
}

// Pool usage statistics
void Pool_Stats_f(void) {
    Com_Printf("Memory pool statistics:\n");
    Com_Printf("%-16s %8s %8s %8s %8s\n", 
              "Pool", "Size", "Count", "Used", "Peak");
    Com_Printf("%-16s %8s %8s %8s %8s\n", 
              "----", "----", "-----", "----", "----");
    
    for (mempool_t* pool = pools; pool; pool = pool->next) {
        Com_Printf("%-16s %8d %8d %8d %8d\n",
                  pool->name, pool->blockSize, pool->numBlocks,
                  pool->used, pool->peak);
    }
}</code></pre>
    </div>
    
    <h3>Garbage Collection System</h3>
    <div class="code-block">
        <pre><code>// Mark-and-sweep garbage collection for referenced data
typedef struct gcobject_s {
    int mark;               // GC mark bit
    int size;               // Object size
    int refCount;           // Reference counter
    void (*destructor)(struct gcobject_s*); // Cleanup function
    struct gcobject_s* next; // GC object list
} gcobject_t;

static gcobject_t* gcObjects = NULL;
static int gcGeneration = 1;

gcobject_t* GC_Alloc(int size, void (*destructor)(gcobject_t*)) {
    gcobject_t* obj = Z_Malloc(sizeof(gcobject_t) + size);
    
    obj->mark = 0;
    obj->size = size;
    obj->refCount = 1;
    obj->destructor = destructor;
    obj->next = gcObjects;
    gcObjects = obj;
    
    return obj;
}

void GC_AddRef(gcobject_t* obj) {
    if (obj) {
        obj->refCount++;
    }
}

void GC_Release(gcobject_t* obj) {
    if (obj && --obj->refCount <= 0) {
        // Mark for collection
        obj->mark = -1;
    }
}

void GC_MarkObject(gcobject_t* obj) {
    if (obj && obj->mark != gcGeneration) {
        obj->mark = gcGeneration;
        // Mark referenced objects here
    }
}

void GC_MarkPhase(void) {
    // Mark all root objects
    // This would mark entities, textures, models, etc.
    // that are currently in use
    
    for (int i = 0; i < MAX_GENTITIES; i++) {
        if (g_entities[i].inuse) {
            GC_MarkObject((gcobject_t*)g_entities[i].gcData);
        }
    }
    
    // Mark renderer objects
    for (int i = 0; i < tr.numTextures; i++) {
        GC_MarkObject((gcobject_t*)tr.textures[i].gcData);
    }
}

void GC_SweepPhase(void) {
    gcobject_t** current = &gcObjects;
    int collected = 0;
    int collectedSize = 0;
    
    while (*current) {
        gcobject_t* obj = *current;
        
        if (obj->mark != gcGeneration || obj->refCount <= 0) {
            // Object not marked or has no references - collect it
            *current = obj->next;
            
            if (obj->destructor) {
                obj->destructor(obj);
            }
            
            collectedSize += obj->size;
            collected++;
            Z_Free(obj);
        } else {
            current = &obj->next;
        }
    }
    
    if (collected > 0) {
        Com_Printf("GC: Collected %d objects (%d KB)\n", 
                  collected, collectedSize / 1024);
    }
}

void GC_Collect(void) {
    int startTime = Sys_Milliseconds();
    
    gcGeneration++;
    GC_MarkPhase();
    GC_SweepPhase();
    
    int elapsed = Sys_Milliseconds() - startTime;
    Com_DPrintf("GC completed in %d ms\n", elapsed);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Safety and Debugging</h2>
    
    <h3>Memory Corruption Detection</h3>
    <div class="code-block">
        <pre><code>// Memory corruption detection using guard patterns
#define GUARD_PATTERN 0xDEADBEEF
#define GUARD_SIZE 16

typedef struct {
    uint32_t frontGuard[GUARD_SIZE/4];
    // User data goes here
    // uint32_t backGuard[GUARD_SIZE/4]; // At end of allocation
} guardedAlloc_t;

void* Z_MallocGuarded(int size) {
    int totalSize = size + 2 * GUARD_SIZE;
    guardedAlloc_t* alloc = Z_Malloc(totalSize);
    
    // Set front guard
    for (int i = 0; i < GUARD_SIZE/4; i++) {
        alloc->frontGuard[i] = GUARD_PATTERN;
    }
    
    // Set back guard
    uint32_t* backGuard = (uint32_t*)((byte*)alloc + GUARD_SIZE + size);
    for (int i = 0; i < GUARD_SIZE/4; i++) {
        backGuard[i] = GUARD_PATTERN;
    }
    
    return (byte*)alloc + GUARD_SIZE;
}

qboolean Z_CheckGuards(void* ptr) {
    if (!ptr) {
        return qfalse;
    }
    
    guardedAlloc_t* alloc = (guardedAlloc_t*)((byte*)ptr - GUARD_SIZE);
    
    // Check front guard
    for (int i = 0; i < GUARD_SIZE/4; i++) {
        if (alloc->frontGuard[i] != GUARD_PATTERN) {
            Com_Printf("^1Memory corruption: front guard violated at %p\n", ptr);
            return qfalse;
        }
    }
    
    // Check back guard (requires knowing size - store in header)
    memblock_t* block = (memblock_t*)((byte*)alloc - sizeof(memblock_t));
    int userSize = block->size - sizeof(memblock_t) - 2 * GUARD_SIZE;
    
    uint32_t* backGuard = (uint32_t*)((byte*)ptr + userSize);
    for (int i = 0; i < GUARD_SIZE/4; i++) {
        if (backGuard[i] != GUARD_PATTERN) {
            Com_Printf("^1Memory corruption: back guard violated at %p\n", ptr);
            return qfalse;
        }
    }
    
    return qtrue;
}

// Automatic guard checking on free
void Z_FreeGuarded(void* ptr) {
    if (Z_CheckGuards(ptr)) {
        Z_Free((byte*)ptr - GUARD_SIZE);
    } else {
        Com_Error(ERR_FATAL, "Memory corruption detected in Z_FreeGuarded");
    }
}

// Comprehensive memory validation
void Z_ValidateHeap(void) {
    // Check zone heap integrity
    Z_CheckHeap();
    
    // Check all guarded allocations
    memblock_t* block;
    int corruptCount = 0;
    
    for (block = mainzone->blocklist.next; 
         block->next != &mainzone->blocklist; 
         block = block->next) {
        
        if (block->tag && block->size > 2 * GUARD_SIZE + sizeof(memblock_t)) {
            void* userData = (byte*)block + sizeof(memblock_t) + GUARD_SIZE;
            if (!Z_CheckGuards(userData)) {
                corruptCount++;
            }
        }
    }
    
    if (corruptCount > 0) {
        Com_Error(ERR_FATAL, "Memory validation failed: %d corrupted blocks", 
                 corruptCount);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="core/main-loop">Main Loop Analysis</a></li>
        <li><a href="core/entity-system">Entity System</a></li>
        <li><a href="modernization/profiling-tools">Performance Profiling</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
    </ul>
</div>