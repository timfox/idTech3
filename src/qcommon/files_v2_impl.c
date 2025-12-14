/*
===========================================================================
Virtual Filesystem v2 - Additional Implementation
Write Policy, Sandboxing, Mod Management, Console Commands, Migration
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "files_internal.h"
#include "files_v2.h"
#include "cmd.h"

// Forward declarations
struct searchpath_s;
typedef struct searchpath_s searchpath_t;

extern searchpath_t *fs_searchpaths;
extern int fs_checksumFeed;

// Mount table instance (shared with files_v2.c)
// Declared in files_v2.c
extern fsMountTable_t fs_mountTable;
extern qboolean fs_mountTableInitialized;

// Helper function from files_v2.c
extern qboolean FS_IsPakFile(const char *path);

// ============================================================================
// Write Policy Management
// ============================================================================

/*
================
FS_WritePolicy_Check
================
Check if write is allowed to path
================
*/
qboolean FS_WritePolicy_Check(const char *qpath, fsMount_t **outMount) {
	fsMount_t *mount;
	
	if (!qpath || !*qpath) {
		return qfalse;
	}
	
	// Check if mount table is active
	if (!FS_MountTable_IsActive()) {
		return qfalse;
	}
	
	// Use write mount if set
	if (fs_mountTable.writeMount) {
		mount = fs_mountTable.writeMount;
		if (mount->enabled && mount->writePolicy != FS_WRITE_DENY) {
			if (outMount) {
				*outMount = mount;
			}
			return qtrue;
		}
	}
	
	// Try to find a mount that allows writes
	for (mount = fs_mountTable.mounts; mount; mount = mount->next) {
		if (!mount->enabled) {
			continue;
		}
		
		if (mount->writePolicy == FS_WRITE_DENY) {
			continue;
		}
		
		// Check if path matches mount point
		if (Q_stricmpn(mount->mountPoint, qpath, strlen(mount->mountPoint)) == 0) {
			if (outMount) {
				*outMount = mount;
			}
			return qtrue;
		}
	}
	
	return qfalse;
}

/*
================
FS_WritePolicy_GetMount
================
Get write mount for path
================
*/
fsMount_t *FS_WritePolicy_GetMount(const char *qpath) {
	fsMount_t *mount = NULL;
	
	if (FS_WritePolicy_Check(qpath, &mount)) {
		return mount;
	}
	
	return NULL;
}

/*
================
FS_WritePolicy_SetBasePath
================
Set write base path
================
*/
qboolean FS_WritePolicy_SetBasePath(const char *path) {
	if (!path || !*path) {
		return qfalse;
	}
	
	Q_strncpyz(fs_mountTable.writeBasePath, path, sizeof(fs_mountTable.writeBasePath));
	return qtrue;
}

// ============================================================================
// Sandboxing
// ============================================================================

/*
================
FS_Sandbox_IsExecutable
================
Check if filename is executable
================
*/
qboolean FS_Sandbox_IsExecutable(const char *filename) {
	const char *ext;
	int len;
	
	if (!filename) {
		return qfalse;
	}
	
	len = strlen(filename);
	if (len < 3) {
		return qfalse;
	}
	
	ext = filename + len;
	
	// Check common executable extensions
	while (ext > filename && *ext != '.') {
		ext--;
	}
	
	if (*ext == '.') {
		ext++;
		if (!Q_stricmp(ext, "exe") ||
		    !Q_stricmp(ext, "dll") ||
		    !Q_stricmp(ext, "so") ||
		    !Q_stricmp(ext, "dylib") ||
		    !Q_stricmp(ext, "sh") ||
		    !Q_stricmp(ext, "bat") ||
		    !Q_stricmp(ext, "cmd") ||
		    !Q_stricmp(ext, "com")) {
			return qtrue;
		}
	}
	
	return qfalse;
}

