/*
===========================================================================
Game Module OOP Bridge (C ABI)

C-callable surface for the C++ entity OOP/EnTT layer. All functions are
no-ops when USE_ENTT is not defined or the feature is disabled by cvar.
===========================================================================
*/

#ifndef __G_OOP_H__
#define __G_OOP_H__

#include "g_local.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize / shutdown
void G_OOP_Init(void);
void G_OOP_Shutdown(void);

// Feature toggle
qboolean G_OOP_Enabled(void);
int G_OOP_ActiveCount(void);

// Spawn hook: returns qtrue if the OOP path handled this classname
qboolean G_OOP_CallSpawn(gentity_t *ent, const char *classname);

// Per-frame update (msec delta from last frame)
void G_OOP_RunFrame(int msec);

// Cleanup when a gentity is freed
void G_OOP_OnFreeEntity(gentity_t *ent);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __G_OOP_H__

