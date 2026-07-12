/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Upscale path: r_upscale 0=off, 1=spatial (r_renderScale blit), 2=engine temporal
upsample (Halton jitter + TAA at internal res, then spatial blit to window).
No FidelityFX/DLSS SDK — see docs/RENDERERS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_upscale.h"
#include "vk_view_state.h"
#include "vk_temporal.h"

static cvar_t *r_upscale;
static cvar_t *r_upscaleSharpness;
static cvar_t *r_upscaleDisplayHistory;

static float s_jitterX;
static float s_jitterY;
static float s_prevJitterX;
static float s_prevJitterY;
static uint32_t s_jitterFrame;
static qboolean s_cmds_registered;

static float Upscale_Halton( uint32_t index, uint32_t base )
{
	float f = 1.0f;
	float r = 0.0f;
	uint32_t i = index;

	while ( i > 0u ) {
		f /= (float)base;
		r += f * (float)( i % base );
		i /= base;
	}
	return r;
}

static void Upscale_Status_f( void )
{
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	int rw = r_renderWidth ? r_renderWidth->integer : 0;
	int rh = r_renderHeight ? r_renderHeight->integer : 0;

	if ( rw > 0 && gls.windowWidth > 0 ) {
		scaleX = (float)gls.windowWidth / (float)rw;
	}
	if ( rh > 0 && gls.windowHeight > 0 ) {
		scaleY = (float)gls.windowHeight / (float)rh;
	}

	ri.Printf( PRINT_ALL,
		"[VK][upscale] mode=%d spatial=%d temporal=%d sharpness=%.2f displayHistory=%d\n"
		"  renderScale=%d render=%dx%d window=%dx%d upsample=%.2fx%.2f\n"
		"  jitter=(%.4f,%.4f) prev=(%.4f,%.4f) frame=%u\n"
		"  resolve=%s\n",
		r_upscale ? r_upscale->integer : 0,
		R_Upscale_WantSpatial() ? 1 : 0,
		R_Upscale_WantTemporal() ? 1 : 0,
		R_Upscale_GetSharpness(),
		( r_upscaleDisplayHistory && r_upscaleDisplayHistory->integer ) ? 1 : 0,
		r_renderScale ? r_renderScale->integer : 0,
		rw, rh, gls.windowWidth, gls.windowHeight, scaleX, scaleY,
		s_jitterX, s_jitterY, s_prevJitterX, s_prevJitterY, s_jitterFrame,
		R_Upscale_WantTemporal()
			? ( ( r_upscaleDisplayHistory && r_upscaleDisplayHistory->integer )
				? "temporal@internal + spatial blit (display ping-pong requested)"
				: "temporal@internal + spatial blit" )
			: ( R_Upscale_WantSpatial() ? "spatial blit only" : "off" ) );
}

