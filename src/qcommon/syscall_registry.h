/*
===========================================================================
Syscall Registry System
Centralized management of syscall numbers and extensions
===========================================================================
*/

#ifndef __SYSCALL_REGISTRY_H__
#define __SYSCALL_REGISTRY_H__

#include "q_shared.h"

// Syscall API versions for compatibility checking
#define SYSCALL_API_VERSION_GAME    8
#define SYSCALL_API_VERSION_CGAME   4
#define SYSCALL_API_VERSION_UI      6

// Extension API version (incremented when new extensions are added)
#define SYSCALL_EXT_API_VERSION     1

// Syscall categories for organization
typedef enum {
    SYSCALL_CATEGORY_STANDARD,      // Standard Quake 3 syscalls (always available)
    SYSCALL_CATEGORY_EXTENSION,     // Q3E extension syscalls (optional)
    SYSCALL_CATEGORY_LEGACY,        // Deprecated syscalls (maintained for compatibility)
} syscall_category_t;

// Syscall metadata for documentation and validation
typedef struct {
    int syscall_num;                // Syscall number
    const char *name;               // Syscall name (e.g., "G_PRINT")
    syscall_category_t category;    // Category
    int api_version;                // Minimum API version required
    const char *description;        // Human-readable description
    qboolean deprecated;            // Marked as deprecated
    const char *deprecated_since;   // Version when deprecated
} syscall_info_t;

// Extension syscall registration
typedef struct {
    const char *key_name;           // Extension key (e.g., "trap_Cvar_SetDescription_Q3E")
    int syscall_num;                // Syscall number
    int api_version;                // Extension API version
    const char *description;        // Description
} syscall_extension_t;

// Function prototypes
const syscall_info_t *Syscall_GetInfo( int syscall_num, vmIndex_t vm_index );
qboolean Syscall_IsAvailable( int syscall_num, vmIndex_t vm_index, int api_version );
const char *Syscall_GetName( int syscall_num, vmIndex_t vm_index );
syscall_category_t Syscall_GetCategory( int syscall_num, vmIndex_t vm_index );

// Extension system helpers
qboolean Syscall_ExtensionAvailable( const char *key_name, vmIndex_t vm_index );
int Syscall_GetExtensionNumber( const char *key_name, vmIndex_t vm_index );
const syscall_extension_t *Syscall_GetExtensionInfo( const char *key_name, vmIndex_t vm_index );

// Unified GetValue handler for all VM modules
qboolean Syscall_GetValue( vmIndex_t vm_index, char *value, int valueSize, const char *key );

#endif // __SYSCALL_REGISTRY_H__

