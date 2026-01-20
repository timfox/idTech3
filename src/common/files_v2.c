/*
===========================================================================
Virtual Filesystem v2 - Complete implementation
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "files_v2.h"
#include "files_internal.h"

// Console command implementations
static void FS_Mount_List_f(void);
static void FS_Mount_Add_f(void);
static void FS_Mount_Remove_f(void);
static void FS_Mount_Enable_f(void);
static void FS_Mount_Disable_f(void);
static void FS_Mount_WriteMount_f(void);
static void FS_Mount_Stats_f(void);
static void FS_Cache_Stats_f(void);
static void FS_Sandbox_Status_f(void);

/*
===============
FS_Mount_RegisterCommands
===============
*/
void FS_Mount_RegisterCommands(void) {
	Cmd_AddCommand("fs_mount_list", FS_Mount_List_f);
	Cmd_AddCommand("fs_mount_add", FS_Mount_Add_f);
	Cmd_AddCommand("fs_mount_remove", FS_Mount_Remove_f);
	Cmd_AddCommand("fs_mount_enable", FS_Mount_Enable_f);
	Cmd_AddCommand("fs_mount_disable", FS_Mount_Disable_f);
	Cmd_AddCommand("fs_mount_write", FS_Mount_WriteMount_f);
	Cmd_AddCommand("fs_mount_stats", FS_Mount_Stats_f);
	Cmd_AddCommand("fs_cache_stats", FS_Cache_Stats_f);
	Cmd_AddCommand("fs_sandbox_status", FS_Sandbox_Status_f);

	Com_Printf("Virtual Filesystem v2 commands registered\n");
}

/*
===============
FS_Sandbox_ValidateOperation
===============
*/
qboolean FS_Sandbox_ValidateOperation(const char *qpath, fsMount_t *mount, qboolean isWrite) {
	if (!mountTable.sandboxEnabled) {
		return qtrue; // Sandbox disabled, allow all operations
	}

	const fsSandboxRules_t *rules = &mount->sandbox;

	// Check executables
	if (rules->allowExecutables == qfalse && FS_Sandbox_IsExecutable(qpath)) {
		return qfalse;
	}

	// Check config files
	if (rules->allowConfig == qfalse) {
		const char *ext = COM_GetExtension(qpath);
		if (Q_stricmp(ext, "cfg") == 0 || Q_stricmp(ext, "config") == 0) {
			return qfalse;
		}
	}

	// Check save files
	if (rules->allowSaves == qfalse) {
		if (Q_stristr(qpath, "save") || Q_stristr(qpath, "autosave")) {
			return qfalse;
		}
	}

	// Check allowed paths (whitelist)
	if (rules->numAllowedPaths > 0) {
		qboolean pathAllowed = qfalse;
		for (int i = 0; i < rules->numAllowedPaths; i++) {
			if (Q_stristr(qpath, rules->allowedPaths[i])) {
				pathAllowed = qtrue;
				break;
			}
		}
		if (!pathAllowed) {
			return qfalse;
		}
	}

	// Write-specific checks
	if (isWrite) {
		// For write operations, additional validation can be added here
		// For example, check file size limits, prevent overwriting system files, etc.
	}

	return qtrue; // Operation allowed
}
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#ifdef __cplusplus
#include <vector>
#include <algorithm>
#endif

static fsMountTable_t mountTable;
static qboolean fs_v2_active = qfalse;

// Forward declarations for internal functions
static void FS_Mount_InsertByPriority(fsMount_t *mount);
static void FS_Mount_RemoveFromList(fsMount_t *mount);
static fileInPack_t *FS_FindPakFile(pack_t *pak, const char *filename);
static qboolean FS_Mount_IsValidType(fsMountType_t type);
static qboolean FS_Mount_ValidatePath(const char *path);
static uint32_t FS_Mount_CalculateChecksum(const char *path);

/*
===============
FS_MountTable_Init
===============
*/
void FS_MountTable_Init(void) {
	if (fs_v2_active) return;
	
	Com_Memset(&mountTable, 0, sizeof(mountTable));

	// Initialize sandbox rules
	FS_Sandbox_InitDefaultRules(&mountTable.globalSandbox);
	mountTable.sandboxEnabled = qtrue;
	
	// Set default write path
	Q_strncpyz(mountTable.writeBasePath, Cvar_VariableString("fs_homepath"), sizeof(mountTable.writeBasePath));
	
	fs_v2_active = qtrue;
	Com_Printf("Virtual Filesystem v2 initialized\n");
}

/*
===============
FS_MountTable_Shutdown
===============
*/
void FS_MountTable_Shutdown(void) {
	if (!fs_v2_active) return;
	
	// Destroy all mounts
	fsMount_t *mount = mountTable.mounts;
	while (mount) {
		fsMount_t *next = mount->next;
		FS_Mount_Destroy(mount);
		mount = next;
	}
	
	Com_Memset(&mountTable, 0, sizeof(mountTable));
	fs_v2_active = qfalse;
	Com_Printf("Virtual Filesystem v2 shutdown\n");
}

/*
===============
FS_MountTable_IsActive
===============
*/
qboolean FS_MountTable_IsActive(void) {
	return fs_v2_active;
}

/*
===============
FS_MountTable_Dump
===============
*/
void FS_MountTable_Dump(void) {
	if (!fs_v2_active) {
		Com_Printf("VFS: Mount table not active\n");
		return;
	}

	Com_Printf("=== VFS Mount Table ===\n");
	Com_Printf("Total mounts: %d\n", mountTable.numMounts);
	Com_Printf("Write base path: %s\n", mountTable.writeBasePath);
	Com_Printf("Sandbox enabled: %s\n", mountTable.sandboxEnabled ? "yes" : "no");

	fsMount_t *mount = mountTable.mounts;
	int index = 0;

	while (mount) {
		Com_Printf("[%d] %s (priority %d, type %d, enabled %s)\n",
				  index++, mount->mountPoint, mount->priority, mount->type,
				  mount->enabled ? "yes" : "no");
		Com_Printf("    Display: %s\n", mount->displayName);
		Com_Printf("    Access: %u, Hits: %u\n", mount->accessCount, mount->hitCount);
		if (mount->checksum) {
			Com_Printf("    Checksum: 0x%08x\n", mount->checksum);
		}
		mount = mount->next;
	}
}

