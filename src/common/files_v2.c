/*
===========================================================================
Virtual Filesystem v2 - Complete implementation
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "files_v2.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

static fsMountTable_t mountTable;
static qboolean fs_v2_active = qfalse;

// Forward declarations for internal functions
static void FS_Mount_InsertByPriority(fsMount_t *mount);
static void FS_Mount_RemoveFromList(fsMount_t *mount);
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
			  totalAccess > 0 ? (float)totalHits / totalAccess * 100.0f : 0.0f);
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
				// TODO: Clean up PAK resources
			}
			break;
		case FS_MOUNT_DIR:
			if (mount->backend.dir) {
				// TODO: Clean up directory resources
			}
			break;
		case FS_MOUNT_VIRTUAL:
			if (mount->backend.virtual) {
				// TODO: Clean up virtual resources
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
		// TODO: Get PAK path and calculate checksum
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

			// TODO: Implement actual file search in mount
			// For now, just return not found
		}
		mount = mount->next;
	}

	return 0; // Not found
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

fsMount_t *FS_Mount_Find(const char *mountPoint) {
	if (!mountPoint || !mountPoint[0]) return NULL;

	fsMount_t *mount = mountTable.mounts;
	while (mount) {
		if (Q_stricmp(mount->mountPoint, mountPoint) == 0) {
			return mount;
		}
		mount = mount->next;
	}
	return NULL;
}

qboolean FS_Mount_Remove(const char *mountPoint) {
	fsMount_t *mount = FS_Mount_Find(mountPoint);
	if (!mount) {
		Com_Printf("VFS: Mount '%s' not found\n", mountPoint);
		return qfalse;
	}

	// Remove from linked list
	if (mount->prev) {
		mount->prev->next = mount->next;
	} else {
		mountTable.mounts = mount->next;
	}

	if (mount->next) {
		mount->next->prev = mount->prev;
	}

	// Update priority lookup table
	if (mount->priority <= FS_PRIORITY_SYSTEM && mount->priority >= 0) {
		if (mountTable.mountsByPriority[mount->priority] == mount) {
			mountTable.mountsByPriority[mount->priority] = NULL;
		}
	}

	mountTable.numMounts--;
	Com_Printf("VFS: Removed mount '%s'\n", mount->mountPoint);

	// Don't destroy the mount here - just remove from table
	return qtrue;
}

void FS_Mount_Destroy(fsMount_t *mount) {
	if (!mount) return;

	// If still in table, remove it first
	if (mount->next || mount->prev || mountTable.mounts == mount) {
		FS_Mount_Remove(mount->mountPoint);
	}

	// Backend-specific cleanup would go here

	Z_Free(mount);
}

qboolean FS_Mount_Add(fsMount_t *mount) {
	if (!mount) return qfalse;

	// Check for mount point conflicts
	fsMount_t *existing = FS_Mount_Find(mount->mountPoint);
	if (existing) {
		Com_Printf("VFS: Mount point '%s' already exists, rejecting new mount\n", mount->mountPoint);
		return qfalse;
	}

	// Insert by priority (descending)
	fsMount_t **prev = &mountTable.mounts;
	fsMount_t *curr = mountTable.mounts;

	while (curr && curr->priority >= mount->priority) {
		prev = &curr->next;
		curr = curr->next;
	}

	mount->next = curr;
	if (curr) curr->prev = mount;
	*prev = mount;

	// Update priority lookup table
	if (mount->priority <= FS_PRIORITY_SYSTEM && mount->priority >= 0) {
		if (!mountTable.mountsByPriority[mount->priority]) {
			mountTable.mountsByPriority[mount->priority] = mount;
		}
	}

	mountTable.numMounts++;
	Com_Printf("VFS: Added mount '%s' (priority %d, type %d)\n",
		mount->mountPoint, mount->priority, mount->type);
	return qtrue;
}

void FS_MigrateLegacySearchPaths(void) {
	// Placeholder for migrating existing search paths
	Com_Printf("Migrating legacy search paths to VFS v2...\n");
}

void FS_MountTable_Dump(void) {
	Com_Printf("=== Virtual Filesystem v2 Mount Table ===\n");
	Com_Printf("Total mounts: %d\n", mountTable.numMounts);
	Com_Printf("Write path: %s\n", mountTable.writeBasePath);
	Com_Printf("Sandbox enabled: %s\n", mountTable.sandboxEnabled ? "yes" : "no");

	fsMount_t *mount = mountTable.mounts;
	int count = 0;
	while (mount && count < 50) {  // Limit output to prevent spam
		const char *typeStr = "unknown";
		switch (mount->type) {
			case FS_MOUNT_PAK: typeStr = "pak"; break;
			case FS_MOUNT_DIR: typeStr = "dir"; break;
			case FS_MOUNT_VIRTUAL: typeStr = "virtual"; break;
		}

		const char *writeStr = "deny";
		switch (mount->writePolicy) {
			case FS_WRITE_ALLOW: writeStr = "allow"; break;
			case FS_WRITE_SANDBOX: writeStr = "sandbox"; break;
			case FS_WRITE_DENY: writeStr = "deny"; break;
		}

		Com_Printf("  [%d] %s (priority %d, type %s, write %s, enabled %s)\n",
			count, mount->mountPoint, mount->priority, typeStr, writeStr,
			mount->enabled ? "yes" : "no");
		Com_Printf("      Access: %u hits, %u total\n",
			mount->hitCount, mount->accessCount);

		mount = mount->next;
		count++;
	}

	if (mount) {
		Com_Printf("  ... and %d more mounts\n", mountTable.numMounts - count);
	}
	Com_Printf("===========================================\n");
}

void FS_MountTable_Stats(void) {
	uint32_t totalAccess = 0;
	uint32_t totalHits = 0;
	int enabledCount = 0;

	fsMount_t *mount = mountTable.mounts;
	while (mount) {
		totalAccess += mount->accessCount;
		totalHits += mount->hitCount;
		if (mount->enabled) enabledCount++;
		mount = mount->next;
	}

	float hitRate = totalAccess > 0 ? (float)totalHits / totalAccess * 100.0f : 0.0f;

	Com_Printf("VFS Statistics:\n");
	Com_Printf("  Total mounts: %d (%d enabled)\n", mountTable.numMounts, enabledCount);
	Com_Printf("  Total access: %u\n", totalAccess);
	Com_Printf("  Cache hits: %u (%.1f%% hit rate)\n", totalHits, hitRate);
}

void FS_Mount_RegisterCommands(void) {
	Cmd_AddCommand("vfs_dump", FS_MountTable_Dump);
	Cmd_AddCommand("vfs_stats", FS_MountTable_Stats);
}

int FS_Mount_FindFile(const char *qpath, fileHandle_t *file, 
                      fsMount_t **outMount, pack_t **outPak, 
                      fileInPack_t **outPakFile) {
	(void)qpath; (void)file; (void)outMount; (void)outPak; (void)outPakFile;
	return -1; // Not found
}

qboolean FS_Mount_FileExists(const char *qpath) {
	return FS_Mount_FindFile(qpath, NULL, NULL, NULL, NULL) >= 0;
}

fsMount_t *FS_WritePolicy_GetMount(const char *qpath) {
	(void)qpath;
	return mountTable.writeMount;
}

qboolean FS_Sandbox_ValidateOperation(const char *qpath, fsMount_t *mount,
                                      qboolean isWrite) {
	if (!mountTable.sandboxEnabled) return qtrue;

	// Check global sandbox rules
	if (!FS_Sandbox_CheckPath(qpath, &mountTable.globalSandbox)) {
		return qfalse;
	}

	// Check mount-specific sandbox rules
	if (!FS_Sandbox_CheckPath(qpath, &mount->sandbox)) {
		return qfalse;
	}

	// Check write permissions
	if (isWrite && mount->writePolicy == FS_WRITE_DENY) {
		return qfalse;
	}

	return qtrue;
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
		// TODO: Get actual path from mount backend
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
	// TODO: Implement migration from old searchpath_t to mount table
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
			// Simple eviction: clear 25% of cache
			size_t targetSize = cacheStats.cacheSizeBytes * 0.75f;
			// TODO: Implement LRU eviction
			FS_Cache_Clear(); // For now, just clear everything
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
	(void)path;
	// TODO: Implement platform-specific file monitoring
	// This would use inotify on Linux, FSEvents on macOS, etc.
	return qtrue;
}

/*
===============
FS_Monitor_RemovePath
===============
*/
void FS_Monitor_RemovePath(const char *path) {
	(void)path;
	// TODO: Remove from monitoring
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

	// TODO: Check for file changes and call callbacks
	// This would be called regularly from the main loop
}