/*
================
FS_Sandbox_CheckPath
================
Check if path matches sandbox rules
================
*/
qboolean FS_Sandbox_CheckPath(const char *qpath, const fsSandboxRules_t *rules) {
	int i;
	
	if (!qpath || !*qpath) {
		return qfalse;
	}
	
	if (!rules) {
		return qtrue;  // No rules = allow
	}
	
	// Check executable files
	if (FS_Sandbox_IsExecutable(qpath) && !rules->allowExecutables) {
		return qfalse;
	}
	
	// Check config files
	if (Q_stricmpn(qpath, "config", 6) == 0 || 
	    Q_stricmpn(qpath, ".cfg", 4) == 0 ||
	    strstr(qpath, ".cfg")) {
		if (!rules->allowConfig) {
			return qfalse;
		}
	}
	
	// Check save files
	if (Q_stricmpn(qpath, "save", 4) == 0 ||
	    Q_stricmpn(qpath, "saves", 5) == 0) {
		if (!rules->allowSaves) {
			return qfalse;
		}
	}
	
	// Check whitelist paths
	if (rules->numAllowedPaths > 0) {
		qboolean whitelisted = qfalse;
		for (i = 0; i < rules->numAllowedPaths; i++) {
			if (Q_stricmpn(rules->allowedPaths[i], qpath, strlen(rules->allowedPaths[i])) == 0) {
				whitelisted = qtrue;
				break;
			}
		}
		if (!whitelisted) {
			return qfalse;
		}
	}
	
	return qtrue;
}

/*
================
FS_Sandbox_ValidateOperation
================
Apply sandbox rules to file operation
================
*/
qboolean FS_Sandbox_ValidateOperation(const char *qpath, fsMount_t *mount, 
                                      qboolean isWrite) {
	fsSandboxRules_t *rules;
	
	if (!qpath || !*qpath) {
		return qfalse;
	}
	
	// If sandboxing is disabled globally, allow everything
	if (!fs_mountTable.sandboxEnabled) {
		return qtrue;
	}
	
	// Use mount-specific rules if available, otherwise global rules
	if (mount) {
		rules = &mount->sandbox;
	} else {
		rules = &fs_mountTable.globalSandbox;
	}
	
	// Check path against rules
	if (!FS_Sandbox_CheckPath(qpath, rules)) {
		return qfalse;
	}
	
	// Additional write-specific checks
	if (isWrite && mount) {
		if (mount->writePolicy == FS_WRITE_DENY) {
			return qfalse;
		}
		
		if (mount->writePolicy == FS_WRITE_SANDBOX) {
			// Additional sandbox checks for writes
			// (already handled by FS_Sandbox_CheckPath)
		}
	}
	
	return qtrue;
}

/*
================
FS_Sandbox_InitDefaultRules
================
Initialize default sandbox rules
================
*/
void FS_Sandbox_InitDefaultRules(fsSandboxRules_t *rules) {
	if (!rules) {
		return;
	}
	
	Com_Memset(rules, 0, sizeof(*rules));
	
	// Default: allow config and saves, deny executables
	rules->allowConfig = qtrue;
	rules->allowSaves = qtrue;
	rules->allowExecutables = qfalse;
	rules->numAllowedPaths = 0;
}

/*
================
FS_Sandbox_InitModRules
================
Initialize mod-specific sandbox rules (more restrictive)
================
*/
void FS_Sandbox_InitModRules(fsSandboxRules_t *rules) {
	if (!rules) {
		return;
	}
	
	Com_Memset(rules, 0, sizeof(*rules));
	
	// Mods: very restrictive
	rules->allowConfig = qfalse;
	rules->allowSaves = qfalse;
	rules->allowExecutables = qfalse;
	
	// Allow only specific paths
	rules->numAllowedPaths = 3;
	Q_strncpyz(rules->allowedPaths[0], "maps/", sizeof(rules->allowedPaths[0]));
	Q_strncpyz(rules->allowedPaths[1], "textures/", sizeof(rules->allowedPaths[1]));
	Q_strncpyz(rules->allowedPaths[2], "scripts/", sizeof(rules->allowedPaths[2]));
}