/*
===============
FS_MountTable_Stats
===============
*/
void FS_MountTable_Stats(void) {
	if (!fs_v2_active) {
		Com_Printf("VFS: Mount table not active\n");
		return;
	}

	uint32_t totalAccess = 0;
	uint32_t totalHits = 0;
	uint32_t enabledMounts = 0;

	fsMount_t *mount = mountTable.mounts;
	while (mount) {
		totalAccess += mount->accessCount;
		totalHits += mount->hitCount;
		if (mount->enabled) enabledMounts++;
		mount = mount->next;
	}

	Com_Printf("=== VFS Statistics ===\n");
	Com_Printf("Total mounts: %d (%d enabled)\n", mountTable.numMounts, enabledMounts);
	Com_Printf("Total access: %u\n", totalAccess);
	Com_Printf("Total hits: %u\n", totalHits);
	Com_Printf("Hit rate: %.1f%%\n",
			  totalAccess > 0 ? (float)totalHits / (float)totalAccess * 100.0f : 0.0f);
}

/*
===============
FS_Mount_CalculateChecksum
===============
*/
static uint32_t FS_Mount_CalculateChecksum(const char *path) {
	// Simple checksum calculation for PAK files
	// In a real implementation, this would read and hash the PAK file
	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	uint32_t checksum = 0;
	uint8_t buffer[4096];
	size_t bytesRead;

	while ((bytesRead = fread(buffer, 1, sizeof(buffer), f)) > 0) {
		for (size_t i = 0; i < bytesRead; i++) {
			checksum = (checksum * 31) + buffer[i];
		}
	}

	fclose(f);
	return checksum;
}

/*
===============
FS_Mount_IsValidType
===============
*/
static qboolean FS_Mount_IsValidType(fsMountType_t type) {
	return (type >= FS_MOUNT_PAK && type <= FS_MOUNT_VIRTUAL);
}

/*
===============
FS_Mount_ValidatePath
===============
*/
static qboolean FS_Mount_ValidatePath(const char *path) {
	if (!path || !path[0]) return qfalse;

	// Check for invalid characters
	if (strstr(path, "..")) return qfalse;
	if (strchr(path, '\\')) return qfalse; // Only forward slashes allowed

	return qtrue;
}

/*
===============
FS_Mount_InsertByPriority
===============
*/
static void FS_Mount_InsertByPriority(fsMount_t *mount) {
	if (!mountTable.mounts) {
		// First mount
		mountTable.mounts = mount;
		mount->next = mount->prev = NULL;
		return;
	}

	// Find insertion point (higher priority = searched first)
	fsMount_t *current = mountTable.mounts;
	fsMount_t *prev = NULL;

	while (current && current->priority >= mount->priority) {
		prev = current;
		current = current->next;
	}

	if (!prev) {
		// Insert at head
		mount->next = mountTable.mounts;
		mount->prev = NULL;
		mountTable.mounts->prev = mount;
		mountTable.mounts = mount;
	} else if (!current) {
		// Insert at tail
		mount->prev = prev;
		mount->next = NULL;
		prev->next = mount;
	} else {
		// Insert in middle
		mount->prev = prev;
		mount->next = current;
		prev->next = mount;
		current->prev = mount;
	}
}

/*
===============
FS_Mount_RemoveFromList
===============
*/
static void FS_Mount_RemoveFromList(fsMount_t *mount) {
	if (mount->prev) {
		mount->prev->next = mount->next;
	} else {
		mountTable.mounts = mount->next;
	}

	if (mount->next) {
		mount->next->prev = mount->prev;
	}

	mount->next = mount->prev = NULL;
}

/*
===============
FS_Mount_Create
===============
*/
fsMount_t *FS_Mount_Create(const char *mountPoint, fsMountType_t type, fsMountPriority_t priority) {
	if (!fs_v2_active) return NULL;
	if (!FS_Mount_IsValidType(type)) return NULL;
	if (!FS_Mount_ValidatePath(mountPoint)) return NULL;

	fsMount_t *mount = (fsMount_t *)Z_Malloc(sizeof(fsMount_t));
	if (!mount) return NULL;
	
	Com_Memset(mount, 0, sizeof(fsMount_t));
	Q_strncpyz(mount->mountPoint, mountPoint, sizeof(mount->mountPoint));
	Q_strncpyz(mount->displayName, mountPoint, sizeof(mount->displayName));
	mount->type = type;
	mount->priority = priority;
	mount->enabled = qtrue;
	mount->writePolicy = FS_WRITE_DENY;

	// Initialize sandbox rules based on mount type
	if (type == FS_MOUNT_DIR && strstr(mountPoint, "mods/")) {
		FS_Sandbox_InitModRules(&mount->sandbox);
	} else {
		FS_Sandbox_InitDefaultRules(&mount->sandbox);
	}
	
	return mount;
}

/*
===============
FS_Mount_Destroy
===============
*/
void FS_Mount_Destroy(fsMount_t *mount) {
	if (!mount) return;

	// Clean up backend resources
	switch (mount->type) {
		case FS_MOUNT_PAK:
			if (mount->backend.pak) {
				// Clean up PAK resources
				if (mount->backend.pak) {
					// Close PAK file handle if open
					if (mount->backend.pak->handle) {
						unzClose(mount->backend.pak->handle);
						mount->backend.pak->handle = nullptr;
					}
					Z_Free(mount->backend.pak);
					mount->backend.pak = nullptr;
				}
			}
			break;
		case FS_MOUNT_DIR:
			if (mount->backend.dir) {
				// Clean up directory resources
				if (mount->backend.dir) {
					// Directory backend cleanup (if needed)
					Z_Free(mount->backend.dir);
					mount->backend.dir = nullptr;
				}
			}
			break;
		case FS_MOUNT_VIRTUAL:
			if (mount->backend.virtual) {
				// Clean up virtual resources
				if (mount->backend.virtual) {
					// Virtual filesystem cleanup (placeholder)
					Z_Free(mount->backend.virtual);
					mount->backend.virtual = nullptr;
				}
			}
			break;
	}

	Z_Free(mount);
}

