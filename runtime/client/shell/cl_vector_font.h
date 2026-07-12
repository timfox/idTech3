/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#ifndef CL_VECTOR_FONT_H
#define CL_VECTOR_FONT_H

#include "q_shared.h"

void        VectorFont_Init( void );
void        VectorFont_Reload( void );
qboolean    VectorFont_IsActive( void );
qboolean    VectorFont_DrawStringExt( int x, int y, float size, const char *string,
	const float *setColor, qboolean forceColor, qboolean noColorEscape,
	qboolean virtual640 );

#endif /* CL_VECTOR_FONT_H */
