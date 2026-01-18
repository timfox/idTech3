/*
===========================================================================
Virtual Filesystem v2 - Mount Table API
===========================================================================
*/

#ifndef __FILES_V2_H__
#define __FILES_V2_H__

#include "q_shared.h"
#include "qcommon.h"

// Forward declarations
typedef struct pack_s pack_t;
typedef struct directory_s directory_t;
typedef struct fileInPack_s fileInPack_t;

// Mount priority levels (higher = searched first)
typedef enum {
	FS_PRIORITY_SYSTEM = 1000,    // System files (highest)
	FS_PRIORITY_MOD = 800,        // User mods
	FS_PRIORITY_GAME = 600,       // Base game
	FS_PRIORITY_CD = 400,         // CD/read-only
	FS_PRIORITY_FALLBACK = 200    // Fallback (lowest)
} fsMountPriority_t;

// Mount type
typedef enum {
	FS_MOUNT_PAK,      // PAK file (.pk3, .orb)
	FS_MOUNT_DIR,      // Directory
	FS_MOUNT_VIRTUAL   // Virtual mount (future: network, etc.)
} fsMountType_t;

// Write policy
typedef enum {
	FS_WRITE_DENY,     // No writes allowed
	FS_WRITE_ALLOW,    // Writes allowed
	FS_WRITE_SANDBOX   // Writes allowed but sandboxed
} fsWritePolicy_t;

// Sandbox rules
typedef struct {
	qboolean allowExecutables;    // Allow .exe, .so, .dll
	qboolean allowConfig;         // Allow .cfg files
	qboolean allowSaves;          // Allow save files
	char allowedPaths[16][MAX_QPATH];  // Whitelist paths
	int numAllowedPaths;
} fsSandboxRules_t;

// Mount entry
typedef struct fsMount_s {
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
} fsMount_t;

// Mount table
typedef struct {
	fsMount_t *mounts;            // Priority-ordered list
	fsMount_t *mountsByPriority[FS_PRIORITY_SYSTEM + 1];  // Quick lookup (first mount at each priority)
	int numMounts;
	
	// Write policy
	fsMount_t *writeMount;        // Default write location
	char writeBasePath[MAX_OSPATH];
	
	// Security
	fsSandboxRules_t globalSandbox;
	qboolean sandboxEnabled;
} fsMountTable_t;

// ============================================================================
// Mount Table Management API
// ============================================================================

// Initialize mount table
void FS_MountTable_Init(void);

// Shutdown mount table
void FS_MountTable_Shutdown(void);

// Create a mount entry
fsMount_t *FS_Mount_Create(const char *mountPoint, fsMountType_t type, 
                           fsMountPriority_t priority);

// Destroy a mount entry
void FS_Mount_Destroy(fsMount_t *mount);

// Add mount to table (inserted by priority)
qboolean FS_Mount_Add(fsMount_t *mount);

// Remove mount from table
qboolean FS_Mount_Remove(const char *mountPoint);

// Find mount by point
fsMount_t *FS_Mount_Find(const char *mountPoint);

// Enable/disable mount
qboolean FS_Mount_SetEnabled(const char *mountPoint, qboolean enabled);

// Set write mount
qboolean FS_Mount_SetWriteMount(const char *mountPoint);

// Get write mount
fsMount_t *FS_Mount_GetWriteMount(void);

// Mount table debugging and monitoring
void FS_MountTable_Dump(void);
void FS_MountTable_Stats(void);

// ============================================================================
// File Search API
// ============================================================================

// Find file in mount table (priority-ordered)
int FS_Mount_FindFile(const char *qpath, fileHandle_t *file, 
                      fsMount_t **outMount, pack_t **outPak, 
                      fileInPack_t **outPakFile);

// Check if file exists in mount table
qboolean FS_Mount_FileExists(const char *qpath);

// ============================================================================
// Write Policy API
// ============================================================================

// Check if write is allowed to path
qboolean FS_WritePolicy_Check(const char *qpath, fsMount_t **outMount);

// Get write mount for path
fsMount_t *FS_WritePolicy_GetMount(const char *qpath);

// Set write base path
qboolean FS_WritePolicy_SetBasePath(const char *path);

// ============================================================================
// Sandboxing API
// ============================================================================

// Check if path matches sandbox rules
qboolean FS_Sandbox_CheckPath(const char *qpath, const fsSandboxRules_t *rules);

// Check if filename is executable
qboolean FS_Sandbox_IsExecutable(const char *filename);

