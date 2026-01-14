/*
===========================================================================
VM Hot Reloading System
===========================================================================
*/

#include "qcommon.h"
#include "vm_local.h"
#include "vm_hot_reload.h"

// Forward declarations for VM table access
extern struct vm_s vmTable[VM_COUNT];
extern const char *vmName[VM_COUNT];

// Hot reload configuration
cvar_t *vm_hotReload;
cvar_t *vm_hotReloadInterval;
cvar_t *vm_hotReloadVerbose;

// File checksums for QVM files (simple approach)
static unsigned int vmFileChecksums[VM_COUNT];
static qboolean vmHotReloadInitialized = qfalse;

/*
================
VM_HotReloadInit
================
*/
void VM_HotReloadInit(void) {
	if (vmHotReloadInitialized) {
		return;
	}

	// Register cvars
	vm_hotReload = Cvar_Get("vm_hotReload", "1", CVAR_ARCHIVE);
	vm_hotReloadInterval = Cvar_Get("vm_hotReloadInterval", "500", CVAR_ARCHIVE);
	vm_hotReloadVerbose = Cvar_Get("vm_hotReloadVerbose", "0", CVAR_ARCHIVE);

	Cvar_SetDescription(vm_hotReload, "Enable automatic hot reloading of QVM modules when source files change");
	Cvar_SetDescription(vm_hotReloadInterval, "Interval in milliseconds to check for QVM file changes");
	Cvar_SetDescription(vm_hotReloadVerbose, "Enable verbose logging for hot reload operations");

	// Initialize checksums
	Com_Memset(vmFileChecksums, 0, sizeof(vmFileChecksums));

	vmHotReloadInitialized = qtrue;

	if (vm_hotReloadVerbose && vm_hotReloadVerbose->integer) {
		Com_Printf("VM hot reload system initialized\n");
	}
}

/*
================
VM_GetQVMFilePath
================
*/
static void VM_GetQVMFilePath(vmIndex_t index, char *path, size_t pathSize) {
	const char *moduleName = vmName[index];
	Com_sprintf(path, pathSize, "vm/%s.qvm", moduleName);
}

/*
================
VM_CalculateFileChecksum
================
*/
static unsigned int VM_CalculateFileChecksum(const char *data, int length) {
	// Simple checksum calculation (FNV-1a like)
	unsigned int hash = 2166136261u;
	for (int i = 0; i < length; i++) {
		hash ^= (unsigned char)data[i];
		hash *= 16777619u;
	}
	return hash;
}

/*
================
VM_CheckFileModified
================
*/
static qboolean VM_CheckFileModified(vmIndex_t index) {
	char qvmPath[MAX_QPATH];
	char *buffer;
	int len;

	VM_GetQVMFilePath(index, qvmPath, sizeof(qvmPath));

	// Try to open the file to check if it exists and read contents
	len = FS_ReadFile(qvmPath, (void **)&buffer);
	if (len < 0 || !buffer) {
		// File doesn't exist or couldn't be read
		return qfalse;
	}

	// Calculate checksum of file contents
	unsigned int checksum = VM_CalculateFileChecksum(buffer, len);

	// Free the file data
	FS_FreeFile(buffer);

	// Check if checksum has changed
	if (vmFileChecksums[index] != checksum && vmFileChecksums[index] != 0) {
		vmFileChecksums[index] = checksum;
		return qtrue;
	}

	// Update checksum if this is the first time checking
	if (vmFileChecksums[index] == 0) {
		vmFileChecksums[index] = checksum;
	}

	return qfalse;
}

/*
================
VM_PerformHotReload
================
*/
static void VM_PerformHotReload(vmIndex_t index) {
	vm_t *vm = &vmTable[index];

	if (!vm->name) {
		// VM not loaded
		return;
	}

	if (vm_hotReloadVerbose && vm_hotReloadVerbose->integer) {
		Com_Printf("Hot reloading VM: %s\n", vmName[index]);
	}

	// Attempt to restart the VM
	vm_t *newVm = VM_Restart(vm);

	if (newVm) {
		// Update the VM table entry
		vmTable[index] = *newVm;

		if (vm_hotReloadVerbose && vm_hotReloadVerbose->integer) {
			Com_Printf("Successfully hot reloaded VM: %s\n", vmName[index]);
		}

		// Notify other systems that may need to update references
		// This could include:
		// - Game state reset notifications
		// - Entity system updates
		// - UI refresh for changed game logic

		Com_Printf("Hot reloaded %s module\n", vmName[index]);
	} else {
		Com_Printf("Failed to hot reload %s module\n", vmName[index]);
	}
}

