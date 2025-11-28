<?php 
$title = "Debugging Guide - id Tech 3 Documentation";
$description = "Complete guide to debugging Quake III mods and identifying common issues";
$breadcrumbs = [
    '/development' => 'Development',
    '/development/debugging' => 'Debugging'
];
?>

<div class="content-section">
    <h1>Quake III Debugging Guide</h1>
    
    <blockquote>
        <strong>Debug Like a Pro:</strong> Effective debugging is essential for creating stable, high-quality mods. This guide covers tools, techniques, and common pitfalls in Q3 development.
    </blockquote>

    <h2>Essential Debugging Tools</h2>
    
    <h3>Built-in Console Commands</h3>
    <p>Quake III provides powerful debugging capabilities through console commands:</p>
    
    <table>
        <thead>
            <tr>
                <th>Command</th>
                <th>Function</th>
                <th>Usage</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><code>developer 1</code></td>
                <td>Enable developer mode</td>
                <td>Shows additional debug information</td>
            </tr>
            <tr>
                <td><code>com_speeds 1</code></td>
                <td>Performance metrics</td>
                <td>Display frame timing information</td>
            </tr>
            <tr>
                <td><code>r_showfps 1</code></td>
                <td>Frame rate display</td>
                <td>Monitor rendering performance</td>
            </tr>
            <tr>
                <td><code>g_debugMove 1</code></td>
                <td>Movement debugging</td>
                <td>Debug player physics and movement</td>
            </tr>
            <tr>
                <td><code>bot_debug 1</code></td>
                <td>Bot AI debugging</td>
                <td>Show bot decision making</td>
            </tr>
        </tbody>
    </table>

    <h3>Logging and Output</h3>
    <div class="example">
        <pre>// Custom debug output functions
void G_Printf(const char *fmt, ...) {
    va_list argptr;
    char text[1024];
    
    va_start(argptr, fmt);
    vsprintf(text, fmt, argptr);
    va_end(argptr);
    
    trap_Printf(text);
}

// Conditional debugging
#ifdef _DEBUG
    #define DEBUG_PRINT(x) G_Printf x
#else
    #define DEBUG_PRINT(x)
#endif

// Usage example
DEBUG_PRINT(("Player %s health: %d\n", ent->client->pers.netname, ent->health));</pre>
    </div>

    <h2>Common Debugging Scenarios</h2>
    
    <h3>Entity System Debugging</h3>
    <p>Entities are the backbone of Q3. Common issues include:</p>
    
    <h4>Entity Validation</h4>
    <div class="example">
        <pre>// Always validate entity pointers
qboolean G_ValidEntity(gentity_t *ent) {
    if (!ent) {
        G_Printf("WARNING: NULL entity pointer\n");
        return qfalse;
    }
    
    if (ent < g_entities || ent >= &g_entities[level.num_entities]) {
        G_Printf("WARNING: Entity out of bounds\n");
        return qfalse;
    }
    
    if (!ent->inuse) {
        G_Printf("WARNING: Entity not in use\n");
        return qfalse;
    }
    
    return qtrue;
}

// Usage
if (G_ValidEntity(target)) {
    // Safe to use target
}</pre>
    </div>

    <h3>Memory Management</h3>
    <p>Q3 uses custom memory allocation. Watch for these issues:</p>
    
    <ul>
        <li><strong>Memory Leaks:</strong> Forgetting to free allocated memory</li>
        <li><strong>Double Free:</strong> Freeing the same memory twice</li>
        <li><strong>Buffer Overruns:</strong> Writing past allocated boundaries</li>
        <li><strong>Use After Free:</strong> Accessing freed memory</li>
    </ul>

    <div class="example">
        <pre>// Safe string operations
void G_SafeStringCopy(char *dest, const char *src, int destSize) {
    if (!dest || !src || destSize <= 0) {
        return;
    }
    
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';  // Ensure null termination
}

// Memory initialization
gentity_t *ent = G_Spawn();
if (ent) {
    memset(ent, 0, sizeof(*ent));  // Clear all fields
}</pre>
    </div>

    <h2>Network Debugging</h2>
    
    <h3>Client-Server Synchronization</h3>
    <p>Network issues can be subtle and hard to reproduce:</p>
    
    <div class="example">
        <pre>// Debug network messages
