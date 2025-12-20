// C-facing API for the AIML interpreter so C modules can consume it easily.
#pragma once

#include <stddef.h>

#include "../q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void		AIML_Init(void);
void		AIML_Shutdown(void);

qboolean	AIML_LoadFile(const char *path);
qboolean	AIML_LoadBuffer(const char *name, const char *buffer);

qboolean	AIML_Respond(const char *userId, const char *input, char *outBuffer, size_t outSize);

void		AIML_SetBotPredicate(const char *key, const char *value);
int			AIML_GetBotPredicate(const char *key, char *outBuffer, size_t outSize);

void		AIML_ResetSession(const char *userId);
void		AIML_ResetAllSessions(void);

#ifdef __cplusplus
}
#endif