/*
================
FS_Sandbox_SetEnabled
================
Enable/disable global sandboxing
================
*/
void FS_Sandbox_SetEnabled(qboolean enabled) {
	fs_mountTable.sandboxEnabled = enabled;
}

// ============================================================================
// Mod Management API
// ============================================================================

/*
================
FS_Mod_Mount
================
Mount a mod (PAK or directory)
================
*/
qboolean FS_Mod_Mount(const char *modName, const char *path, 
                      fsMountPriority_t priority) {
	fsMount_t *mount;
	fsMountType_t type;
	pack_t *pak;
	directory_t *dir;
	char mountPoint[MAX_QPATH];
	int path_len, dir_len, len;
	
	if (!modName || !*modName || !path || !*path) {
		return qfalse;
	}
	
	if (!fs_mountTableInitialized) {
		FS_MountTable_Init();
	}
	
	// Check if mod already mounted
	Com_sprintf(mountPoint, sizeof(mountPoint), "mods/%s", modName);
	if (FS_Mount_Find(mountPoint)) {
		return qfalse;
	}
	
	// Determine mount type
	type = FS_IsPakFile(path) ? FS_MOUNT_PAK : FS_MOUNT_DIR;
	
	// Create mount
	mount = FS_Mount_Create(mountPoint, type, priority);
	if (!mount) {
		return qfalse;
	}
	
	Q_strncpyz(mount->displayName, modName, sizeof(mount->displayName));
	
	// Load backend
	if (type == FS_MOUNT_PAK) {
		// Load PAK file
		pak = FS_LoadZipFile(path);
		if (!pak) {
			FS_Mount_Destroy(mount);
			return qfalse;
		}
		mount->backend.pak = pak;
		mount->checksum = pak->checksum;
		mount->writePolicy = FS_WRITE_DENY;  // PAK files are read-only
	} else {
		// Create directory mount
		path_len = (int)strlen(path) + 1;
		path_len = PAD(path_len, sizeof(int));
		dir_len = (int)strlen(modName) + 1;
		dir_len = PAD(dir_len, sizeof(int));
		len = sizeof(*dir) + path_len + dir_len;
		
		dir = Z_TagMalloc(len, TAG_SEARCH_PATH);
		if (!dir) {
			FS_Mount_Destroy(mount);
			return qfalse;
		}
		
		Com_Memset(dir, 0, len);
		dir->path = (char *)(dir + 1);
		dir->gamedir = (char *)(dir->path + path_len);
		
		strcpy(dir->path, path);
		strcpy(dir->gamedir, modName);
		
		mount->backend.dir = dir;
		mount->writePolicy = FS_WRITE_SANDBOX;  // Mods are sandboxed by default
	}
	
	// Apply mod-specific sandbox rules
	FS_Sandbox_InitModRules(&mount->sandbox);
	
	// Add to mount table
	if (!FS_Mount_Add(mount)) {
		if (mount->backend.dir) {
			Z_Free(mount->backend.dir);
		}
		FS_Mount_Destroy(mount);
		return qfalse;
	}
	
	return qtrue;
}

/*
================
FS_Mod_Unmount
================
Unmount a mod
================
*/
qboolean FS_Mod_Unmount(const char *modName) {
	char mountPoint[MAX_QPATH];
	fsMount_t *mount;
	
	if (!modName || !*modName) {
		return qfalse;
	}
	
	Com_sprintf(mountPoint, sizeof(mountPoint), "mods/%s", modName);
	
	mount = FS_Mount_Find(mountPoint);
	if (!mount) {
		return qfalse;
	}
	
	// Clean up backend
	if (mount->backend.dir) {
		Z_Free(mount->backend.dir);
		mount->backend.dir = NULL;
	}
	// Note: PAK cleanup handled by legacy system
	
	// Remove from mount table
	if (!FS_Mount_Remove(mountPoint)) {
		return qfalse;
	}
	
	// Destroy mount
	FS_Mount_Destroy(mount);
	
	return qtrue;
}

