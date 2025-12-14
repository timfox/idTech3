/*
===========================================================================
VM Hot Reloading System Header
===========================================================================
*/

#ifndef __VM_HOT_RELOAD_H__
#define __VM_HOT_RELOAD_H__

// Initialize the hot reload system
void VM_HotReloadInit(void);

// Check for VM file changes and perform hot reload if needed
void VM_CheckHotReload(void);

// Register console commands for manual VM reloading
void VM_HotReloadRegisterCommands(void);

// Shutdown the hot reload system
void VM_HotReloadShutdown(void);

#endif // __VM_HOT_RELOAD_H__