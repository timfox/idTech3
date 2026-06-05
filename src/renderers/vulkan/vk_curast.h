/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CuRast — software rasterization scaffold (arXiv:2604.21749).
See docs/CURAST.md.
===========================================================================
*/

#ifndef VK_CURAST_H
#define VK_CURAST_H

#include "tr_local.h"

void R_CuRast_Init( void );
void R_CuRast_Shutdown( void );

qboolean R_CuRast_Active( void );
qboolean R_CuRast_RenderFrame( void );

#endif /* VK_CURAST_H */