/*
================
VM_CheckHotReload
================
*/
void VM_CheckHotReload(void) {
	static int lastCheckTime = 0;
	int currentTime;

	if (!vm_hotReload || !vm_hotReload->integer) {
		return;
	}

	currentTime = Sys_Milliseconds();

	// Throttle checks to avoid excessive file system operations
	if (currentTime - lastCheckTime < vm_hotReloadInterval->integer) {
		return;
	}

	lastCheckTime = currentTime;

	// Check each VM for file changes
	for (vmIndex_t i = 0; i < VM_COUNT; i++) {
		if (VM_CheckFileModified(i)) {
			// File was modified, perform hot reload
			VM_PerformHotReload(i);
		}
	}
}

/*
================
VM_ReloadVM_f

Console command to manually reload a VM
Usage: vm_reload <vm_name>
================
*/
static void VM_ReloadVM_f(void) {
		const char *argVmName = NULL;
	vmIndex_t index;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: vm_reload <vm_name>\n");
		Com_Printf("Available VMs: ");
		for (vmIndex_t i = 0; i < VM_COUNT; i++) {
			if (i > 0) Com_Printf(", ");
			Com_Printf("%s", vmName[i]);
		}
		Com_Printf("\n");
		return;
	}

		argVmName = Cmd_Argv(1);

	// Find VM by name
	index = VM_COUNT; // Invalid index
	for (vmIndex_t i = 0; i < VM_COUNT; i++) {
		if (Q_stricmp(vmName[i], argVmName) == 0) {
			index = i;
			break;
		}
	}

	if (index >= VM_COUNT) {
		Com_Printf("Unknown VM name: %s\n", argVmName);
		return;
	}

	Com_Printf("Manually reloading VM: %s\n", vmName[index]);
	VM_PerformHotReload(index);
}

/*
================
VM_ReloadAll_f

Console command to reload all VMs
Usage: vm_reload_all
================
*/
static void VM_ReloadAll_f(void) {
	Com_Printf("Reloading all VMs...\n");

	for (vmIndex_t i = 0; i < VM_COUNT; i++) {
		if (vmTable[i].name) {
			VM_PerformHotReload(i);
		}
	}

	Com_Printf("VM reload complete\n");
}

/*
================
VM_HotReloadStatus_f

Console command to show hot reload status
Usage: vm_hotreload_status
================
*/
static void VM_HotReloadStatus_f(void) {
	Com_Printf("VM Hot Reload Status:\n");
	Com_Printf("  Enabled: %s\n", vm_hotReload && vm_hotReload->integer ? "Yes" : "No");
	Com_Printf("  Check Interval: %d ms\n", vm_hotReloadInterval ? vm_hotReloadInterval->integer : 0);
	Com_Printf("  Verbose: %s\n", vm_hotReloadVerbose && vm_hotReloadVerbose->integer ? "Yes" : "No");

	Com_Printf("\nLoaded VMs:\n");
	for (vmIndex_t i = 0; i < VM_COUNT; i++) {
		if (vmTable[i].name) {
			char qvmPath[MAX_QPATH];
			VM_GetQVMFilePath(i, qvmPath, sizeof(qvmPath));
			Com_Printf("  %s (%s)\n", vmName[i], qvmPath);
		}
	}
}

/*
================
VM_HotReloadRegisterCommands
================
*/
void VM_HotReloadRegisterCommands(void) {
	Cmd_AddCommand("vm_reload", VM_ReloadVM_f);
	Cmd_AddCommand("vm_reload_all", VM_ReloadAll_f);
	Cmd_AddCommand("vm_hotreload_status", VM_HotReloadStatus_f);

	// TODO: Implement command completion for vm_reload command.
	// This would allow tab-completion of VM names (game, cgame, ui) when using
	// the vm_reload command, improving developer experience and reducing typos.
	// Cmd_SetCommandCompletionFunc("vm_reload", Cmd_CompleteVMName);
}

/*
================
VM_HotReloadShutdown
================
*/
void VM_HotReloadShutdown(void) {
	// Clean up any resources if needed
	vmHotReloadInitialized = qfalse;
}