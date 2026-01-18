/*
===========================================================================
Virtual Filesystem v2 - Stub implementation
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "files_v2.h"

static fsMountTable_t mountTable;
static qboolean fs_v2_active = qfalse;

void FS_MountTable_Init(void) {
	if (fs_v2_active) return;
	
	Com_Memset(&mountTable, 0, sizeof(mountTable));
	mountTable.sandboxEnabled = qtrue;
	
	// Set default write path
	Q_strncpyz(mountTable.writeBasePath, Cvar_VariableString("fs_homepath"), sizeof(mountTable.writeBasePath));
	
	fs_v2_active = qtrue;
	Com_Printf("Virtual Filesystem v2 initialized\n");
}

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

qboolean FS_MountTable_IsActive(void) {
	return fs_v2_active;
}

fsMount_t *FS_Mount_Create(const char *mountPoint, fsMountType_t type, fsMountPriority_t priority) {
	fsMount_t *mount = (fsMount_t *)Z_Malloc(sizeof(fsMount_t));
	if (!mount) return NULL;
	
	Q_strncpyz(mount->mountPoint, mountPoint, sizeof(mount->mountPoint));
	mount->type = type;
	mount->priority = priority;
	mount->enabled = qtrue;
	mount->writePolicy = FS_WRITE_DENY;
	
	return mount;
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
	(void)qpath; (void)mount; (void)isWrite;
	return qtrue; // Allowed for now
}