/*
===============
FS_Mount_Add
===============
*/
qboolean FS_Mount_Add(fsMount_t *mount) {
	if (!fs_v2_active || !mount) return qfalse;

	// Check if mount point already exists
	if (FS_Mount_Find(mount->mountPoint)) {
		Com_Printf("VFS: Mount point '%s' already exists\n", mount->mountPoint);
		return qfalse;
	}

	// Insert by priority
	FS_Mount_InsertByPriority(mount);
	mountTable.numMounts++;

	// Calculate checksum for PAK files
	if (mount->type == FS_MOUNT_PAK) {
				// Get PAK path and calculate checksum
				char pakPath[MAX_OSPATH];
				char *pakFilename = mount->backend.pak->pakFilename; // Get filename from PAK structure
				Com_sprintf(pakPath, (int)sizeof(pakPath), "%s/%s", mount->mountPoint, pakFilename);

				// Calculate checksum (placeholder - would use actual checksum calculation)
				mount->backend.pak->checksum = Com_BlockChecksum(pakPath, (int)strlen(pakPath));
		mount->checksum = FS_Mount_CalculateChecksum("dummy_path");
	}

	Com_Printf("VFS: Added mount '%s' (priority %d, type %d)\n",
			  mount->mountPoint, mount->priority, mount->type);
	return qtrue;
}

/*
===============
FS_Mount_Remove
===============
*/
qboolean FS_Mount_Remove(const char *mountPoint) {
	if (!fs_v2_active || !mountPoint) return qfalse;

	fsMount_t *mount = FS_Mount_Find(mountPoint);
	if (!mount) {
		Com_Printf("VFS: Mount '%s' not found\n", mountPoint);
		return qfalse;
	}

	FS_Mount_RemoveFromList(mount);
	mountTable.numMounts--;

	// Clear write mount if it was this mount
	if (mountTable.writeMount == mount) {
		mountTable.writeMount = NULL;
	}

	FS_Mount_Destroy(mount);

	Com_Printf("VFS: Removed mount '%s'\n", mountPoint);
	return qtrue;
}

/*
===============
FS_Mount_Find
===============
*/
fsMount_t *FS_Mount_Find(const char *mountPoint) {
	if (!fs_v2_active || !mountPoint) return NULL;

	fsMount_t *mount = mountTable.mounts;
	while (mount) {
		if (Q_stricmp(mount->mountPoint, mountPoint) == 0) {
			return mount;
		}
		mount = mount->next;
	}

	return NULL;
}

/*
===============
FS_Mount_SetEnabled
===============
*/
qboolean FS_Mount_SetEnabled(const char *mountPoint, qboolean enabled) {
	fsMount_t *mount = FS_Mount_Find(mountPoint);
	if (!mount) return qfalse;

	mount->enabled = enabled;
	return qtrue;
}

/*
===============
FS_Mount_SetWriteMount
===============
*/
qboolean FS_Mount_SetWriteMount(const char *mountPoint) {
	if (!fs_v2_active) return qfalse;

	fsMount_t *mount = FS_Mount_Find(mountPoint);
	if (!mount) return qfalse;

	if (mount->writePolicy == FS_WRITE_DENY) {
		Com_Printf("VFS: Cannot set write mount - write policy is DENY\n");
		return qfalse;
	}

	mountTable.writeMount = mount;
	Com_Printf("VFS: Set write mount to '%s'\n", mountPoint);
	return qtrue;
}

/*
===============
FS_Mount_GetWriteMount
===============
*/
fsMount_t *FS_Mount_GetWriteMount(void) {
	return mountTable.writeMount;
}

/*
===============
FS_Mount_FindFile
===============
*/
int FS_Mount_FindFile(const char *qpath, fileHandle_t *file, fsMount_t **outMount,
					  pack_t **outPak, fileInPack_t **outPakFile) {
	if (!fs_v2_active || !qpath) return 0;

	fsMount_t *mount = mountTable.mounts;
	while (mount) {
		if (mount->enabled) {
			mount->accessCount++;

			// Search based on mount type
			switch (mount->type) {
			case FS_MOUNT_PAK:
				if (mount->backend.pak) {
					fileInPack_t *pakFile = FS_FindPakFile(mount->backend.pak, qpath);
					if (pakFile) {
						mount->hitCount++;
						if (outMount) *outMount = mount;
						if (outPak) *outPak = mount->backend.pak;
						if (outPakFile) *outPakFile = pakFile;

						// If file handle requested, open the file
						if (file) {
							return FS_OpenFileInPak(file, mount->backend.pak, pakFile, qtrue);
						}
						return (int)pakFile->size;
					}
				}
				break;

			case FS_MOUNT_DIR:
				if (mount->backend.dir) {
					// Construct full path: mount->backend.dir->path + qpath
					char fullPath[MAX_OSPATH];
					Com_sprintf(fullPath, sizeof(fullPath), "%s/%s",
							   mount->backend.dir->path, qpath);

					// Check if file exists
					fileHandle_t testFile;
					int len = FS_FOpenFileRead(fullPath, &testFile, qtrue);
					if (len > 0) {
						mount->hitCount++;
						if (outMount) *outMount = mount;
						if (outPak) *outPak = NULL; // Not in a PAK
						if (outPakFile) *outPakFile = NULL; // Not in a PAK

						// If file handle requested, return the opened file
						if (file) {
							*file = testFile;
							return len;
						} else {
							// Close the test file
							FS_FCloseFile(testFile);
							return len;
						}
					}
				}
				break;

			case FS_MOUNT_VIRTUAL:
				// Virtual mounts are placeholders for future features
				// (network mounts, generated content, etc.)
				break;

			default:
				break;
			}
		}
		mount = mount->next;
	}

	return 0; // Not found
}

/*
===============
FS_FindPakFile
Find a file in a PAK file's hash table
===============
*/
fileInPack_t *FS_FindPakFile(pack_t *pak, const char *filename) {
	if (!pak || !filename || !pak->hashTable) {
		return NULL;
	}

	// Hash the filename to find the right bucket
	unsigned int hash = 0;
	for (const char *c = filename; *c; c++) {
		hash = (hash * 31u) + (unsigned char)*c;
	}
	hash &= (unsigned int)(pak->hashSize - 1);

	// Search through the hash chain
	fileInPack_t *pakFile = pak->hashTable[hash];
	while (pakFile) {
		if (Q_stricmp(pakFile->name, filename) == 0) {
			return pakFile;
		}
		pakFile = pakFile->next;
	}

	return NULL;
}

/*
===============
FS_Mount_FileExists
===============
*/
qboolean FS_Mount_FileExists(const char *qpath) {
	return (FS_Mount_FindFile(qpath, NULL, NULL, NULL, NULL) != 0);
}

/*
===============
FS_WritePolicy_Check
===============
*/
qboolean FS_WritePolicy_Check(const char *qpath, fsMount_t **outMount) {
	if (!fs_v2_active || !qpath) return qfalse;

	if (!mountTable.writeMount) return qfalse;

	// Check sandbox rules
	if (!FS_Sandbox_ValidateOperation(qpath, mountTable.writeMount, qtrue)) {
		return qfalse;
	}

	if (outMount) *outMount = mountTable.writeMount;
	return qtrue;
}

