<?php
/**
 * Virtual Filesystem v2.0 - id Tech 3 Engine Documentation  
 */
$title = 'Virtual Filesystem v2.0 - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/filesystem-v2' => 'Virtual Filesystem v2.0'
];
?>

<h1>Virtual Filesystem v2.0</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Virtual Filesystem v2.0 (VFS v2) is a complete redesign of the id Tech 3 filesystem architecture, introducing modern mount management, priority-based file search, enhanced security through sandboxing, and explicit mod management APIs. VFS v2 maintains full backward compatibility with the legacy filesystem while providing a more maintainable and feature-rich foundation for future development.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Mount Table System:</strong> Centralized mount management with priority ordering</li>
            <li><strong>Priority-Based Search:</strong> Efficient file resolution using priority levels</li>
            <li><strong>Write Policy Management:</strong> Centralized control over write operations</li>
            <li><strong>Enhanced Sandboxing:</strong> Per-mount and global security rules</li>
            <li><strong>Mod Management APIs:</strong> Explicit mount/unmount operations for mods</li>
            <li><strong>Backward Compatibility:</strong> Seamless migration from legacy searchpaths</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Architecture</h2>
    
    <h3>Mount Priority Levels</h3>
    <p>VFS v2 uses a priority-based system where higher priority mounts are searched first:</p>
    <div class="code-block">
        <pre><code>typedef enum {
    FS_PRIORITY_SYSTEM = 1000,    // System files (highest priority)
    FS_PRIORITY_MOD = 800,        // User mods
    FS_PRIORITY_GAME = 600,       // Base game
    FS_PRIORITY_CD = 400,         // CD/read-only
    FS_PRIORITY_FALLBACK = 200    // Fallback (lowest priority)
} fsMountPriority_t;</code></pre>
    </div>
    
    <h3>Mount Types</h3>
    <p>VFS v2 supports multiple mount types:</p>
    <div class="code-block">
        <pre><code>typedef enum {
    FS_MOUNT_PAK,      // PAK file (.pk3, .orb)
    FS_MOUNT_DIR,      // Directory
    FS_MOUNT_VIRTUAL   // Virtual mount (future: network, etc.)
} fsMountType_t;</code></pre>
    </div>
    
    <h3>Write Policies</h3>
    <p>Each mount can have different write policies:</p>
    <div class="code-block">
        <pre><code>typedef enum {
    FS_WRITE_DENY,     // No writes allowed (read-only)
    FS_WRITE_ALLOW,    // Writes allowed
    FS_WRITE_SANDBOX   // Writes allowed but sandboxed
} fsWritePolicy_t;</code></pre>
    </div>
    
    <h3>Mount Structure</h3>
    <div class="code-block">
        <pre><code>typedef struct fsMount_s {
    struct fsMount_s *next;
    struct fsMount_s *prev;       // For priority-ordered list
    
    // Identity
    char mountPoint[MAX_QPATH];   // Virtual mount point (e.g., "mods/mymod")
    fsMountType_t type;
    fsMountPriority_t priority;
    
    // Backend
    union {
        pack_t *pak;
        directory_t *dir;
        void *virtual;  // Future: network mount, etc.
    } backend;
    
    // Policies
    fsWritePolicy_t writePolicy;
    fsSandboxRules_t sandbox;
    
    // Metadata
    char displayName[MAX_QPATH];
    qboolean enabled;
    uint32_t checksum;  // For PAK files
    
    // Statistics
    uint32_t accessCount;
    uint32_t hitCount;
} fsMount_t;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Mount Table Management</h2>
    
    <h3>Initialization</h3>
    <p>The mount table is automatically initialized during filesystem startup:</p>
    <div class="code-block">
        <pre><code>// Initialize mount table
FS_MountTable_Init();

// Migrate legacy searchpaths to mount table
if (fs_searchpaths) {
    FS_MigrateLegacySearchPaths();
}

// Register console commands
FS_Mount_RegisterCommands();</code></pre>
    </div>
    
    <h3>Creating Mounts</h3>
    <p>Mounts are created with a mount point, type, and priority:</p>
    <div class="code-block">
        <pre><code>// Create a directory mount