/*
================
FS_Mod_ListMounted
================
List mounted mods
================
*/
int FS_Mod_ListMounted(char *buffer, int bufferSize) {
	fsMount_t *mount;
	int len = 0;
	int written;
	
	if (!buffer || bufferSize <= 0) {
		return 0;
	}
	
	buffer[0] = '\0';
	
	for (mount = fs_mountTable.mounts; mount; mount = mount->next) {
		if (Q_stricmpn(mount->mountPoint, "mods/", 5) != 0) {
			continue;
		}
		
		if (len > 0 && len < bufferSize - 1) {
			buffer[len++] = ' ';
		}
		
		written = Com_sprintf(buffer + len, bufferSize - len, "%s", mount->displayName);
		if (written < 0) {
			break;
		}
		len += written;
		
		if (len >= bufferSize - 1) {
			break;
		}
	}
	
	buffer[len] = '\0';
	return len;
}

/*
================
FS_Mod_GetInfo
================
Get mod info
================
*/
qboolean FS_Mod_GetInfo(const char *modName, char *path, int pathSize,
                        fsMountPriority_t *priority, qboolean *enabled) {
	char mountPoint[MAX_QPATH];
	fsMount_t *mount;
	
	if (!modName || !*modName) {
		return qfalse;
	}
	
	Com_sprintf(mountPoint, sizeof(mountPoint), "mods/%s", modName);
	
	mount = FS_Mount_Find(mountPoint);
	if (!mount) {
		return qfalse;
	}
	
	if (path && pathSize > 0) {
		if (mount->backend.dir) {
			Com_sprintf(path, pathSize, "%s/%s", mount->backend.dir->path, mount->backend.dir->gamedir);
		} else {
			path[0] = '\0';
		}
	}
	
	if (priority) {
		*priority = mount->priority;
	}
	
	if (enabled) {
		*enabled = mount->enabled;
	}
	
	return qtrue;
}

// ============================================================================
// Console Commands
// ============================================================================

/*
================
FS_MountList_f
================
List all mounts
================
*/
static void FS_MountList_f(void) {
	fsMount_t *mount;
	int count = 0;
	
	Com_Printf("Mounted filesystems:\n");
	Com_Printf("===================\n");
	
	for (mount = fs_mountTable.mounts; mount; mount = mount->next) {
		const char *typeStr = mount->type == FS_MOUNT_PAK ? "PAK" : 
		                      mount->type == FS_MOUNT_DIR ? "DIR" : "VIRTUAL";
		const char *writeStr = mount->writePolicy == FS_WRITE_DENY ? "read-only" :
		                      mount->writePolicy == FS_WRITE_ALLOW ? "read-write" : "sandboxed";
		
		Com_Printf("  [%d] %s (%s) priority=%d %s %s\n",
		           count++,
		           mount->mountPoint,
		           typeStr,
		           mount->priority,
		           mount->enabled ? "enabled" : "disabled",
		           writeStr);
		
		if (mount->displayName[0]) {
			Com_Printf("      Display: %s\n", mount->displayName);
		}
		if (mount->accessCount > 0) {
			Com_Printf("      Stats: %u accesses, %u hits\n", 
			           mount->accessCount, mount->hitCount);
		}
	}
	
	Com_Printf("\nTotal mounts: %d\n", fs_mountTable.numMounts);
	if (fs_mountTable.writeMount) {
		Com_Printf("Write mount: %s\n", fs_mountTable.writeMount->mountPoint);
	} else {
		Com_Printf("Write mount: (none)\n");
	}
}