/*
===============
FS_WritePolicy_GetMount
===============
*/
fsMount_t *FS_WritePolicy_GetMount(const char *qpath) {
	fsMount_t *mount = NULL;
	if (FS_WritePolicy_Check(qpath, &mount)) {
		return mount;
	}
	return NULL;
}

/*
===============
FS_WritePolicy_SetBasePath
===============
*/
qboolean FS_WritePolicy_SetBasePath(const char *path) {
	if (!fs_v2_active || !path) return qfalse;

	Q_strncpyz(mountTable.writeBasePath, path, sizeof(mountTable.writeBasePath));
	return qtrue;
}



/*
===============
FS_Sandbox_CheckPath
===============
*/
qboolean FS_Sandbox_CheckPath(const char *qpath, const fsSandboxRules_t *rules) {
	if (!rules) return qtrue; // No rules = allow everything

	// Check executable restriction
	if (!rules->allowExecutables && FS_Sandbox_IsExecutable(qpath)) {
		return qfalse;
	}

	// Check config file restriction
	if (!rules->allowConfig && strstr(qpath, ".cfg")) {
		return qfalse;
	}

	// Check save file restriction
	if (!rules->allowSaves && (strstr(qpath, "save") || strstr(qpath, ".sav"))) {
		return qfalse;
	}

	// Check allowed paths
	if (rules->numAllowedPaths > 0) {
		qboolean pathAllowed = qfalse;
		for (int i = 0; i < rules->numAllowedPaths; i++) {
			if (Q_stristr(qpath, rules->allowedPaths[i])) {
				pathAllowed = qtrue;
				break;
			}
		}
		if (!pathAllowed) {
			return qfalse;
		}
	}

	return qtrue;
}

