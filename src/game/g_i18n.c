#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "../common/i18n.h"

// Game-side i18n is handled by the common i18n system
// This file exists for potential game-specific extensions

void G_I18n_Init(void)
{
	// i18n is initialized in common.c
	// Game module can load language-specific content here if needed
}

void G_I18n_Shutdown(void)
{
	// i18n shutdown is handled in common.c
}

