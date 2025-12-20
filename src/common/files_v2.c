/*
===========================================================================
Virtual Filesystem v2 - Mount Table Implementation
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "files_internal.h"  // Internal type definitions
#include "files_v2.h"

// Forward declarations
struct searchpath_s;
typedef struct searchpath_s searchpath_t;

extern searchpath_t *fs_searchpaths;
extern int fs_checksumFeed;

// Mount table instance (shared with files_v2_impl.c)
fsMountTable_t fs_mountTable;
qboolean fs_mountTableInitialized = qfalse;

// Forward declarations
static void FS_Mount_InsertByPriority(fsMount_t *mount);
static void FS_Mount_RemoveFromPriority(fsMount_t *mount);

// ============================================================================
// Mount Table Management
// ============================================================================

/*
================
FS_MountTable_Init
================
*/
void FS_MountTable_Init(void) {
	if (fs_mountTableInitialized) {
		return;
	}

	Com_Memset(&fs_mountTable, 0, sizeof(fs_mountTable));
	fs_mountTable.sandboxEnabled = qtrue;
	
	// Initialize default global sandbox rules
	FS_Sandbox_InitDefaultRules(&fs_mountTable.globalSandbox);
	
	fs_mountTableInitialized = qtrue;
}

/*
================
FS_MountTable_Shutdown
================
*/
void FS_MountTable_Shutdown(void) {
	fsMount_t *mount, *next;
	
	if (!fs_mountTableInitialized) {
		return;
	}
	
	// Destroy all mounts
	for (mount = fs_mountTable.mounts; mount; mount = next) {
		next = mount->next;
		FS_Mount_Destroy(mount);
	}
	
	Com_Memset(&fs_mountTable, 0, sizeof(fs_mountTable));
	fs_mountTableInitialized = qfalse;
}

/*
================
FS_MountTable_IsActive
================
*/
qboolean FS_MountTable_IsActive(void) {
	return fs_mountTableInitialized && fs_mountTable.mounts != NULL;
}

/*
================
FS_Mount_Create
================
*/
fsMount_t *FS_Mount_Create(const char *mountPoint, fsMountType_t type, 
                           fsMountPriority_t priority) {
	fsMount_t *mount;
	
	if (!mountPoint || !*mountPoint) {
		return NULL;
	}
	
	if (priority < FS_PRIORITY_FALLBACK || priority > FS_PRIORITY_SYSTEM) {
		return NULL;
	}
	
	mount = Z_TagMalloc(sizeof(*mount), TAG_SEARCH_PATH);
	if (!mount) {
		return NULL;
	}
	
	Com_Memset(mount, 0, sizeof(*mount));
	
	Q_strncpyz(mount->mountPoint, mountPoint, sizeof(mount->mountPoint));
	mount->type = type;
	mount->priority = priority;
	mount->enabled = qtrue;
	mount->writePolicy = FS_WRITE_DENY;  // Default: no writes
	
	// Initialize sandbox rules
	FS_Sandbox_InitDefaultRules(&mount->sandbox);
	
	return mount;
}

/*
================
FS_Mount_Destroy
================
*/
void FS_Mount_Destroy(fsMount_t *mount) {
	if (!mount) {
		return;
	}
	
	// Remove from mount table if still linked
	if (mount->next || mount->prev || fs_mountTable.mounts == mount) {
		FS_Mount_Remove(mount->mountPoint);
	}
	
	// Note: We don't free backend.pak or backend.dir here
	// as they may be shared with legacy searchpath_t system
	// The caller should handle cleanup if needed
	
	Z_Free(mount);
}

/*
================
FS_Mount_InsertByPriority
================
Insert mount into priority-ordered list
================
*/
static void FS_Mount_InsertByPriority(fsMount_t *mount) {
	fsMount_t *current, *prev;
	
	if (!mount) {
		return;
	}
	
	// Find insertion point (maintain priority order, highest first)
	prev = NULL;
	for (current = fs_mountTable.mounts; current; current = current->next) {
		if (current->priority < mount->priority) {
			// Insert before current
			break;
		}
		prev = current;
	}
	
	// Insert mount
	mount->next = current;
	mount->prev = prev;
	
	if (prev) {
		prev->next = mount;
	} else {
		fs_mountTable.mounts = mount;
	}
	
	if (current) {
		current->prev = mount;
	}
	
	// Update priority lookup (first mount at this priority)
	if (!fs_mountTable.mountsByPriority[mount->priority] ||
	    fs_mountTable.mountsByPriority[mount->priority]->priority != mount->priority) {
		fs_mountTable.mountsByPriority[mount->priority] = mount;
	}
	
	fs_mountTable.numMounts++;
}