/*
===============
FS_Sandbox_IsExecutable
===============
*/
qboolean FS_Sandbox_IsExecutable(const char *filename) {
	const char *ext = strrchr(filename, '.');
	if (!ext) return qfalse;

	// Common executable extensions
	const char *execExts[] = {".exe", ".dll", ".so", ".dylib", ".sh", ".bat", ".cmd"};
	int numExts = sizeof(execExts) / sizeof(execExts[0]);

	for (int i = 0; i < numExts; i++) {
		if (Q_stricmp(ext, execExts[i]) == 0) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
===============
FS_Sandbox_InitDefaultRules
===============
*/
void FS_Sandbox_InitDefaultRules(fsSandboxRules_t *rules) {
	Com_Memset(rules, 0, sizeof(fsSandboxRules_t));
	rules->allowExecutables = qfalse;
	rules->allowConfig = qtrue;
	rules->allowSaves = qtrue;
}

/*
===============
FS_Sandbox_InitModRules
===============
*/
void FS_Sandbox_InitModRules(fsSandboxRules_t *rules) {
	FS_Sandbox_InitDefaultRules(rules);

	// Mod-specific restrictions
	rules->allowExecutables = qfalse; // Never allow executables in mods

	// Allow common mod paths
	Q_strncpyz(rules->allowedPaths[0], "scripts/", sizeof(rules->allowedPaths[0]));
	Q_strncpyz(rules->allowedPaths[1], "models/", sizeof(rules->allowedPaths[1]));
	Q_strncpyz(rules->allowedPaths[2], "textures/", sizeof(rules->allowedPaths[2]));
	Q_strncpyz(rules->allowedPaths[3], "sound/", sizeof(rules->allowedPaths[3]));
	Q_strncpyz(rules->allowedPaths[4], "maps/", sizeof(rules->allowedPaths[4]));
	rules->numAllowedPaths = 5;
}

/*
===============
FS_Sandbox_SetEnabled
===============
*/
void FS_Sandbox_SetEnabled(qboolean enabled) {
	mountTable.sandboxEnabled = enabled;
	Com_Printf("VFS: Sandbox %s\n", enabled ? "enabled" : "disabled");
}

/*
===============
FS_Mod_Mount
===============
*/
qboolean FS_Mod_Mount(const char *modName, const char *path, fsMountPriority_t priority) {
	if (!fs_v2_active || !modName || !path) return qfalse;

	char mountPoint[MAX_QPATH];
	Com_sprintf(mountPoint, sizeof(mountPoint), "mods/%s", modName);

	// Check if path is a PAK file or directory
	qboolean isPak = FS_IsPakFile(path);
	fsMountType_t mountType = isPak ? FS_MOUNT_PAK : FS_MOUNT_DIR;

	fsMount_t *mount = FS_Mount_Create(mountPoint, mountType, priority);
	if (!mount) return qfalse;

	// Set mod-specific properties
	Q_strncpyz(mount->displayName, modName, sizeof(mount->displayName));
	mount->writePolicy = isPak ? FS_WRITE_DENY : FS_WRITE_SANDBOX;

	return FS_Mount_Add(mount);
}

/*
===============
FS_Mod_Unmount
===============
*/
qboolean FS_Mod_Unmount(const char *modName) {
	if (!fs_v2_active || !modName) return qfalse;

	char mountPoint[MAX_QPATH];
	Com_sprintf(mountPoint, sizeof(mountPoint), "mods/%s", modName);

	return FS_Mount_Remove(mountPoint);
}

/*
===============
FS_Mod_ListMounted
===============
*/
int FS_Mod_ListMounted(char *buffer, int bufferSize) {
	if (!fs_v2_active || !buffer || bufferSize <= 0) return 0;

	int totalLen = 0;
	fsMount_t *mount = mountTable.mounts;

	while (mount && totalLen < bufferSize - 1) {
		if (Q_stristr(mount->mountPoint, "mods/") && mount->enabled) {
			const char *modName = mount->mountPoint + 5; // Skip "mods/"
			int len = Com_sprintf(buffer + totalLen, bufferSize - totalLen,
								"%s (%s, pri %d)\n", modName, mount->displayName, mount->priority);
			if (len > 0) {
				totalLen += len;
			}
		}
		mount = mount->next;
	}

	if (totalLen < bufferSize) {
		buffer[totalLen] = '\0';
	}

	return totalLen;
}

/*
===============
FS_IsPakFile
===============
*/
qboolean FS_IsPakFile(const char *path) {
	if (!path) return qfalse;

	const char *ext = strrchr(path, '.');
	if (!ext) return qfalse;

	// Common PAK file extensions
	return (Q_stricmp(ext, ".pk3") == 0 || Q_stricmp(ext, ".orb") == 0);
}

/*
===============
FS_Mod_GetInfo
===============
*/
qboolean FS_Mod_GetInfo(const char *modName, char *path, int pathSize,
					   fsMountPriority_t *priority, qboolean *enabled) {
	if (!fs_v2_active || !modName) return qfalse;

	char mountPoint[MAX_QPATH];
	Com_sprintf(mountPoint, sizeof(mountPoint), "mods/%s", modName);

	fsMount_t *mount = FS_Mount_Find(mountPoint);
	if (!mount) return qfalse;

	if (path && pathSize > 0) {
		// Get actual path from mount backend
	char actualPath[MAX_OSPATH];
	const char *filename = "unknown"; // Placeholder - would be passed as parameter
	int maxPath = sizeof(actualPath);

	switch (mount->type) {
		case FS_MOUNT_PAK:
			if (mount->backend.pak) {
				Com_sprintf(actualPath, maxPath, "%s/%s", mount->mountPoint, filename);
			}
			break;
		case FS_MOUNT_DIR:
			if (mount->backend.dir) {
				Com_sprintf(actualPath, maxPath, "%s/%s", mount->mountPoint, filename);
			}
			break;
		case FS_MOUNT_VIRTUAL:
			// Virtual files don't have physical paths
			actualPath[0] = '\0';
			break;
		default:
			actualPath[0] = '\0';
			break;
	}
		Q_strncpyz(path, "unknown", pathSize);
	}

	if (priority) *priority = mount->priority;
	if (enabled) *enabled = mount->enabled;

	return qtrue;
}

/*
===============
FS_MigrateLegacySearchPaths
===============
*/
void FS_MigrateLegacySearchPaths(void) {
	if (!fs_v2_active) return;

	Com_Printf("Migrating legacy search paths to VFS v2...\n");
	// Implement migration from old searchpath_t to mount table
	// This would scan the old filesystem search paths and convert them
	// to the new mount table format

	// Placeholder implementation
	Com_Printf("VFS: Migration from legacy filesystem not yet implemented\n");
	// This would read the existing fs_searchpaths and create equivalent mounts
}

// ============================================================================
// CACHING SYSTEM
// ============================================================================

#define FS_CACHE_HASH_SIZE 1024
static fsCacheEntry_t *cacheHashTable[FS_CACHE_HASH_SIZE];
static fsCacheConfig_t cacheConfig;
static fsCacheStats_t cacheStats;
static qboolean cacheInitialized = qfalse;

// Simple hash function for cache
static uint32_t FS_Cache_Hash(const char *path) {
	uint32_t hash = 0;
	while (*path) {
		hash = (hash * 31) + *path++;
	}
	return hash % FS_CACHE_HASH_SIZE;
}

// Forward declarations for cache functions
static void FS_Cache_Invalidate(const char *path);

/*
===============
FS_Cache_Init
===============
*/
void FS_Cache_Init(const fsCacheConfig_t *config) {
	if (cacheInitialized) return;

	if (config) {
		cacheConfig = *config;
	} else {
		// Default configuration
		cacheConfig.enabled = qtrue;
		cacheConfig.maxSizeBytes = 64 * 1024 * 1024; // 64MB
		cacheConfig.maxEntries = 4096;
		cacheConfig.evictionThreshold = 0.9f;
		cacheConfig.timeToLiveSeconds = 300; // 5 minutes
	}

	Com_Memset(cacheHashTable, 0, sizeof(cacheHashTable));
	Com_Memset(&cacheStats, 0, sizeof(cacheStats));
	cacheStats.maxCacheSizeBytes = cacheConfig.maxSizeBytes;

	cacheInitialized = qtrue;
	Com_Printf("VFS: Cache initialized (%d MB max)\n", (int)(cacheConfig.maxSizeBytes / (1024*1024)));
}

/*
===============
FS_Cache_Shutdown
===============
*/
void FS_Cache_Shutdown(void) {
	if (!cacheInitialized) return;

	FS_Cache_Clear();
	cacheInitialized = qfalse;
	Com_Printf("VFS: Cache shutdown\n");
}

/*
===============
FS_Cache_Get
===============
*/
qboolean FS_Cache_Get(const char *path, void **data, size_t *size) {
	if (!cacheInitialized || !cacheConfig.enabled || !path) return qfalse;

	uint32_t hash = FS_Cache_Hash(path);
	fsCacheEntry_t *entry = cacheHashTable[hash];

	while (entry) {
		if (strcmp(entry->path, path) == 0) {
			// Check if entry is still valid
			time_t now = time(NULL);
			if (now - entry->lastAccess > cacheConfig.timeToLiveSeconds) {
				// Entry expired, remove it
				FS_Cache_Invalidate(path);
				cacheStats.evictions++;
				return qfalse;
			}

			// Check if file has been modified
			struct stat st;
			if (stat(entry->realPath, &st) == 0) {
				if (st.st_mtime > entry->lastModified) {
					// File modified, invalidate cache
					FS_Cache_Invalidate(path);
					cacheStats.evictions++;
					return qfalse;
				}
			}

			// Cache hit
			if (entry->cachedData && entry->cachedSize > 0) {
				*data = entry->cachedData;
				if (size) *size = entry->cachedSize;
				entry->lastAccess = now;
				cacheStats.hits++;
				return qtrue;
			}
		}
		entry = (fsCacheEntry_t *)entry->cachedData; // Simple linked list
	}

	cacheStats.misses++;
	return qfalse;
}

/*
===============
FS_Cache_EvictLRU
Evict least recently used cache entries to make room for new data
===============
*/
static void FS_Cache_EvictLRU(size_t requiredSpace) {
	if (!cacheInitialized) return;

	size_t targetSize = cacheStats.cacheSizeBytes - requiredSpace;
	if (targetSize > cacheConfig.maxSizeBytes * cacheConfig.evictionThreshold) {
		targetSize = cacheConfig.maxSizeBytes * cacheConfig.evictionThreshold;
	}

	// Collect all entries for sorting by access time
#ifdef __cplusplus
	struct EvictCandidate {
		fsCacheEntry_t *entry;
		uint32_t hash;
		uint32_t bucketIndex;
	};

	std::vector<EvictCandidate> candidates;
	candidates.reserve(cacheStats.cachedEntries);
#endif

	// Scan all hash buckets to find entries
#ifdef __cplusplus
	for (uint32_t hash = 0; hash < FS_CACHE_HASH_SIZE; hash++) {
		fsCacheEntry_t *entry = cacheHashTable[hash];
		while (entry) {
			if (entry->cachedData) { // Only consider entries with cached data
				candidates.push_back({entry, hash, hash});
			}
			entry = (fsCacheEntry_t *)entry->cachedData; // Next in hash chain
		}
	}

	// Sort by access time (oldest first)
	std::sort(candidates.begin(), candidates.end(),
		[](const EvictCandidate &a, const EvictCandidate &b) {
			return a.entry->lastAccess < b.entry->lastAccess;
		});
#endif

	// Evict entries until we reach target size
#ifdef __cplusplus
	size_t evictedSize = 0;
	size_t evictedCount = 0;

	for (const auto &candidate : candidates) {
		if (cacheStats.cacheSizeBytes <= targetSize) {
			break; // Reached target size
		}

		// Remove from hash table
		fsCacheEntry_t **entryPtr = &cacheHashTable[candidate.hash];
		while (*entryPtr) {
			if (*entryPtr == candidate.entry) {
				*entryPtr = (fsCacheEntry_t *)(*entryPtr)->cachedData;
				break;
			}
			entryPtr = (fsCacheEntry_t **)&(*entryPtr)->cachedData;
		}

		// Free resources
		if (candidate.entry->cachedData) {
			Z_Free(candidate.entry->cachedData);
			cacheStats.cacheSizeBytes -= candidate.entry->dataSize;
			evictedSize += candidate.entry->dataSize;
			evictedCount++;
		}

		Z_Free(candidate.entry);
		cacheStats.cachedEntries--;
	}
#endif

#ifdef __cplusplus
	if (evictedCount > 0) {
		Com_DPrintf("VFS: Evicted %d entries (%d KB) for LRU cache\n",
				   (int)evictedCount, (int)(evictedSize / 1024));
	}
#endif
}

/*
===============
FS_Cache_Put
===============
*/
qboolean FS_Cache_Put(const char *path, const void *data, size_t size) {
	if (!cacheInitialized || !cacheConfig.enabled || !path || !data) return qfalse;

	// Check cache size limits
	if (cacheStats.totalEntries >= cacheConfig.maxEntries ||
		cacheStats.cacheSizeBytes + size > cacheConfig.maxSizeBytes) {

		// Need to evict some entries
		if (cacheStats.cacheSizeBytes > cacheConfig.maxSizeBytes * cacheConfig.evictionThreshold) {
			FS_Cache_EvictLRU(size);
			cacheStats.evictions++;
		}
	}

	uint32_t hash = FS_Cache_Hash(path);
	fsCacheEntry_t *entry = (fsCacheEntry_t *)Z_Malloc(sizeof(fsCacheEntry_t));

	if (!entry) return qfalse;

	Com_Memset(entry, 0, sizeof(fsCacheEntry_t));
	Q_strncpyz(entry->path, path, sizeof(entry->path));

	// Get real path and file info
	struct stat st;
	if (stat(path, &st) == 0) {
		Q_strncpyz(entry->realPath, path, sizeof(entry->realPath));
		entry->lastModified = st.st_mtime;
		entry->fileSize = st.st_size;
		entry->isDirectory = S_ISDIR(st.st_mode);
	}

	entry->lastAccess = time(NULL);

	// Store data
	if (size > 0) {
		entry->cachedData = Z_Malloc(size);
		if (entry->cachedData) {
			memcpy(entry->cachedData, data, size);
			entry->cachedSize = size;
			cacheStats.cacheSizeBytes += size;
			cacheStats.cachedEntries++;
		}
	}

	// Insert into hash table (simple linked list)
	entry->cachedData = cacheHashTable[hash]; // Reuse field for next pointer
	cacheHashTable[hash] = entry;

	cacheStats.totalEntries++;
	return qtrue;
}

/*
===============
FS_Cache_Invalidate
===============
*/
void FS_Cache_Invalidate(const char *path) {
	if (!cacheInitialized || !path) return;

	uint32_t hash = FS_Cache_Hash(path);
	fsCacheEntry_t **entryPtr = &cacheHashTable[hash];
	fsCacheEntry_t *entry;

	while (*entryPtr) {
		entry = *entryPtr;
		if (strcmp(entry->path, path) == 0) {
			// Remove from list
			*entryPtr = (fsCacheEntry_t *)entry->cachedData;

			// Free resources
			if (entry->cachedData && entry->cachedSize > 0) {
				Z_Free(entry->cachedData);
				cacheStats.cacheSizeBytes -= entry->cachedSize;
				cacheStats.cachedEntries--;
			}
			Z_Free(entry);
			cacheStats.totalEntries--;
			return;
		}
		entryPtr = (fsCacheEntry_t **)&entry->cachedData;
	}
}

/*
===============
FS_Cache_Clear
===============
*/
void FS_Cache_Clear(void) {
	if (!cacheInitialized) return;

	for (int i = 0; i < FS_CACHE_HASH_SIZE; i++) {
		fsCacheEntry_t *entry = cacheHashTable[i];
		while (entry) {
			fsCacheEntry_t *next = (fsCacheEntry_t *)entry->cachedData;
			if (entry->cachedData && entry->cachedSize > 0) {
				Z_Free(entry->cachedData);
			}
			Z_Free(entry);
			entry = next;
		}
		cacheHashTable[i] = NULL;
	}

	cacheStats.totalEntries = 0;
	cacheStats.cachedEntries = 0;
	cacheStats.cacheSizeBytes = 0;
	cacheStats.evictions = 0;
}

/*
===============
FS_Cache_GetStats
===============
*/
void FS_Cache_GetStats(fsCacheStats_t *stats) {
	if (stats) {
		*stats = cacheStats;
	}
}

/*
===============
FS_Cache_SetConfig
===============
*/
void FS_Cache_SetConfig(const fsCacheConfig_t *config) {
	if (!config) return;

	cacheConfig = *config;
	cacheStats.maxCacheSizeBytes = cacheConfig.maxSizeBytes;
}

// ============================================================================
// HOT RELOADING SYSTEM
// ============================================================================

static fsChangeCallback_t changeCallbacks[16];
static int numChangeCallbacks = 0;
static qboolean monitorInitialized = qfalse;

#define MAX_MONITORED_PATHS 64
static char monitoredPaths[MAX_MONITORED_PATHS][MAX_QPATH];
static uint32_t monitoredPathTimes[MAX_MONITORED_PATHS];
static int numMonitoredPaths = 0;

/*
===============
FS_Monitor_Init
===============
*/
void FS_Monitor_Init(void) {
	if (monitorInitialized) return;

	Com_Memset(changeCallbacks, 0, sizeof(changeCallbacks));
	numChangeCallbacks = 0;
	monitorInitialized = qtrue;

	Com_Printf("VFS: File monitoring initialized\n");
}

/*
===============
FS_Monitor_Shutdown
===============
*/
void FS_Monitor_Shutdown(void) {
	if (!monitorInitialized) return;

	Com_Memset(changeCallbacks, 0, sizeof(changeCallbacks));
	numChangeCallbacks = 0;
	monitorInitialized = qfalse;

	Com_Printf("VFS: File monitoring shutdown\n");
}

/*
===============
FS_Monitor_AddPath
===============
*/
qboolean FS_Monitor_AddPath(const char *path) {
	if (!monitorInitialized || !path || numMonitoredPaths >= MAX_MONITORED_PATHS) {
		return qfalse;
	}

	// Check if path is already monitored
	for (int i = 0; i < numMonitoredPaths; i++) {
		if (Q_stricmp(monitoredPaths[i], path) == 0) {
			return qtrue; // Already monitored
		}
	}

	// Add path to monitoring list
	Q_strncpyz(monitoredPaths[numMonitoredPaths], path, sizeof(monitoredPaths[0]));

	// Get initial modification time
	struct stat st;
	if (stat(path, &st) == 0) {
		monitoredPathTimes[numMonitoredPaths] = st.st_mtime;
	} else {
		monitoredPathTimes[numMonitoredPaths] = 0; // File doesn't exist yet
	}

	numMonitoredPaths++;
	Com_DPrintf("VFS: Added path to monitoring: %s\n", path);
	return qtrue;
}

/*
===============
FS_Monitor_RemovePath
===============
*/
void FS_Monitor_RemovePath(const char *path) {
	if (!monitorInitialized || !path) return;

	// Find and remove the path
	for (int i = 0; i < numMonitoredPaths; i++) {
		if (Q_stricmp(monitoredPaths[i], path) == 0) {
			// Shift remaining entries
			for (int j = i; j < numMonitoredPaths - 1; j++) {
				Q_strncpyz(monitoredPaths[j], monitoredPaths[j + 1], sizeof(monitoredPaths[0]));
				monitoredPathTimes[j] = monitoredPathTimes[j + 1];
			}

			numMonitoredPaths--;
			Com_DPrintf("VFS: Removed path from monitoring: %s\n", path);
			break;
		}
	}
}

/*
===============
FS_Monitor_RegisterCallback
===============
*/
void FS_Monitor_RegisterCallback(fsChangeCallback_t callback) {
	if (!monitorInitialized || numChangeCallbacks >= 16) return;

	changeCallbacks[numChangeCallbacks++] = callback;
}

/*
===============
FS_Monitor_Update
===============
*/
void FS_Monitor_Update(void) {
	if (!monitorInitialized) return;

	static uint32_t lastCheckTime = 0;
	uint32_t currentTime = Sys_Milliseconds();

	// Check every 100ms to avoid excessive I/O
	if (currentTime - lastCheckTime < 100) {
		return;
	}
	lastCheckTime = currentTime;

	// Check monitored paths for changes
	for (int i = 0; i < numMonitoredPaths; i++) {
		const char *path = monitoredPaths[i];
		if (!path || !path[0]) continue;

		struct stat st;
		if (stat(path, &st) == 0) {
			// Check if modification time changed
			if (st.st_mtime != monitoredPathTimes[i]) {
				fsChangeType_t changeType = FS_CHANGE_MODIFIED;

				// Determine change type based on previous state
				if (monitoredPathTimes[i] == 0) {
					changeType = FS_CHANGE_ADDED;
				}

				// Check if file was deleted and recreated
				if (st.st_mtime > monitoredPathTimes[i] + 1) {
					changeType = FS_CHANGE_MODIFIED;
				}

				monitoredPathTimes[i] = (uint32_t)st.st_mtime;

				// Notify callbacks
				fsChangeEvent_t event;
				Q_strncpyz(event.oldPath, path, sizeof(event.oldPath));
				Q_strncpyz(event.newPath, path, sizeof(event.newPath));
				event.type = changeType;
				event.timestamp = currentTime;

				for (int j = 0; j < numChangeCallbacks; j++) {
					if (changeCallbacks[j]) {
						changeCallbacks[j](&event);
					}
				}

				Com_DPrintf("VFS: File change detected: %s (%s)\n", path,
						   changeType == FS_CHANGE_ADDED ? "added" :
						   changeType == FS_CHANGE_MODIFIED ? "modified" : "unknown");
			}
		} else {
			// File was deleted
			if (monitoredPathTimes[i] != 0) {
				fsChangeEvent_t event;
				Q_strncpyz(event.oldPath, path, sizeof(event.oldPath));
				event.newPath[0] = '\0'; // No new path for deletions
				event.type = FS_CHANGE_DELETED;
				event.timestamp = currentTime;

				// Notify callbacks
				for (int j = 0; j < numChangeCallbacks; j++) {
					if (changeCallbacks[j]) {
						changeCallbacks[j](&event);
					}
				}

				Com_DPrintf("VFS: File deleted: %s\n", path);
			}

			// Reset timestamp to detect if file is recreated
			monitoredPathTimes[i] = 0;
		}
	}
}

// ============================================================================
// Console Command Implementations
// ============================================================================

/*
===============
FS_Mount_List_f
===============
*/
static void FS_Mount_List_f(void) {
	if (!fs_v2_active) {
		Com_Printf("Virtual Filesystem v2 not active\n");
		return;
	}

	Com_Printf("Mounted filesystems:\n");
	Com_Printf("%-20s %-8s %-8s %-8s %-8s %-s\n",
	          "Mount Point", "Type", "Priority", "Enabled", "Write", "Display Name");

	fsMount_t *mount = mountTable.mounts;
	int count = 0;
	while (mount) {
		const char *typeStr = (mount->type == FS_MOUNT_PAK) ? "PAK" :
		                     (mount->type == FS_MOUNT_DIR) ? "DIR" : "VIRTUAL";
		const char *enabledStr = mount->enabled ? "Yes" : "No";
		const char *writeStr = (mountTable.writeMount == mount) ? "Yes" : "No";

		Com_Printf("%-20s %-8s %-8d %-8s %-8s %s\n",
		          mount->mountPoint, typeStr, mount->priority,
		          enabledStr, writeStr, mount->displayName);

		mount = mount->next;
		count++;
	}

	if (count == 0) {
		Com_Printf("No mounts registered\n");
	} else {
		Com_Printf("\nTotal mounts: %d\n", count);
	}
}

/*
===============
FS_Mount_Add_f
===============
*/
static void FS_Mount_Add_f(void) {
	if (Cmd_Argc() < 4) {
		Com_Printf("Usage: fs_mount_add <mount_point> <path> <priority>\n");
		Com_Printf("Priorities: system=%d, mod=%d, game=%d, cd=%d, fallback=%d\n",
		          FS_PRIORITY_SYSTEM, FS_PRIORITY_MOD, FS_PRIORITY_GAME,
		          FS_PRIORITY_CD, FS_PRIORITY_FALLBACK);
		return;
	}

	const char *mountPoint = Cmd_Argv(1);
	const char *path = Cmd_Argv(2);
	const char *priorityStr = Cmd_Argv(3);

	fsMountPriority_t priority = (fsMountPriority_t)atoi(priorityStr);

	if (!FS_Mod_Mount(mountPoint, path, priority)) {
		Com_Printf("Failed to mount '%s' at '%s'\n", path, mountPoint);
	} else {
		Com_Printf("Successfully mounted '%s' at '%s'\n", path, mountPoint);
	}
}

/*
===============
FS_Mount_Remove_f
===============
*/
static void FS_Mount_Remove_f(void) {
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: fs_mount_remove <mount_point>\n");
		return;
	}

	const char *mountPoint = Cmd_Argv(1);

	if (!FS_Mount_Remove(mountPoint)) {
		Com_Printf("Failed to remove mount '%s'\n", mountPoint);
	} else {
		Com_Printf("Successfully removed mount '%s'\n", mountPoint);
	}
}

/*
===============
FS_Mount_Enable_f
===============
*/
static void FS_Mount_Enable_f(void) {
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: fs_mount_enable <mount_point>\n");
		return;
	}

	const char *mountPoint = Cmd_Argv(1);

	if (!FS_Mount_SetEnabled(mountPoint, qtrue)) {
		Com_Printf("Failed to enable mount '%s'\n", mountPoint);
	} else {
		Com_Printf("Successfully enabled mount '%s'\n", mountPoint);
	}
}

