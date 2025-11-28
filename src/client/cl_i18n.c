/*
===========================================================================
Copyright (C) 2024 id Tech 3

Client-side i18n wrapper functions.
The actual i18n implementation is in qcommon/i18n.c
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/i18n.h"
#include "client.h"

// Client-side i18n is handled by the common i18n system
// This file exists for potential client-specific extensions

void CL_I18n_Init(void);
void CL_I18n_Shutdown(void);

void CL_I18n_Init(void)
{
	// i18n is initialized in common.c
	// Load language from CVAR if set
	cvar_t *com_language = Cvar_Get("com_language", "en", CVAR_ARCHIVE);
	if (com_language && com_language->string && com_language->string[0]) {
		I18n_SetLanguage(com_language->string);
		I18n_LoadLanguage(com_language->string);
	}
}

void CL_I18n_Shutdown(void)
{
	// i18n shutdown is handled in common.c
}

