/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

AIML (Artificial Intelligence Markup Language) engine.
Parses standard AIML 1.0/2.0 files and provides pattern-matching
chatbot responses for NPC dialogue. Supports wildcards, random
responses, SRAI recursion, bot properties, and user variables.

Compatible with AIML files from ALICE, Pandorabots, and libAIML.
===========================================================================
*/

#ifndef G_AIML_H
#define G_AIML_H

#include "../qcommon/q_shared.h"

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
