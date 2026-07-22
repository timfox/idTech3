/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Clustered Hybrid M1 — R_SelectSurfaceRenderPath + path debug counters.
===========================================================================
*/

#include "tr_local.h"
#include "vk_render_path.h"
#include "vk_deferred_gbuffer.h"
#include "vk_transparency_route.h"
#include "tr_render_mode_vk.h"

cvar_t *r_renderPathDebug;
cvar_t *r_hybridCompare;

static uint32_t s_pathCounts[RENDER_PATH_COUNT];
static qboolean s_statusCmdRegistered;

const char *R_RenderPath_Name( renderPath_t path )
{
	switch ( path ) {
	case RENDER_PATH_NONE: return "none";
	case RENDER_PATH_LEGACY_FORWARD: return "legacy_forward";
	case RENDER_PATH_DEFERRED_OPAQUE: return "deferred_opaque";
	case RENDER_PATH_FORWARD_PLUS_OPAQUE: return "forward_plus_opaque";
	case RENDER_PATH_FORWARD_PLUS_TRANSPARENT: return "forward_plus_transparent";
	case RENDER_PATH_FORWARD_PLUS_WEAPON: return "forward_plus_weapon";
	case RENDER_PATH_OIT: return "oit";
	case RENDER_PATH_SKY: return "sky";
	case RENDER_PATH_UI: return "ui";
	default: return "unknown";
	}
}

void R_RenderPath_DebugColor( renderPath_t path, float outRgb[3] )
{
	/* Distinct, stable palette for r_renderPathDebug 1. */
	switch ( path ) {
	case RENDER_PATH_DEFERRED_OPAQUE:
		outRgb[0] = 0.15f; outRgb[1] = 0.55f; outRgb[2] = 1.00f; break; /* blue */
	case RENDER_PATH_FORWARD_PLUS_OPAQUE:
		outRgb[0] = 0.20f; outRgb[1] = 0.90f; outRgb[2] = 0.35f; break; /* green */
	case RENDER_PATH_FORWARD_PLUS_TRANSPARENT:
		outRgb[0] = 1.00f; outRgb[1] = 0.55f; outRgb[2] = 0.10f; break; /* orange */
	case RENDER_PATH_FORWARD_PLUS_WEAPON:
		outRgb[0] = 1.00f; outRgb[1] = 0.20f; outRgb[2] = 0.75f; break; /* magenta */
	case RENDER_PATH_OIT:
		outRgb[0] = 0.70f; outRgb[1] = 0.35f; outRgb[2] = 1.00f; break; /* violet */
	case RENDER_PATH_SKY:
		outRgb[0] = 0.45f; outRgb[1] = 0.75f; outRgb[2] = 1.00f; break; /* sky */
	case RENDER_PATH_UI:
		outRgb[0] = 0.90f; outRgb[1] = 0.90f; outRgb[2] = 0.20f; break; /* yellow */
	case RENDER_PATH_LEGACY_FORWARD:
		outRgb[0] = 0.65f; outRgb[1] = 0.65f; outRgb[2] = 0.65f; break; /* gray */
	default:
		outRgb[0] = 0.25f; outRgb[1] = 0.25f; outRgb[2] = 0.25f; break;
	}
}

qboolean R_RenderPath_WantsDeferredHandoff( renderPath_t path )
{
	return ( path == RENDER_PATH_DEFERRED_OPAQUE ) ? qtrue : qfalse;
}

void R_RenderPath_BeginFrame( void )
{
	Com_Memset( s_pathCounts, 0, sizeof( s_pathCounts ) );
}

void R_RenderPath_Note( renderPath_t path )
{
	if ( path >= 0 && path < RENDER_PATH_COUNT ) {
		s_pathCounts[path]++;
	}
}

void R_RenderPath_Status_f( void )
{
	int i;

	ri.Printf( PRINT_ALL, "======== Render Path Status ========\n" );
	ri.Printf( PRINT_ALL, "r_renderMode=%d pathDebug=%d hybridCompare=%d\n",
		r_renderMode ? r_renderMode->integer : -1,
		r_renderPathDebug ? r_renderPathDebug->integer : 0,
		r_hybridCompare ? r_hybridCompare->integer : 0 );
	ri.Printf( PRINT_ALL, "deferredPathReady=%s unified=%s\n",
		vk_deferred_lighting_path_ready() ? "yes" : "no",
		vk_unified_clustered_active() ? "yes" : "no" );
	for ( i = 0; i < RENDER_PATH_COUNT; i++ ) {
		if ( s_pathCounts[i] == 0u && i != RENDER_PATH_NONE ) {
			continue;
		}
		ri.Printf( PRINT_ALL, "  %-28s %u\n", R_RenderPath_Name( (renderPath_t)i ), s_pathCounts[i] );
	}
}

