#ifndef __FILES_VALIDATION_H__
#define __FILES_VALIDATION_H__

#include "q_shared.h"

// Prototypes for mod sandboxing helpers (used across modules)
qboolean Mod_ApplySandboxRestrictions( const char *modName );
void Mod_RemoveSandboxRestrictions( const char *modName );

#endif // __FILES_VALIDATION_H__
