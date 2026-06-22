/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Menu background video playback.
Plays a looping video as the main menu background instead of
a static image. Supports ROQ, and modern codecs via cl_cin_modern.
===========================================================================
*/

#ifndef CL_MENUVIDEO_H
#define CL_MENUVIDEO_H

#include "../../qcommon/q_shared.h"

void    MenuVideo_Init( void );
void    MenuVideo_Shutdown( void );
void    MenuVideo_Frame( void );
void    MenuVideo_Draw( void );
qboolean MenuVideo_IsPlaying( void );

#endif /* CL_MENUVIDEO_H */
