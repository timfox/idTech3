#ifndef VK_UPSCALE_H
#define VK_UPSCALE_H

#include "tr_local.h"

void R_Upscale_Init( void );
void R_Upscale_Shutdown( void );

/* 1 = spatial renderScale path, 2 = temporal upsample (Halton + TAA). */
qboolean R_Upscale_WantSpatial( void );
qboolean R_Upscale_WantTemporal( void );

/* Halton sub-pixel jitter in render-pixel units (0 when temporal off). */
void R_Upscale_NoteJitter( float *jitterX, float *jitterY );
void R_Upscale_GetJitter( float *jitterX, float *jitterY );
void R_Upscale_GetPrevJitter( float *jitterX, float *jitterY );

/* Apply current-frame jitter to a projection matrix (NDC offset via [8]/[9]). */
void R_Upscale_ApplyProjectionJitter( float projectionMatrix[16] );

/* Auto-enable r_renderScale + 75% internal size when r_upscale >= 1 and scale was off. */
void R_Upscale_ApplyRenderScaleDefaults( void );

float R_Upscale_GetSharpness( void );

#endif