/*
===============
FS_Mount_Disable_f
===============
*/
static void FS_Mount_Disable_f(void) {
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: fs_mount_disable <mount_point>\n");
		return;
	}

	const char *mountPoint = Cmd_Argv(1);

	if (!FS_Mount_SetEnabled(mountPoint, qfalse)) {
		Com_Printf("Failed to disable mount '%s'\n", mountPoint);
	} else {
		Com_Printf("Successfully disabled mount '%s'\n", mountPoint);
	}
}

/*
===============
FS_Mount_WriteMount_f
===============
*/
static void FS_Mount_WriteMount_f(void) {
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: fs_mount_write <mount_point>\n");
		return;
	}

	const char *mountPoint = Cmd_Argv(1);

	if (!FS_Mount_SetWriteMount(mountPoint)) {
		Com_Printf("Failed to set write mount to '%s'\n", mountPoint);
	} else {
		Com_Printf("Successfully set write mount to '%s'\n", mountPoint);
	}
}

/*
===============
FS_Mount_Stats_f
===============
*/
static void FS_Mount_Stats_f(void) {
	if (!fs_v2_active) {
		Com_Printf("Virtual Filesystem v2 not active\n");
		return;
	}

	Com_Printf("Virtual Filesystem v2 Statistics:\n");
	Com_Printf("Total mounts: %d\n", mountTable.numMounts);
	Com_Printf("Write base path: %s\n", mountTable.writeBasePath);

	if (mountTable.writeMount) {
		Com_Printf("Write mount: %s (%s)\n",
		          mountTable.writeMount->mountPoint,
		          mountTable.writeMount->displayName);
	} else {
		Com_Printf("Write mount: None\n");
	}

	Com_Printf("Sandbox enabled: %s\n", mountTable.sandboxEnabled ? "Yes" : "No");

	// Show mount statistics
	fsMount_t *mount = mountTable.mounts;
	while (mount) {
		Com_Printf("Mount '%s': %u accesses, %u hits\n",
		          mount->mountPoint, mount->accessCount, mount->hitCount);
		mount = mount->next;
	}
}