void R_Upscale_Init( void )
{
	r_upscale = ri.Cvar_Get( "r_upscale", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_upscale, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_upscale,
		"Upscaler: 0=off, 1=spatial (auto r_renderScale + blit), 2=temporal (Halton jitter + TAA then spatial blit). No FSR2/DLSS SDK." );
	ri.Cvar_SetGroup( r_upscale, CVG_RENDERER );

	r_upscaleSharpness = ri.Cvar_Get( "r_upscaleSharpness", "0.2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_upscaleSharpness, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_upscaleSharpness, "Extra TAA sharpen when r_upscale 2 (added to r_taa_sharpen)." );
	ri.Cvar_SetGroup( r_upscaleSharpness, CVG_RENDERER );

	r_upscaleDisplayHistory = ri.Cvar_Get( "r_upscaleDisplayHistory", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_upscaleDisplayHistory, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_upscaleDisplayHistory,
		"Request display-sized temporal history for r_upscale 2 (v1: status + sharpen path; ping-pong RT deferred)." );
	ri.Cvar_SetGroup( r_upscaleDisplayHistory, CVG_RENDERER );

	s_jitterX = s_jitterY = 0.0f;
	s_prevJitterX = s_prevJitterY = 0.0f;
	s_jitterFrame = 0;

	if ( !s_cmds_registered ) {
		ri.Cmd_AddCommand( "upscale_status", Upscale_Status_f );
		s_cmds_registered = qtrue;
	}

	if ( r_upscale->integer == 1 ) {
		ri.Printf( PRINT_ALL,
			"[VK][upscale] r_upscale=1 spatial — uses r_renderWidth/Height + r_renderScale blit\n" );
	} else if ( r_upscale->integer == 2 ) {
		ri.Printf( PRINT_ALL,
			"[VK][upscale] r_upscale=2 temporal — Halton jitter + TAA at internal res, spatial blit to window\n" );
	}
}

void R_Upscale_Shutdown( void )
{
	if ( s_cmds_registered ) {
		ri.Cmd_RemoveCommand( "upscale_status" );
		s_cmds_registered = qfalse;
	}
}

qboolean R_Upscale_WantSpatial( void )
{
	return ( r_upscale && r_upscale->integer >= 1 ) ? qtrue : qfalse;
}

qboolean R_Upscale_WantTemporal( void )
{
	return ( r_upscale && r_upscale->integer == 2 ) ? qtrue : qfalse;
}

float R_Upscale_GetSharpness( void )
{
	return r_upscaleSharpness ? r_upscaleSharpness->value : 0.2f;
}

void R_Upscale_ApplyRenderScaleDefaults( void )
{
	int rw;
	int rh;

	if ( !R_Upscale_WantSpatial() ) {
		return;
	}
	if ( !r_renderScale || r_renderScale->integer > 0 ) {
		return;
	}

	rw = gls.windowWidth > 0 ? gls.windowWidth : ( r_renderWidth ? r_renderWidth->integer : 800 );
	rh = gls.windowHeight > 0 ? gls.windowHeight : ( r_renderHeight ? r_renderHeight->integer : 600 );
	rw = (int)( (float)rw * 0.75f + 0.5f );
	rh = (int)( (float)rh * 0.75f + 0.5f );
	if ( rw < 96 ) {
		rw = 96;
	}
	if ( rh < 72 ) {
		rh = 72;
	}

	ri.Cvar_Set( "r_renderScale", "3" );
	ri.Cvar_SetValue( "r_renderWidth", (float)rw );
	ri.Cvar_SetValue( "r_renderHeight", (float)rh );
	ri.Printf( PRINT_ALL,
		"[VK][upscale] auto r_renderScale 3 @ %dx%d (75%% window) for r_upscale %d\n",
		rw, rh, r_upscale->integer );
}

void R_Upscale_GetJitter( float *jitterX, float *jitterY )
{
	if ( jitterX ) {
		*jitterX = s_jitterX;
	}
	if ( jitterY ) {
		*jitterY = s_jitterY;
	}
}

void R_Upscale_GetPrevJitter( float *jitterX, float *jitterY )
{
	if ( jitterX ) {
		*jitterX = s_prevJitterX;
	}
	if ( jitterY ) {
		*jitterY = s_prevJitterY;
	}
}

void R_Upscale_NoteJitter( float *jitterX, float *jitterY )
{
	float jx = 0.0f;
	float jy = 0.0f;
	uint32_t w;
	uint32_t h;

	s_prevJitterX = s_jitterX;
	s_prevJitterY = s_jitterY;

	if ( !R_Upscale_WantTemporal() ) {
		s_jitterX = 0.0f;
		s_jitterY = 0.0f;
		if ( jitterX ) {
			*jitterX = 0.0f;
		}
		if ( jitterY ) {
			*jitterY = 0.0f;
		}
		return;
	}

	s_jitterFrame++;
	/* Halton(2,3) in [-0.5,0.5] pixel offsets */
	jx = Upscale_Halton( s_jitterFrame, 2u ) - 0.5f;
	jy = Upscale_Halton( s_jitterFrame, 3u ) - 0.5f;

	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w < 1u ) {
		w = 1u;
	}
	if ( h < 1u ) {
		h = 1u;
	}
	/* Convert to NDC offset later; store as pixel jitter for TAA unjitter */
	s_jitterX = jx;
	s_jitterY = jy;

	if ( jitterX ) {
		*jitterX = s_jitterX;
	}
	if ( jitterY ) {
		*jitterY = s_jitterY;
	}
}

void R_Upscale_ApplyProjectionJitter( float projectionMatrix[16] )
{
	uint32_t w;
	uint32_t h;
	float jx;
	float jy;

	if ( !projectionMatrix || !R_Upscale_WantTemporal() ) {
		return;
	}

	R_Upscale_NoteJitter( &jx, &jy );
	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w < 1u ) {
		w = 1u;
	}
	if ( h < 1u ) {
		h = 1u;
	}
	/* OpenGL-style projection: offset clip.xy by 2*jitter/size */
	projectionMatrix[8] += ( 2.0f * jx ) / (float)w;
	projectionMatrix[9] += ( 2.0f * jy ) / (float)h;
}