/*
================
FS_Mount_RemoveFromPriority
================
Remove mount from priority-ordered list
================
*/
static void FS_Mount_RemoveFromPriority(fsMount_t *mount) {
	if (!mount) {
		return;
	}
	
	// Update links
	if (mount->prev) {
		mount->prev->next = mount->next;
	} else {
		fs_mountTable.mounts = mount->next;
	}
	
	if (mount->next) {
		mount->next->prev = mount->prev;
	}
	
	// Update priority lookup if needed
	if (fs_mountTable.mountsByPriority[mount->priority] == mount) {
		// Find next mount at same priority
		fsMount_t *next = mount->next;
		while (next && next->priority != mount->priority) {
			next = next->next;
		}
		fs_mountTable.mountsByPriority[mount->priority] = next;
	}
	
	mount->next = NULL;
	mount->prev = NULL;
	
	fs_mountTable.numMounts--;
}

/*
================
FS_Mount_Add
================
*/
qboolean FS_Mount_Add(fsMount_t *mount) {
	if (!mount) {
		return qfalse;
	}
	
	if (!fs_mountTableInitialized) {
		FS_MountTable_Init();
	}
	
	// Check if mount point already exists
	if (FS_Mount_Find(mount->mountPoint)) {
		return qfalse;
	}
	
	FS_Mount_InsertByPriority(mount);
	
	return qtrue;
}

/*
================
FS_Mount_Remove
================
*/
qboolean FS_Mount_Remove(const char *mountPoint) {
	fsMount_t *mount;
	
	if (!mountPoint || !*mountPoint) {
		return qfalse;
	}
	
	mount = FS_Mount_Find(mountPoint);
	if (!mount) {
		return qfalse;
	}
	
	// If this is the write mount, clear it
	if (fs_mountTable.writeMount == mount) {
		fs_mountTable.writeMount = NULL;
	}
	
	FS_Mount_RemoveFromPriority(mount);
	
	// Don't destroy here - caller should handle cleanup
	// FS_Mount_Destroy(mount);
	
	return qtrue;
}

/*
================
FS_Mount_Find
================
*/
fsMount_t *FS_Mount_Find(const char *mountPoint) {
	fsMount_t *mount;
	
	if (!mountPoint || !*mountPoint) {
		return NULL;
	}
	
	for (mount = fs_mountTable.mounts; mount; mount = mount->next) {
		if (!Q_stricmp(mount->mountPoint, mountPoint)) {
			return mount;
		}
	}
	
	return NULL;
}

/*
================
FS_Mount_SetEnabled
================
*/
qboolean FS_Mount_SetEnabled(const char *mountPoint, qboolean enabled) {
	fsMount_t *mount;
	
	mount = FS_Mount_Find(mountPoint);
	if (!mount) {
		return qfalse;
	}
	
	mount->enabled = enabled;
	return qtrue;
}

/*
================
FS_Mount_SetWriteMount
================
*/
qboolean FS_Mount_SetWriteMount(const char *mountPoint) {
	fsMount_t *mount;
	
	if (!mountPoint || !*mountPoint) {
		fs_mountTable.writeMount = NULL;
		return qtrue;
	}
	
	mount = FS_Mount_Find(mountPoint);
	if (!mount) {
		return qfalse;
	}
	
	if (mount->writePolicy == FS_WRITE_DENY) {
		return qfalse;
	}
	
	fs_mountTable.writeMount = mount;
	return qtrue;
}

/*
================
FS_Mount_GetWriteMount
================
*/
fsMount_t *FS_Mount_GetWriteMount(void) {
	return fs_mountTable.writeMount;
}

// ============================================================================
// Helper Functions
// ============================================================================

/*
================
FS_IsPakFile
================
Check if path points to a PAK file
================
*/
qboolean FS_IsPakFile(const char *path) {
	const char *ext;
	int len;
	
	if (!path) {
		return qfalse;
	}
	
	len = strlen(path);
	if (len < 4) {
		return qfalse;
	}
	
	ext = path + len - 4;
	if (!Q_stricmp(ext, ".pk3") || !Q_stricmp(ext, ".orb")) {
		return qtrue;
	}
	
	return qfalse;
}

// ============================================================================
// Priority-Based File Search
// ============================================================================

/*
================
FS_Mount_GetNextByPriority
================
Get next mount at current priority, then advance to next priority level
================
*/
static fsMount_t *FS_Mount_GetNextByPriority(fsMountPriority_t *currentPriority, fsMount_t *currentMount) {
	fsMount_t *mount;
	
	if (!currentPriority) {
		return NULL;
	}
	
	// If we have a current mount, try next mount at same priority
	if (currentMount && currentMount->next && currentMount->next->priority == *currentPriority) {
		return currentMount->next;
	}
	
	// Advance to next priority level
	*currentPriority = *currentPriority - 1;
	
	// Find first mount at new priority level
	while (*currentPriority >= FS_PRIORITY_FALLBACK) {
		mount = fs_mountTable.mountsByPriority[*currentPriority];
		if (mount && mount->enabled) {
			return mount;
		}
		*currentPriority = *currentPriority - 1;
	}
	
	return NULL;
}