/*
================
FS_ModMount_f
================
Mount a mod
================
*/
static void FS_ModMount_f(void) {
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: fs_mod_mount <name> <path> [priority]\n");
		Com_Printf("  name: Mod name (e.g., 'mymod')\n");
		Com_Printf("  path: Path to mod directory or PAK file\n");
		Com_Printf("  priority: Optional priority (default: %d)\n", FS_PRIORITY_MOD);
		return;
	}
	
	const char *name = Cmd_Argv(1);
	const char *path = Cmd_Argv(2);
	fsMountPriority_t priority = FS_PRIORITY_MOD;
	
	if (Cmd_Argc() >= 4) {
		int prio = atoi(Cmd_Argv(3));
		if (prio >= FS_PRIORITY_FALLBACK && prio <= FS_PRIORITY_SYSTEM) {
			priority = (fsMountPriority_t)prio;
		} else {
			Com_Printf("Invalid priority. Using default %d\n", FS_PRIORITY_MOD);
		}
	}
	
	if (FS_Mod_Mount(name, path, priority)) {
		Com_Printf("Mod '%s' mounted successfully\n", name);
	} else {
		Com_Printf("Failed to mount mod '%s'\n", name);
	}
}

/*
================
FS_ModUnmount_f
================
Unmount a mod
================
*/
static void FS_ModUnmount_f(void) {
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: fs_mod_unmount <name>\n");
		return;
	}
	
	const char *name = Cmd_Argv(1);
	
	if (FS_Mod_Unmount(name)) {
		Com_Printf("Mod '%s' unmounted successfully\n", name);
	} else {
		Com_Printf("Failed to unmount mod '%s' (not found?)\n", name);
	}
}

/*
================
FS_MountInfo_f
================
Show mount info
================
*/
static void FS_MountInfo_f(void) {
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: fs_mount_info <mount_point>\n");
		return;
	}
	
	const char *mountPoint = Cmd_Argv(1);
	fsMount_t *mount = FS_Mount_Find(mountPoint);
	
	if (!mount) {
		Com_Printf("Mount '%s' not found\n", mountPoint);
		return;
	}
	
	Com_Printf("Mount Info: %s\n", mount->mountPoint);
	Com_Printf("===================\n");
	Com_Printf("Type: %s\n", mount->type == FS_MOUNT_PAK ? "PAK" : 
	                        mount->type == FS_MOUNT_DIR ? "DIR" : "VIRTUAL");
	Com_Printf("Priority: %d\n", mount->priority);
	Com_Printf("Enabled: %s\n", mount->enabled ? "yes" : "no");
	Com_Printf("Write Policy: %s\n", 
	           mount->writePolicy == FS_WRITE_DENY ? "deny" :
	           mount->writePolicy == FS_WRITE_ALLOW ? "allow" : "sandbox");
	
	if (mount->displayName[0]) {
		Com_Printf("Display Name: %s\n", mount->displayName);
	}
	
	if (mount->backend.dir) {
		Com_Printf("Path: %s\n", mount->backend.dir->path);
		Com_Printf("Game Dir: %s\n", mount->backend.dir->gamedir);
	} else if (mount->backend.pak) {
		Com_Printf("PAK File: %s\n", mount->backend.pak->pakFilename);
		Com_Printf("Files: %d\n", mount->backend.pak->numfiles);
		Com_Printf("Checksum: %u\n", mount->checksum);
	}
	
	Com_Printf("Access Count: %u\n", mount->accessCount);
	Com_Printf("Hit Count: %u\n", mount->hitCount);
	
	Com_Printf("Sandbox Rules:\n");
	Com_Printf("  Allow Executables: %s\n", mount->sandbox.allowExecutables ? "yes" : "no");
	Com_Printf("  Allow Config: %s\n", mount->sandbox.allowConfig ? "yes" : "no");
	Com_Printf("  Allow Saves: %s\n", mount->sandbox.allowSaves ? "yes" : "no");
	Com_Printf("  Allowed Paths: %d\n", mount->sandbox.numAllowedPaths);
}