fsMount_t *mount = FS_Mount_Create("mods/mymod", FS_MOUNT_DIR, FS_PRIORITY_MOD);
if (mount) {
    mount->backend.dir = myDirectory;
    mount->writePolicy = FS_WRITE_SANDBOX;
    FS_Mount_Add(mount);
}</code></pre>
    </div>
    
    <h3>Finding Mounts</h3>
    <div class="code-block">
        <pre><code>// Find mount by mount point
fsMount_t *mount = FS_Mount_Find("mods/mymod");

// Enable/disable mount
FS_Mount_SetEnabled("mods/mymod", qfalse);

// Remove mount
FS_Mount_Remove("mods/mymod");
FS_Mount_Destroy(mount);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Priority-Based File Search</h2>
    
    <p>VFS v2 searches mounts in priority order, starting with the highest priority mount. This ensures that system files are found before mod files, and mod files before base game files.</p>
    
    <h3>File Search Process</h3>
    <ol>
        <li>Start at highest priority level (FS_PRIORITY_SYSTEM)</li>
        <li>Search all enabled mounts at current priority</li>
        <li>If not found, advance to next lower priority</li>
        <li>Continue until file is found or all priorities exhausted</li>
    </ol>
    
    <h3>Implementation</h3>
    <div class="code-block">
        <pre><code>int FS_Mount_FindFile(const char *qpath, fileHandle_t *file, 
                      fsMount_t **outMount, pack_t **outPak, 
                      fileInPack_t **outPakFile) {
    fsMountPriority_t currentPriority = FS_PRIORITY_SYSTEM;
    fsMount_t *mount = fs_mountTable.mountsByPriority[currentPriority];
    
    // Search through mounts by priority
    while (mount) {
        if (!mount->enabled) {
            mount = FS_Mount_GetNextByPriority(&currentPriority, mount);
            continue;
        }
        
        // Check PAK file mount
        if (mount->type == FS_MOUNT_PAK && mount->backend.pak) {
            // Search PAK hash table...
        }
        // Check directory mount
        else if (mount->type == FS_MOUNT_DIR && mount->backend.dir) {
            // Try to open file from directory...
        }
        
        mount = FS_Mount_GetNextByPriority(&currentPriority, mount);
    }
    
    return -1; // Not found
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Write Policy Management</h2>
    
    <p>VFS v2 provides centralized write policy management to control where files can be written.</p>
    
    <h3>Write Policy Checking</h3>
    <div class="code-block">
        <pre><code>// Check if write is allowed
fsMount_t *writeMount = NULL;
if (FS_WritePolicy_Check("saves/game.save", &writeMount)) {
    // Write allowed, use writeMount
    // ...
} else {
    // Write denied
    return FS_INVALID_HANDLE;
}</code></pre>
    </div>
    
    <h3>Setting Write Mount</h3>
    <div class="code-block">
        <pre><code>// Set default write mount
FS_Mount_SetWriteMount("mods/mymod");

// Get current write mount
fsMount_t *writeMount = FS_Mount_GetWriteMount();</code></pre>
    </div>
    
    <h3>Write Policy Integration</h3>
    <p>Write policy is automatically checked in <code>FS_FOpenFileWrite</code>:</p>
    <div class="code-block">
        <pre><code>// VFS v2: Check write policy if mount table is active
if (FS_MountTable_IsActive()) {
    fsMount_t *writeMount = FS_WritePolicy_GetMount(filename);
    if (!writeMount) {
        return FS_INVALID_HANDLE; // Write denied
    }
    
    // Apply sandboxing
    if (!FS_Sandbox_ValidateOperation(filename, writeMount, qtrue)) {
        return FS_INVALID_HANDLE; // Sandbox violation
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Sandboxing</h2>
    
    <p>VFS v2 includes enhanced sandboxing capabilities to prevent unauthorized file operations.</p>
    
    <h3>Sandbox Rules</h3>
    <div class="code-block">
        <pre><code>typedef struct {
    qboolean allowExecutables;    // Allow .exe, .so, .dll
    qboolean allowConfig;         // Allow .cfg files
    qboolean allowSaves;          // Allow save files
    char allowedPaths[16][MAX_QPATH];  // Whitelist paths
    int numAllowedPaths;
} fsSandboxRules_t;</code></pre>
    </div>
    
    <h3>Default Sandbox Rules</h3>
    <p>Default rules allow config and saves, but deny executables:</p>
    <div class="code-block">
        <pre><code>void FS_Sandbox_InitDefaultRules(fsSandboxRules_t *rules) {
    rules->allowConfig = qtrue;
    rules->allowSaves = qtrue;
    rules->allowExecutables = qfalse;
    rules->numAllowedPaths = 0;
}</code></pre>
    </div>
    
    <h3>Mod Sandbox Rules</h3>
    <p>Mods use more restrictive rules:</p>
    <div class="code-block">
        <pre><code>void FS_Sandbox_InitModRules(fsSandboxRules_t *rules) {
    rules->allowConfig = qfalse;
    rules->allowSaves = qfalse;
    rules->allowExecutables = qfalse;
    
    // Only allow specific paths
    rules->numAllowedPaths = 3;
    Q_strncpyz(rules->allowedPaths[0], "maps/", ...);
    Q_strncpyz(rules->allowedPaths[1], "textures/", ...);
    Q_strncpyz(rules->allowedPaths[2], "scripts/", ...);
}</code></pre>
    </div>
    
    <h3>Sandbox Validation</h3>
    <div class="code-block">
        <pre><code>// Validate file operation against sandbox rules
qboolean FS_Sandbox_ValidateOperation(const char *qpath, fsMount_t *mount, 
                                      qboolean isWrite) {
    // Use mount-specific rules if available, otherwise global rules
    fsSandboxRules_t *rules = mount ? &mount->sandbox : &fs_mountTable.globalSandbox;
    
    // Check path against rules
    if (!FS_Sandbox_CheckPath(qpath, rules)) {
        return qfalse;
    }
    
    // Additional write-specific checks
    if (isWrite && mount && mount->writePolicy == FS_WRITE_DENY) {
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Mod Management</h2>
    
    <p>VFS v2 provides explicit APIs for mounting and unmounting mods.</p>
    
    <h3>Mounting a Mod</h3>
    <div class="code-block">
        <pre><code>// Mount a mod (PAK or directory)
qboolean FS_Mod_Mount(const char *modName, const char *path, 
                      fsMountPriority_t priority);

// Example: Mount a directory mod
if (FS_Mod_Mount("mymod", "/path/to/mymod", FS_PRIORITY_MOD)) {
    Com_Printf("Mod 'mymod' mounted successfully\n");
}

// Example: Mount a PAK mod
if (FS_Mod_Mount("mypakmod", "/path/to/mymod.pk3", FS_PRIORITY_MOD)) {
    Com_Printf("PAK mod 'mypakmod' mounted successfully\n");
}</code></pre>
    </div>
    
    <h3>Unmounting a Mod</h3>
    <div class="code-block">
        <pre><code>// Unmount a mod
qboolean FS_Mod_Unmount(const char *modName);

if (FS_Mod_Unmount("mymod")) {
    Com_Printf("Mod 'mymod' unmounted successfully\n");
}</code></pre>
    </div>
    
    <h3>Listing Mounted Mods</h3>
    <div class="code-block">
        <pre><code>// List all mounted mods
char buffer[1024];
int len = FS_Mod_ListMounted(buffer, sizeof(buffer));
Com_Printf("Mounted mods: %s\n", buffer);</code></pre>
    </div>
    
    <h3>Getting Mod Info</h3>
    <div class="code-block">
        <pre><code>// Get mod information
char path[MAX_OSPATH];
fsMountPriority_t priority;
qboolean enabled;

if (FS_Mod_GetInfo("mymod", path, sizeof(path), &priority, &enabled)) {
    Com_Printf("Mod path: %s\n", path);
    Com_Printf("Priority: %d\n", priority);
    Com_Printf("Enabled: %s\n", enabled ? "yes" : "no");
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Console Commands</h2>
    
    <p>VFS v2 provides several console commands for managing mounts:</p>
    
    <h3>fs_mount_list</h3>
    <p>Lists all mounted filesystems with their properties:</p>
    <div class="code-block">
        <pre><code>fs_mount_list

// Output:
// Mounted filesystems:
// ===================
//   [0] mods/mymod (DIR) priority=800 enabled read-write
//       Display: mymod
//       Stats: 42 accesses, 38 hits
//   [1] legacy/pak0 (PAK) priority=600 enabled read-only
//       Display: pak0
//       Stats: 1024 accesses, 1024 hits
// 
// Total mounts: 2
// Write mount: mods/mymod</code></pre>
    </div>
    
    <h3>fs_mod_mount</h3>
    <p>Mounts a mod:</p>
    <div class="code-block">
        <pre><code>fs_mod_mount mymod /path/to/mymod [priority]

// Examples:
fs_mod_mount mymod /home/user/mymod
fs_mod_mount mypakmod /home/user/mymod.pk3 800</code></pre>
    </div>
    
    <h3>fs_mod_unmount</h3>
    <p>Unmounts a mod:</p>
    <div class="code-block">
        <pre><code>fs_mod_unmount mymod</code></pre>
    </div>
    
    <h3>fs_mount_info</h3>
    <p>Shows detailed information about a mount:</p>
    <div class="code-block">
        <pre><code>fs_mount_info mods/mymod

// Output:
// Mount Info: mods/mymod
// ===================
// Type: DIR
// Priority: 800
// Enabled: yes
// Write Policy: sandbox
// Display Name: mymod
// Path: /home/user/mymod
// Game Dir: mymod
// Access Count: 42
// Hit Count: 38
// Sandbox Rules:
//   Allow Executables: no
//   Allow Config: no
//   Allow Saves: no
//   Allowed Paths: 3</code></pre>
    </div>
</div>

<div class="section">
    <h2>Backward Compatibility</h2>
    
    <p>VFS v2 maintains full backward compatibility with the legacy filesystem. During initialization, all existing searchpaths are automatically migrated to the mount table.</p>
    
    <h3>Migration Process</h3>
    <div class="code-block">
        <pre><code>void FS_MigrateLegacySearchPaths(void) {
    searchpath_t *sp;
    fsMount_t *mount;
    fsMountPriority_t priority = FS_PRIORITY_GAME;
    
    for (sp = fs_searchpaths; sp; sp = sp->next) {
        fsMountType_t type;
        char mountPoint[MAX_QPATH];
        
        // Determine mount type
        if (sp->pack) {
            type = FS_MOUNT_PAK;
            Com_sprintf(mountPoint, sizeof(mountPoint), "legacy/pak%d", count--);
        } else if (sp->dir) {
            type = FS_MOUNT_DIR;
            Com_sprintf(mountPoint, sizeof(mountPoint), "legacy/dir%d", count--);
        } else {
            continue;
        }
        
        // Create mount and copy backend data
        mount = FS_Mount_Create(mountPoint, type, priority);
        // ... copy data and add to mount table
    }
}</code></pre>
    </div>
    
    <h3>Dual-Mode Operation</h3>
    <p>VFS v2 operates in dual-mode:</p>
    <ul>
        <li>If mount table is active, file operations use VFS v2</li>
        <li>If mount table is not active or file not found, falls back to legacy searchpaths</li>
        <li>This ensures seamless operation during transition</li>
    </ul>
    
    <h3>Integration Points</h3>
    <p>VFS v2 is integrated into key filesystem functions:</p>
    <div class="code-block">
        <pre><code>// FS_FOpenFileRead integration
int FS_FOpenFileRead(const char *filename, fileHandle_t *file, qboolean uniqueFILE) {
    // VFS v2: Try mount table first if active
    if (FS_MountTable_IsActive()) {
        fsMount_t *mount = NULL;
        pack_t *foundPak = NULL;
        fileInPack_t *foundPakFile = NULL;
        int result = FS_Mount_FindFile(filename, file, &mount, &foundPak, &foundPakFile);
        if (result >= 0) {
            return result; // File found in mount table
        }
        // Not found in mount table, fall through to legacy search
    }
    
    // Legacy search path...
}

// FS_FOpenFileWrite integration
fileHandle_t FS_FOpenFileWrite(const char *qpath) {
    // VFS v2: Check write policy if mount table is active
    if (!fs_startupInProgress && FS_MountTable_IsActive()) {
        fsMount_t *writeMount = FS_WritePolicy_GetMount(qpath);
        if (!writeMount) {
            return FS_INVALID_HANDLE; // Write denied
        }
        
        // Apply sandboxing
        if (!FS_Sandbox_ValidateOperation(qpath, writeMount, qtrue)) {
            return FS_INVALID_HANDLE; // Sandbox violation
        }
    }
    
    // Continue with write operation...
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>File Structure</h2>
    
    <p>VFS v2 is implemented across several files:</p>
    
    <h3>Header Files</h3>
    <ul>
        <li><code>src/common/files_v2.h</code> - Public API and data structures</li>
        <li><code>src/common/files_internal.h</code> - Internal type definitions shared between files.c and files_v2.c</li>
    </ul>
    
    <h3>Implementation Files</h3>
    <ul>
        <li><code>src/common/files_v2.c</code> - Core mount table management and file search</li>
        <li><code>src/common/files_v2_impl.c</code> - Write policy, sandboxing, mod management, console commands, and migration</li>
        <li><code>src/common/files.c</code> - Integration points with legacy filesystem</li>
    </ul>
    
    <h3>Test Files</h3>
    <ul>
        <li><code>tests/test_filesystem_v2.c</code> - Unit tests for mount operations, priority ordering, write policy, and sandbox validation</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    
    <h3>Priority-Based Search</h3>
    <p>Priority-based search provides several performance benefits:</p>
    <ul>
        <li><strong>Early Termination:</strong> File found at high priority stops search</li>
        <li><strong>Efficient Lookup:</strong> Priority lookup table provides O(1) access to first mount at each priority</li>
        <li><strong>Reduced Overhead:</strong> Disabled mounts are skipped automatically</li>
    </ul>
    
    <h3>Statistics Tracking</h3>
    <p>Each mount tracks access and hit statistics for performance analysis:</p>
    <div class="code-block">
        <pre><code>// Statistics are automatically updated during file search
mount->accessCount++;  // Incremented when mount is checked
mount->hitCount++;     // Incremented when file is found in mount</code></pre>
    </div>
</div>

<div class="section">
    <h2>Security Features</h2>
    
    <h3>Sandboxing</h3>
    <ul>
        <li>Prevents execution of unauthorized executables</li>
        <li>Restricts file writes to allowed paths</li>
        <li>Per-mount and global sandbox rules</li>
        <li>Mod-specific restrictive rules</li>
    </ul>
    
    <h3>Write Policy</h3>
    <ul>
        <li>Centralized write policy management</li>
        <li>Per-mount write policies (DENY, ALLOW, SANDBOX)</li>
        <li>Default write mount configuration</li>
        <li>Automatic write policy checking</li>
    </ul>
    
    <h3>Path Validation</h3>
    <ul>
        <li>Directory traversal protection</li>
        <li>Path whitelisting for mods</li>
        <li>Executable file detection</li>
    </ul>
</div>

<div class="section">
    <h2>Future Enhancements</h2>
    
    <p>VFS v2 is designed with extensibility in mind:</p>
    
    <ul>
        <li><strong>Virtual Mounts:</strong> Support for network mounts, cloud storage, etc.</li>
        <li><strong>Mount Plugins:</strong> Extensible mount type system</li>
        <li><strong>Advanced Caching:</strong> File existence and path resolution caching</li>
        <li><strong>Mount Profiles:</strong> Save/load mount configurations</li>
        <li><strong>Hot Reloading:</strong> Reload mounts without restart</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/filesystem">Filesystem Documentation</a> - Legacy filesystem reference</li>
        <li><a href="core/filesystem-improvements">Filesystem Improvements</a> - Other filesystem enhancements</li>
        <li><a href="development/modding">Modding Guide</a> - Creating mods with VFS v2</li>
        <li><a href="core/memory-management">Memory Management</a> - Memory allocation for mounts</li>
    </ul>
</div>