void G_DebugClientUpdate(int clientNum) {
    gclient_t *client = &level.clients[clientNum];
    
    G_Printf("Client %d: ping=%d, rate=%d\n", 
             clientNum, client->ps.ping, client->rate);
    
    if (client->ps.ping > 999) {
        G_Printf("WARNING: High ping for client %d\n", clientNum);
    }
}</pre>
    </div>

    <h3>Event System Debugging</h3>
    <p>Track events between game and cgame modules:</p>
    
    <div class="example">
        <pre>// Debug entity events
void G_AddEvent(gentity_t *ent, int event, int eventParm) {
    DEBUG_PRINT(("Event %d added to entity %d (parm=%d)\n", 
                 event, ent->s.number, eventParm));
    
    if (ent->s.event == event && ent->s.eventParm == eventParm) {
        G_Printf("WARNING: Duplicate event %d\n", event);
    }
    
    ent->s.event = event;
    ent->s.eventParm = eventParm;
}</pre>
    </div>

    <h2>Performance Debugging</h2>
    
    <h3>Frame Rate Analysis</h3>
    <p>Identify performance bottlenecks:</p>
    
    <div class="example">
        <pre>// Simple profiling
typedef struct {
    char name[32];
    int startTime;
    int totalTime;
    int callCount;
} profiler_t;

profiler_t g_profilers[MAX_PROFILERS];

void G_ProfileStart(int id, const char *name) {
    if (id >= 0 && id < MAX_PROFILERS) {
        strcpy(g_profilers[id].name, name);
        g_profilers[id].startTime = trap_Milliseconds();
    }
}

void G_ProfileEnd(int id) {
    if (id >= 0 && id < MAX_PROFILERS) {
        int elapsed = trap_Milliseconds() - g_profilers[id].startTime;
        g_profilers[id].totalTime += elapsed;
        g_profilers[id].callCount++;
    }
}</pre>
    </div>

    <h3>Memory Usage Tracking</h3>
    <div class="example">
        <pre>// Track entity usage
void G_PrintEntityStats(void) {
    int i, used = 0, free = 0;
    
    for (i = 0; i < level.num_entities; i++) {
        if (g_entities[i].inuse) {
            used++;
        } else {
            free++;
        }
    }
    
    G_Printf("Entities: %d used, %d free, %d total\n", 
             used, free, level.num_entities);
}</pre>
    </div>

    <h2>Visual Debugging Techniques</h2>
    
    <h3>Drawing Debug Information</h3>
    <p>Use the cgame module for visual debugging:</p>
    
    <div class="example">
        <pre>// cgame debugging (cg_draw.c)
void CG_DrawBoundingBox(vec3_t mins, vec3_t maxs, vec3_t color) {
    vec3_t corners[8];
    int i;
    
    // Calculate all 8 corners of the bounding box
    for (i = 0; i < 8; i++) {
        corners[i][0] = (i & 1) ? maxs[0] : mins[0];
        corners[i][1] = (i & 2) ? maxs[1] : mins[1];
        corners[i][2] = (i & 4) ? maxs[2] : mins[2];
    }
    
    // Draw the wireframe box
    for (i = 0; i < 12; i++) {
        CG_DrawLine(corners[boxLines[i][0]], 
                   corners[boxLines[i][1]], color);
    }
}</pre>
    </div>

    <h2>Common Bug Patterns</h2>
    
    <h3>Weapon System Issues</h3>
    <table>
        <thead>
            <tr>
                <th>Problem</th>
                <th>Symptoms</th>
                <th>Solution</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td>Infinite ammo</td>
                <td>Ammo never decreases</td>
                <td>Check ammo decrement in Fire_Weapon</td>
            </tr>
            <tr>
                <td>Double damage</td>
                <td>Weapons do too much damage</td>
                <td>Verify damage isn't applied twice</td>
            </tr>
            <tr>
                <td>Projectile spawning</td>
                <td>Missiles appear at wrong location</td>
                <td>Check muzzle point calculation</td>
            </tr>
            <tr>
                <td>Hit detection</td>
                <td>Shots don't register</td>
                <td>Verify trace start/end points</td>
            </tr>
        </tbody>
    </table>

    <h3>Player Movement Problems</h3>
    <ul>
        <li><strong>Stuck Players:</strong> Check collision detection and clip brushes</li>
        <li><strong>Flying Players:</strong> Verify gravity settings and ground detection</li>
        <li><strong>Speed Issues:</strong> Check pmove parameters and ground friction</li>
        <li><strong>Jump Problems:</strong> Verify jump velocity and ground state</li>
    </ul>

    <h2>Debugging Workflow</h2>
    
    <ol>
        <li><strong>Reproduce the Issue:</strong> Create reliable test cases</li>
        <li><strong>Isolate the Problem:</strong> Narrow down to specific code sections</li>
        <li><strong>Add Logging:</strong> Insert debug output at key points</li>
        <li><strong>Use Breakpoints:</strong> Step through code execution</li>
        <li><strong>Check Assumptions:</strong> Verify your understanding of the code</li>
        <li><strong>Test Fixes:</strong> Ensure the solution doesn't break other features</li>
    </ol>

    <h2>Debug Build Configuration</h2>
    
    <div class="example">
        <pre>// Compiler flags for debugging