// Apply sandbox rules to file operation
qboolean FS_Sandbox_ValidateOperation(const char *qpath, fsMount_t *mount, 
                                      qboolean isWrite);

// Initialize default sandbox rules
void FS_Sandbox_InitDefaultRules(fsSandboxRules_t *rules);

// Initialize mod-specific sandbox rules
void FS_Sandbox_InitModRules(fsSandboxRules_t *rules);

// Enable/disable global sandboxing
void FS_Sandbox_SetEnabled(qboolean enabled);

// ============================================================================
// Mod Management API
// ============================================================================

// Mount a mod (PAK or directory)
qboolean FS_Mod_Mount(const char *modName, const char *path, 
                      fsMountPriority_t priority);

// Unmount a mod
qboolean FS_Mod_Unmount(const char *modName);

// List mounted mods
int FS_Mod_ListMounted(char *buffer, int bufferSize);

// Check if path points to a PAK file
qboolean FS_IsPakFile(const char *path);

// Get mod info
qboolean FS_Mod_GetInfo(const char *modName, char *path, int pathSize,
                        fsMountPriority_t *priority, qboolean *enabled);

// ============================================================================
// Console Commands
// ============================================================================

// Register console commands
void FS_Mount_RegisterCommands(void);

// ============================================================================
// Migration & Compatibility
// ============================================================================

/*
================
FS_MigrateLegacySearchPaths
================
Convert legacy searchpath_t to mount table
================
*/
void FS_MigrateLegacySearchPaths(void);

// Check if mount table is active
qboolean FS_MountTable_IsActive(void);

// ============================================================================
// File Caching System
// ============================================================================

#define FS_CACHE_HASH_SIZE 1024

// Cache entry structure
typedef struct fsCacheEntry_s {
	char path[MAX_QPATH];            // File path
	char realPath[MAX_QPATH];        // Real filesystem path
	void *cachedData;                // Pointer to cached data or next entry
	size_t dataSize;                 // Size of cached data
	size_t cachedSize;               // Size of cached data (duplicate for compatibility)
	size_t fileSize;                 // Original file size
	uint32_t lastAccess;             // Last access time
	uint32_t lastModified;           // Last modification time
	uint32_t hitCount;               // Number of cache hits
	qboolean isDirectory;            // Is this a directory entry
	struct fsCacheEntry_s *next;     // Next entry in hash chain
} fsCacheEntry_t;

// Cache statistics
typedef struct {
	uint32_t totalEntries;
	uint32_t cachedEntries;
	uint64_t cacheSizeBytes;
	uint64_t maxCacheSizeBytes;
	uint32_t evictions;
	uint32_t hits;
	uint32_t misses;
	float hitRate;
} fsCacheStats_t;

// Cache configuration
typedef struct {
	qboolean enabled;
	size_t maxSizeBytes;
	uint32_t maxEntries;
	float evictionThreshold;
	uint32_t timeToLiveSeconds;
	qboolean compressData;
} fsCacheConfig_t;

// Cache management functions
void FS_Cache_Init(const fsCacheConfig_t *config);
void FS_Cache_Shutdown(void);
qboolean FS_Cache_Lookup(const char *qpath, void **data, size_t *size);
qboolean FS_Cache_Store(const char *qpath, const void *data, size_t size);
void FS_Cache_Remove(const char *qpath);
void FS_Cache_Clear(void);
void FS_Cache_SetSizeLimit(size_t maxBytes);
void FS_Cache_GetStats(fsCacheStats_t *stats);

// ============================================================================
// File Monitoring System (Hot Reload)
// ============================================================================

// Change types
typedef enum {
	FS_CHANGE_ADDED,
	FS_CHANGE_MODIFIED,
	FS_CHANGE_DELETED,
	FS_CHANGE_RENAMED
} fsChangeType_t;

// Change event structure
typedef struct fsChangeEvent_s {
	fsChangeType_t type;
	char oldPath[MAX_QPATH];
	char newPath[MAX_QPATH];
	uint32_t timestamp;
} fsChangeEvent_t;

// Callback function type
typedef void (*fsChangeCallback_t)(const fsChangeEvent_t *event);

// Monitoring functions
void FS_Monitor_Init(void);
void FS_Monitor_Shutdown(void);
qboolean FS_Monitor_AddPath(const char *path);
void FS_Monitor_RemovePath(const char *path);
void FS_Monitor_Update(void);
void FS_Monitor_RegisterCallback(fsChangeCallback_t callback);

#endif // __FILES_V2_H__
