/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Client-side emoji rendering.
Draws emoji glyphs inline with text using a color atlas texture.
Supports both UTF-8 codepoints and :shortcode: syntax.
===========================================================================
*/

#ifndef CL_EMOJI_H
#define CL_EMOJI_H

#include "q_shared.h"

void    CL_Emoji_Init( void );
void    CL_Emoji_Shutdown( void );

qboolean CL_Emoji_DrawChar( int x, int y, float w, float h, uint32_t codepoint );

int     CL_Emoji_ExpandShortcodes( const char *in, char *out, int outSize );

qboolean CL_Emoji_IsEnabled( void );

#endif /* CL_EMOJI_H */
