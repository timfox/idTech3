/*
===========================================================================
Syscall Registry Implementation
Centralized management of syscall numbers and extensions
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "syscall_registry.h"
#include "vm_local.h"

// Forward declarations for syscall enums
#include "../game/g_public.h"
#ifndef USE_DEDICATED
#include "../cgame/cg_public.h"
#include "../ui/ui_public.h"
#endif

// Extension registry per VM module
static syscall_extension_t g_game_extensions[] = {
    { "SVF_SELF_PORTAL2_Q3E", SVF_SELF_PORTAL2, SYSCALL_EXT_API_VERSION, "Portal rendering flag extension" },
    { "trap_Cvar_SetDescription_Q3E", G_CVAR_SETDESCRIPTION, SYSCALL_EXT_API_VERSION, "CVar description extension" },
    { NULL, -1, 0, NULL }
};

#ifndef USE_DEDICATED
static syscall_extension_t g_cgame_extensions[] = {
    { "trap_R_AddRefEntityToScene2", CG_R_ADDREFENTITYTOSCENE2, SYSCALL_EXT_API_VERSION, "Extended ref entity rendering" },
    { "trap_R_ForceFixedDLights", CG_R_FORCEFIXEDDLIGHTS, SYSCALL_EXT_API_VERSION, "Force fixed dlights" },
    { "trap_R_AddLinearLightToScene_Q3E", CG_R_ADDLINEARLIGHTTOSCENE, SYSCALL_EXT_API_VERSION, "Linear light extension" },
    { "trap_IsRecordingDemo", CG_IS_RECORDING_DEMO, SYSCALL_EXT_API_VERSION, "Demo recording check" },
    { "trap_Cvar_SetDescription_Q3E", CG_CVAR_SETDESCRIPTION, SYSCALL_EXT_API_VERSION, "CVar description extension" },
    { NULL, -1, 0, NULL }
};

static syscall_extension_t g_ui_extensions[] = {
    { "trap_R_AddRefEntityToScene2", UI_R_ADDREFENTITYTOSCENE2, SYSCALL_EXT_API_VERSION, "Extended ref entity rendering" },
    { "trap_R_AddLinearLightToScene_Q3E", UI_R_ADDLINEARLIGHTTOSCENE, SYSCALL_EXT_API_VERSION, "Linear light extension" },
    { "trap_Cvar_SetDescription_Q3E", UI_CVAR_SETDESCRIPTION, SYSCALL_EXT_API_VERSION, "CVar description extension" },
    { NULL, -1, 0, NULL }
};
#endif

// Get extension list for a VM module
static syscall_extension_t *Syscall_GetExtensionList( vmIndex_t vm_index )
{
    switch ( vm_index ) {
        case VM_GAME:
            return g_game_extensions;
#ifndef USE_DEDICATED
        case VM_CGAME:
            return g_cgame_extensions;
        case VM_UI:
            return g_ui_extensions;
#endif
        default:
            return NULL;
    }
}

/*
================
Syscall_ExtensionAvailable
Check if an extension syscall is available
================
*/
qboolean Syscall_ExtensionAvailable( const char *key_name, vmIndex_t vm_index )
{
    syscall_extension_t *ext_list = Syscall_GetExtensionList( vm_index );
    
    if ( !ext_list || !key_name ) {
        return qfalse;
    }
    
    for ( int i = 0; ext_list[i].key_name != NULL; i++ ) {
        if ( !Q_stricmp( ext_list[i].key_name, key_name ) ) {
            return qtrue;
        }
    }
    
    return qfalse;
}

/*
================
Syscall_GetExtensionNumber
Get syscall number for an extension key
================
*/
int Syscall_GetExtensionNumber( const char *key_name, vmIndex_t vm_index )
{
    syscall_extension_t *ext_list = Syscall_GetExtensionList( vm_index );
    
    if ( !ext_list || !key_name ) {
        return -1;
    }
    
    for ( int i = 0; ext_list[i].key_name != NULL; i++ ) {
        if ( !Q_stricmp( ext_list[i].key_name, key_name ) ) {
            return ext_list[i].syscall_num;
        }
    }
    
    return -1;
}

/*
================
Syscall_GetExtensionInfo
Get extension info for a key (for documentation/debugging)
================
*/
const syscall_extension_t *Syscall_GetExtensionInfo( const char *key_name, vmIndex_t vm_index )
{
    syscall_extension_t *ext_list = Syscall_GetExtensionList( vm_index );
    
    if ( !ext_list || !key_name ) {
        return NULL;
    }
    
    for ( int i = 0; ext_list[i].key_name != NULL; i++ ) {
        if ( !Q_stricmp( ext_list[i].key_name, key_name ) ) {
            return &ext_list[i];
        }
    }
    
    return NULL;
}

/*
================
Syscall_GetValue
Unified GetValue handler for all VM modules
Replaces module-specific SV_GetValue, CL_GetValue, etc.
================
*/
qboolean Syscall_GetValue( vmIndex_t vm_index, char *value, int valueSize, const char *key )
{
    syscall_extension_t *ext_list = Syscall_GetExtensionList( vm_index );
    
    if ( !ext_list || !key || !value ) {
        return qfalse;
    }
    
    // Search extension list
    for ( int i = 0; ext_list[i].key_name != NULL; i++ ) {
        if ( !Q_stricmp( ext_list[i].key_name, key ) ) {
            Com_sprintf( value, valueSize, "%i", ext_list[i].syscall_num );
            return qtrue;
        }
    }
    
    return qfalse;
}