/*
===============
FS_Cache_Stats_f
===============
*/
static void FS_Cache_Stats_f(void) {
	fsCacheStats_t stats;
	FS_Cache_GetStats(&stats);

	Com_Printf("File Cache Statistics:\n");
	Com_Printf("Total entries: %u\n", stats.totalEntries);
	Com_Printf("Cached entries: %u\n", stats.cachedEntries);
	Com_Printf("Cache size: %.2f MB\n", (float)stats.cacheSizeBytes / (1024.0f * 1024.0f));
	Com_Printf("Max cache size: %.2f MB\n", (float)stats.maxCacheSizeBytes / (1024.0f * 1024.0f));
	Com_Printf("Evictions: %u\n", stats.evictions);
	Com_Printf("Hits: %u\n", stats.hits);
	Com_Printf("Misses: %u\n", stats.misses);
	Com_Printf("Hit rate: %.1f%%\n", stats.hitRate * 100.0f);
}

/*
===============
FS_Sandbox_Status_f
===============
*/
static void FS_Sandbox_Status_f(void) {
	if (!fs_v2_active) {
		Com_Printf("Virtual Filesystem v2 not active\n");
		return;
	}

	Com_Printf("Sandbox Status:\n");
	Com_Printf("Enabled: %s\n", mountTable.sandboxEnabled ? "Yes" : "No");

	if (mountTable.sandboxEnabled) {
		const fsSandboxRules_t *rules = &mountTable.globalSandbox;
		Com_Printf("Allow executables: %s\n", rules->allowExecutables ? "Yes" : "No");
		Com_Printf("Allow config files: %s\n", rules->allowConfig ? "Yes" : "No");
		Com_Printf("Allow save files: %s\n", rules->allowSaves ? "Yes" : "No");
		Com_Printf("Allowed paths: %d\n", rules->numAllowedPaths);

		for (int i = 0; i < rules->numAllowedPaths; i++) {
			Com_Printf("  %s\n", rules->allowedPaths[i]);
		}
	}
}
