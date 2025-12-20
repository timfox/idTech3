/*
===========================================================================
Copyright (C) 2024 id Tech 3

Client-side i18n wrapper functions.
The actual i18n implementation is in qcommon/i18n.c
===========================================================================
*/

#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "../common/i18n.h"
#include "client.h"

// Client-side i18n is handled by the common i18n system
// This file exists for potential client-specific extensions

void CL_I18n_Init(void);
void CL_I18n_Shutdown(void);

void CL_I18n_Init(void)
{
	// i18n is initialized in common.c
	// Sync legacy com_language to the new cl_language if present
	cvar_t *legacy = Cvar_Get("com_language", "", CVAR_ARCHIVE);
	if (legacy && legacy->string && legacy->string[0]) {
		Cvar_Set("cl_language", legacy->string);
	}

	// Ensure the active language is loaded on client bring-up
	{
		cvar_t *cl_language_cvar = Cvar_Get("cl_language", DEFAULT_LANGUAGE_CODE, CVAR_ARCHIVE);
		if (cl_language_cvar && cl_language_cvar->string && cl_language_cvar->string[0]) {
			CL_LoadLanguage(cl_language_cvar->string);
		}
	}
}

void CL_I18n_Shutdown(void)
{
	// i18n shutdown is handled in common.c
}

