#ifndef VK_VT_H
#define VK_VT_H

#include "tr_local.h"

void R_VT_Init( void );
void R_VT_Shutdown( void );
qboolean R_VT_Active( void );
qboolean R_VT_IsSparse( void );

/* Load a page from raw RGBA8 into physical atlas / sparse bind. Returns slot or -1. */
int R_VT_LoadPageRGBA( const byte *rgba, int width, int height, const char *name );

/* Map virtual page id -> physical slot (-1 if missing). */
int R_VT_Lookup( int virtualPage );

image_t *R_VT_AtlasImage( void );
qhandle_t R_VT_AtlasShader( void );
qboolean R_VT_WantSample( void );

/* PiP atlas overlay when r_vtDebug 1 (2D pass). */
void R_VT_DebugDraw( void );

/* Feedback-driven residency (r_vtFeedback). */
void R_VT_Feedback_BeginFrame( void );
void R_VT_Feedback_RequestPage( int virtualPage );
void R_VT_Feedback_RequestUV( float u, float v );
void R_VT_Feedback_EndFrame( void );
void R_VT_SetWorldZoneResidency( const worldZoneResidency_t *zones, int count );

#endif
