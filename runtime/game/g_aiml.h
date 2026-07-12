/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

AIML 3.0 draft–spec–oriented interpreter (see github.com/timfox/aiml-3.0-spec):
Core tier matching, JSON/XML category packs, maps (§8.7), predicates,
<that>/<topic>, <srai>, <random>, <condition>, <id/> (§8.12), ASCII
<uppercase>/<lowercase> (§8.9 optional).
===========================================================================
*/

#ifndef G_AIML_H
#define G_AIML_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AIML_MAX_CATEGORIES     4096
#define AIML_MAX_BOTS           8
#define AIML_MAX_PROPERTIES     64
#define AIML_MAX_USER_VARS      32
#define AIML_MAX_PATTERN_LEN    256
#define AIML_MAX_RESPONSE_LEN   1024
#define AIML_MAX_RANDOM_ITEMS   16
#define AIML_MAX_SRAI_DEPTH     8
#define AIML_MAX_STAR           9
#define AIML_MAX_MAP_ENTRIES    256

typedef int aimlBotHandle_t;

void    AIML_Init( void );
void    AIML_Shutdown( void );

aimlBotHandle_t AIML_CreateBot( const char *name );
void    AIML_DestroyBot( aimlBotHandle_t bot );
void    AIML_SetBotProperty( aimlBotHandle_t bot, const char *key, const char *value );
const char *AIML_GetBotProperty( aimlBotHandle_t bot, const char *key );

qboolean AIML_LoadFile( aimlBotHandle_t bot, const char *filename );
int     AIML_GetCategoryCount( aimlBotHandle_t bot );

const char *AIML_GetResponse( aimlBotHandle_t bot, const char *userId, const char *input );

void    AIML_SetUserVar( aimlBotHandle_t bot, const char *userId, const char *key, const char *value );
const char *AIML_GetUserVar( aimlBotHandle_t bot, const char *userId, const char *key );
void    AIML_ResetUser( aimlBotHandle_t bot, const char *userId );

#ifdef __cplusplus
}
#endif

#endif /* G_AIML_H */