static qboolean R_Path_IsTransparentShader( const shader_t *shader )
{
	unsigned stageBits;
	unsigned srcBlend, dstBlend;
	qboolean additive;

	if ( !shader ) {
		return qfalse;
	}
	stageBits = shader->stages[0] ? shader->stages[0]->stateBits : 0;
	srcBlend = stageBits & GLS_SRCBLEND_BITS;
	dstBlend = stageBits & GLS_DSTBLEND_BITS;
	additive = ( srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE ) ? qtrue : qfalse;
	if ( ( shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 ) ||
		( srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) ||
		additive ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean R_Path_IsComplexOpaque( const shader_t *shader )
{
	if ( !shader ) {
		return qfalse;
	}
	if ( vk_transparency_is_refractive( shader ) ) {
		return qtrue;
	}
	if ( shader->hasScreenMap ) {
		return qtrue;
	}
	return qfalse;
}

renderPath_t R_SelectSurfaceRenderPath(
	const shader_t *shader,
	const surfaceType_t *surface,
	unsigned drawSurfSortFlags,
	int viewClass )
{
	const int mode = r_renderMode ? r_renderMode->integer : 0;
	renderPath_t path;
	const char *reason = "default";

	(void)surface;

	if ( viewClass == VK_VIEW_CLASS_UI || viewClass == VK_VIEW_CLASS_NO_WORLD ) {
		path = RENDER_PATH_UI;
		reason = "ui_or_no_world";
		goto done;
	}

	if ( drawSurfSortFlags & R_PATH_FLAG_FORCE_WEAPON ) {
		path = RENDER_PATH_FORWARD_PLUS_WEAPON;
		reason = "force_weapon";
		goto done;
	}

	if ( viewClass == VK_VIEW_CLASS_WEAPON || ( drawSurfSortFlags & R_PATH_FLAG_WEAPON_CANDIDATE ) ) {
		path = RENDER_PATH_FORWARD_PLUS_WEAPON;
		reason = "weapon_view";
		goto done;
	}

	if ( shader && shader->isSky ) {
		path = RENDER_PATH_SKY;
		reason = "sky";
		goto done;
	}

	if ( R_Path_IsTransparentShader( shader ) ) {
		if ( r_oit && r_oit->integer && r_fbo && r_fbo->integer &&
			vk_deferred_opaque_transparent_split() ) {
			path = RENDER_PATH_OIT;
			reason = "oit_enabled";
		} else {
			path = RENDER_PATH_FORWARD_PLUS_TRANSPARENT;
			reason = "transparent";
		}
		goto done;
	}

	/* Opaque */
	if ( mode == 0 || R_ClassicLightingActive() ) {
		path = RENDER_PATH_LEGACY_FORWARD;
		reason = "classic_or_mode0";
		goto done;
	}

	if ( R_Path_IsComplexOpaque( shader ) ) {
		path = RENDER_PATH_FORWARD_PLUS_OPAQUE;
		reason = "complex_opaque";
		goto done;
	}

	if ( ( mode == 1 || mode == 3 || mode == 4 ) &&
		vk_deferred_lighting_path_ready() &&
		vk_deferred_opaque_transparent_split() ) {
		path = RENDER_PATH_DEFERRED_OPAQUE;
		reason = "deferred_ready";
		goto done;
	}

	if ( r_forwardPlus && r_forwardPlus->integer ) {
		path = RENDER_PATH_FORWARD_PLUS_OPAQUE;
		reason = "forward_plus_opaque";
		goto done;
	}

	path = RENDER_PATH_LEGACY_FORWARD;
	reason = "fallback_legacy";

done:
	if ( r_renderPathDebug && r_renderPathDebug->integer >= 2 ) {
		static int s_logged;
		if ( s_logged < 32 ) {
			ri.Printf( PRINT_DEVELOPER, "[VK][path] %s (%s) mode=%d view=%s\n",
				R_RenderPath_Name( path ), reason, mode, vk_view_class_name( (vkViewClass_t)viewClass ) );
			s_logged++;
		}
	}
	return path;
}

void R_RenderPath_RegisterCvars( void )
{
	r_renderPathDebug = ri.Cvar_Get( "r_renderPathDebug", "0", CVAR_ARCHIVE_ND | CVAR_CHEAT );
	ri.Cvar_CheckRange( r_renderPathDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_renderPathDebug,
		"Clustered Hybrid path debug:\n"
		" 0 = off\n"
		" 1 = tint shaded surfaces by R_SelectSurfaceRenderPath\n"
		" 2 = also log path picks (developer) + render_path_status counts\n"
		"See docs/RENDERER_PATH_OWNERSHIP.md." );
	ri.Cvar_SetGroup( r_renderPathDebug, CVG_RENDERER );

	r_hybridCompare = ri.Cvar_Get( "r_hybridCompare", "0", CVAR_ARCHIVE_ND | CVAR_CHEAT );
	ri.Cvar_CheckRange( r_hybridCompare, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybridCompare,
		"Split-screen deferred vs Forward+ opaque (left=deferred handoff, right=Forward+). "
		"Requires deferred path ready (mode 1/3). Composite only updates the left half." );
	ri.Cvar_SetGroup( r_hybridCompare, CVG_RENDERER );

	if ( !s_statusCmdRegistered ) {
		ri.Cmd_AddCommand( "render_path_status", R_RenderPath_Status_f );
		s_statusCmdRegistered = qtrue;
		ri.Printf( PRINT_ALL, "[VK][path] R_SelectSurfaceRenderPath ready (r_renderPathDebug, r_hybridCompare, render_path_status)\n" );
	}
}
