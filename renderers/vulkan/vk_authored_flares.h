#ifndef VK_AUTHORED_FLARES_H
#define VK_AUTHORED_FLARES_H

#include "q_shared.h"

void R_AuthoredFlares_Init( void );
int R_AuthoredFlares_Load( const char *path );
int R_AuthoredFlares_Count( void );

/* Opaque lookup for authored defs (texture/size/intensity). */
qboolean R_AuthoredFlares_Get( const char *name, char *textureOut, int textureSize,
	float *sizeOut, float *intensityOut, vec3_t colorOut );

#endif
