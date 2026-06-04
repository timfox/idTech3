#ifndef VK_UPSCALE_H
#define VK_UPSCALE_H

#include "tr_local.h"

void R_Upscale_Init( void );
qboolean R_Upscale_UseFsr2( void );
void R_Upscale_NoteJitter( float *jitterX, float *jitterY );

#endif