/*
================
FS_Mount_RegisterCommands
================
Register console commands
================
*/
void FS_Mount_RegisterCommands(void) {
	Cmd_AddCommand("fs_mount_list", FS_MountList_f);
	Cmd_AddCommand("fs_mod_mount", FS_ModMount_f);
	Cmd_AddCommand("fs_mod_unmount", FS_ModUnmount_f);
	Cmd_AddCommand("fs_mount_info", FS_MountInfo_f);
}

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
void FS_MigrateLegacySearchPaths(void) {
	searchpath_t *sp;
	fsMount_t *mount;
	fsMountPriority_t priority;
	int count = 0;
	
	if (!fs_searchpaths) {
		return;
	}
	
	// Count searchpaths to assign priorities
	for (sp = fs_searchpaths; sp; sp = sp->next) {
		count++;
	}
	
	// Migrate in reverse order (to maintain search order)
	// Higher priority = searched first, so we assign priorities in reverse
	priority = FS_PRIORITY_GAME;
	
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
			continue;  // Skip invalid entries
		}
		
		// Create mount
		mount = FS_Mount_Create(mountPoint, type, priority);
		if (!mount) {
			continue;
		}
		
		// Copy backend data
		if (type == FS_MOUNT_PAK) {
			mount->backend.pak = sp->pack;
			mount->checksum = sp->pack->checksum;
			mount->writePolicy = FS_WRITE_DENY;  // PAK files are read-only
		} else {
			// For directories, we need to allocate new directory_t
			// since searchpath_t uses inline allocation
			directory_t *dir;
			int path_len, dir_len, len;
			
			path_len = (int)strlen(sp->dir->path) + 1;
			path_len = PAD(path_len, sizeof(int));
			dir_len = (int)strlen(sp->dir->gamedir) + 1;
			dir_len = PAD(dir_len, sizeof(int));
			len = sizeof(*dir) + path_len + dir_len;
			
			dir = Z_TagMalloc(len, TAG_SEARCH_PATH);
			if (!dir) {
				FS_Mount_Destroy(mount);
				continue;
			}
			
			Com_Memset(dir, 0, len);
			dir->path = (char *)(dir + 1);
			dir->gamedir = (char *)(dir->path + path_len);
			
			strcpy(dir->path, sp->dir->path);
			strcpy(dir->gamedir, sp->dir->gamedir);
			
			mount->backend.dir = dir;
			
			// Set write policy based on legacy policy
			if (sp->policy == DIR_DENY) {
				mount->writePolicy = FS_WRITE_DENY;
			} else if (sp->policy == DIR_STATIC) {
				mount->writePolicy = FS_WRITE_DENY;  // Static = read-only
			} else {
				mount->writePolicy = FS_WRITE_SANDBOX;  // Allow = sandboxed
			}
		}
		
		// Set display name
		if (sp->dir) {
			Com_sprintf(mount->displayName, sizeof(mount->displayName), 
			           "%s/%s", sp->dir->path, sp->dir->gamedir);
		} else if (sp->pack) {
			Q_strncpyz(mount->displayName, sp->pack->pakBasename, sizeof(mount->displayName));
		}
		
		// Add to mount table
		if (!FS_Mount_Add(mount)) {
			if (mount->backend.dir) {
				Z_Free(mount->backend.dir);
			}
			FS_Mount_Destroy(mount);
			continue;
		}
		
		// Lower priority for next mount (so earlier mounts have higher priority)
		if (priority > FS_PRIORITY_FALLBACK) {
			priority--;
		}
	}
	
	if (fs_mountTable.numMounts > 0) {
		Com_Printf("Migrated %d legacy searchpaths to mount table\n", fs_mountTable.numMounts);
	}
}