/*
================
FS_Mount_FindFile
================
Find file in mount table using priority-ordered search
================
*/
int FS_Mount_FindFile(const char *qpath, fileHandle_t *file, 
                      fsMount_t **outMount, pack_t **outPak, 
                      fileInPack_t **outPakFile) {
	fsMount_t *mount;
	fsMountPriority_t currentPriority;
	pack_t *pak;
	fileInPack_t *pakFile;
	directory_t *dir;
	char *netpath;
	FILE *temp;
	int length;
	fileHandleData_t *f;
	uint32_t hash;
	uint32_t fullHash;
	
	if (!qpath || !*qpath) {
		if (file) {
			*file = FS_INVALID_HANDLE;
		}
		return -1;
	}
	
	if (!fs_mountTableInitialized || !fs_mountTable.mounts) {
		if (file) {
			*file = FS_INVALID_HANDLE;
		}
		return -1;
	}
	
	// Generate hash for filename
	fullHash = Com_GenerateHashValue(qpath, 0U);
	hash = fullHash;
	
	// Start at highest priority
	currentPriority = FS_PRIORITY_SYSTEM;
	mount = fs_mountTable.mountsByPriority[currentPriority];
	if (!mount) {
		// Find first available priority
		for (currentPriority = FS_PRIORITY_SYSTEM; currentPriority >= FS_PRIORITY_FALLBACK; currentPriority--) {
			mount = fs_mountTable.mountsByPriority[currentPriority];
			if (mount) {
				break;
			}
		}
		if (!mount) {
			if (file) {
				*file = FS_INVALID_HANDLE;
			}
			return -1;
		}
	}
	
	// Search through mounts by priority
	while (mount) {
		if (!mount->enabled) {
			mount = FS_Mount_GetNextByPriority(&currentPriority, mount);
			continue;
		}
		
		// Update statistics
		mount->accessCount++;
		
		// Check PAK file mount
		if (mount->type == FS_MOUNT_PAK && mount->backend.pak) {
			pak = mount->backend.pak;
			
			// Check pure server pak list
			if (!FS_PakIsPure(pak)) {
				mount = FS_Mount_GetNextByPriority(&currentPriority, mount);
				continue;
			}
			
			if (pak->hashTable && pak->hashTable[(hash & (pak->hashSize - 1))]) {
				pakFile = pak->hashTable[hash & (pak->hashSize - 1)];
				do {
					// Case and separator insensitive comparison
					if (!FS_FilenameCompare(pakFile->name, qpath)) {
						// Found it!
						mount->hitCount++;
						
						if (outMount) {
							*outMount = mount;
						}
						if (outPak) {
							*outPak = pak;
						}
						if (outPakFile) {
							*outPakFile = pakFile;
						}
						
						if (file) {
							// Open file in PAK
							return FS_OpenFileInPak(file, pak, pakFile, qtrue);
						} else {
							// Just checking existence
							return pakFile->size;
						}
					}
					pakFile = pakFile->next;
				} while (pakFile != NULL);
			}
		}
		// Check directory mount
		else if (mount->type == FS_MOUNT_DIR && mount->backend.dir) {
			dir = mount->backend.dir;
			
			// Build OS path
			netpath = FS_BuildOSPath(dir->path, dir->gamedir, qpath);
			
			// Try to open file
			temp = Sys_FOpen(netpath, "rb");
			
			// TODO: Case-insensitive file lookup
			// if (temp == NULL && fs_caseInsensitive && fs_caseInsensitive->integer) {
			//     if (FS_FindFileCaseInsensitive(netpath, qpath)) {
			//         temp = Sys_FOpen(netpath, "rb");
			//     }
			// }
			
			if (temp != NULL) {
				// Found it!
				mount->hitCount++;
				
				if (outMount) {
					*outMount = mount;
				}
				if (outPak) {
					*outPak = NULL;
				}
				if (outPakFile) {
					*outPakFile = NULL;
				}
				
				if (file == NULL) {
					// Just checking existence
					length = FS_FileLength(temp);
					fclose(temp);
					return length;
				}
				
				// Open file handle
				*file = FS_HandleForFile();
				f = &fsh[*file];
				FS_InitHandle(f);
				
				f->handleFiles.file.o = temp;
				Q_strncpyz(f->name, qpath, sizeof(f->name));
				f->zipFile = qfalse;
				
				return FS_FileLength(f->handleFiles.file.o);
			}
		}
		
		// Move to next mount
		mount = FS_Mount_GetNextByPriority(&currentPriority, mount);
	}
	
	// File not found
	if (file) {
		*file = FS_INVALID_HANDLE;
	}
	return -1;
}

/*
================
FS_Mount_FileExists
================
Check if file exists in mount table
================
*/
qboolean FS_Mount_FileExists(const char *qpath) {
	return FS_Mount_FindFile(qpath, NULL, NULL, NULL, NULL) >= 0;
}
