/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

UTF-8 decoding/encoding utilities and emoji support.
===========================================================================
*/

#ifndef Q_UTF8_H
#define Q_UTF8_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define Q_UTF8_INVALID      0xFFFD
#define Q_EMOJI_MAX_ENTRIES  512
#define Q_EMOJI_ATLAS_SIZE   32

typedef struct {
	uint32_t    codepoint;
	int         atlasX;
	int         atlasY;
	const char  *shortcode;
} emojiEntry_t;

uint32_t    Q_UTF8_Decode( const char **pp );

int         Q_UTF8_Encode( uint32_t cp, char *out );

int         Q_UTF8_Width( const char *s );

int         Q_UTF8_Strlen( const char *s );

qboolean    Q_UTF8_IsEmoji( uint32_t codepoint );

const emojiEntry_t *Q_Emoji_Lookup( uint32_t codepoint );

void        Q_Emoji_Init( void );

int         Q_Emoji_Count( void );

const emojiEntry_t *Q_Emoji_GetByIndex( int index );

const emojiEntry_t *Q_Emoji_FindByShortcode( const char *shortcode );

#ifdef __cplusplus
}
#endif

#endif /* Q_UTF8_H */