#ifdef _DEBUG
    #define DEBUG_BUILD 1
    #define ASSERT(x) if (!(x)) { G_Error("Assertion failed: " #x); }
    #define DEBUG_ONLY(x) x
#else
    #define DEBUG_BUILD 0
    #define ASSERT(x)
    #define DEBUG_ONLY(x)
#endif

// Usage examples
ASSERT(ent != NULL);
DEBUG_ONLY(G_Printf("Debug info: %d\n", value));</pre>
    </div>

    <h2>Testing Strategies</h2>
    
    <h3>Unit Testing</h3>
    <p>Test individual functions in isolation:</p>
    
    <div class="example">
        <pre>// Simple test framework
typedef struct {
    const char *name;
    qboolean (*testFunc)(void);
} test_t;

qboolean Test_VectorMath(void) {
    vec3_t a = {1, 0, 0};
    vec3_t b = {0, 1, 0};
    vec3_t result;
    
    CrossProduct(a, b, result);
    
    // Expected result: {0, 0, 1}
    return (result[0] == 0 && result[1] == 0 && result[2] == 1);
}

test_t g_tests[] = {
    {"Vector Math", Test_VectorMath},
    // Add more tests...
};</pre>
    </div>

    <h3>Integration Testing</h3>
    <ul>
        <li><strong>Bot Testing:</strong> Use bots for automated gameplay testing</li>
        <li><strong>Map Testing:</strong> Test on different maps and environments</li>
        <li><strong>Network Testing:</strong> Test with multiple clients and high latency</li>
        <li><strong>Stress Testing:</strong> Push systems to their limits</li>
    </ul>

    <blockquote>
        <strong>Best Practice:</strong> Always test your mods on both dedicated servers and listen servers. Network behavior can differ significantly between these modes.
    </blockquote>

    <h2>Debugging Tools and Resources</h2>
    
    <h3>External Tools</h3>
    <ul>
        <li><strong>Visual Studio Debugger:</strong> Step-through debugging for Windows</li>
        <li><strong>GDB:</strong> Command-line debugger for Linux</li>
        <li><strong>Valgrind:</strong> Memory error detection (Linux)</li>
        <li><strong>Application Verifier:</strong> Memory debugging (Windows)</li>
    </ul>

    <h3>Log Analysis</h3>
    <div class="example">
        <pre>// Structured logging for analysis
void G_LogEvent(const char *category, const char *event, const char *details) {
    char timestamp[32];
    Com_sprintf(timestamp, sizeof(timestamp), "%d", level.time);
    
    G_Printf("[%s] %s: %s - %s\n", timestamp, category, event, details);
}</pre>
    </div>

    <h2>Debugging Checklist</h2>
    
    <ul>
        <li>✅ Enable developer mode and relevant debug cvars</li>
        <li>✅ Add comprehensive logging to key functions</li>
        <li>✅ Validate all input parameters</li>
        <li>✅ Check return values from all function calls</li>
        <li>✅ Use assertions for critical assumptions</li>
        <li>✅ Test with bots for consistent reproduction</li>
        <li>✅ Profile performance-critical sections</li>
        <li>✅ Monitor memory usage and leaks</li>
    </ul>

    <blockquote>
        <strong>Remember:</strong> Good debugging practices prevent more bugs than they find. Write defensive code, validate inputs, and document your assumptions.
    </blockquote>
</div> 