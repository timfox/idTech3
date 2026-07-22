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
#include "vk.h"

cvar_t *r_renderPathDebug;
cvar_t *r_hybridCompare;
cvar_t *r_materialPathDebug;
cvar_t *r_materialPathReason;
cvar_t *r_gbufferCompact;

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
	qboolean verbose = ( ri.Cmd_Argc() >= 2 && !Q_stricmp( ri.Cmd_Argv( 1 ), "verbose" ) );

	ri.Printf( PRINT_ALL, "======== Render Path Status ========\n" );
	ri.Printf( PRINT_ALL, "r_renderMode=%d pathDebug=%d hybridCompare=%d\n",
		r_renderMode ? r_renderMode->integer : -1,
		r_renderPathDebug ? r_renderPathDebug->integer : 0,
		r_hybridCompare ? r_hybridCompare->integer : 0 );
	ri.Printf( PRINT_ALL, "deferredPathReady=%s unified=%s split=%s lightingActive=%s\n",
		vk_deferred_lighting_path_ready() ? "yes" : "no",
		vk_unified_clustered_active() ? "yes" : "no",
		vk_deferred_opaque_transparent_split() ? "yes" : "no",
		vk_deferred_lighting_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "oitFrameState=%u (0=untouched 3=resolved) r_oit=%d\n",
		vk.oitFrameState, r_oit ? r_oit->integer : 0 );
	for ( i = 0; i < RENDER_PATH_COUNT; i++ ) {
		if ( s_pathCounts[i] == 0u && i != RENDER_PATH_NONE ) {
			continue;
		}
		ri.Printf( PRINT_ALL, "  %-28s %u\n", R_RenderPath_Name( (renderPath_t)i ), s_pathCounts[i] );
	}
	if ( verbose ) {
		uint32_t deferredN = s_pathCounts[RENDER_PATH_DEFERRED_OPAQUE];
		uint32_t fpOpaqueN = s_pathCounts[RENDER_PATH_FORWARD_PLUS_OPAQUE];
		uint32_t legacyN = s_pathCounts[RENDER_PATH_LEGACY_FORWARD];
		uint32_t oitN = s_pathCounts[RENDER_PATH_OIT];
		ri.Printf( PRINT_ALL,
			"verbose: deferredOpaque=%u fpOpaque=%u legacy=%u oit=%u\n"
			"  invariant: with lightingActive=0, deferredOpaque must be 0 (Forward+/legacy own color)\n"
			"  post-OIT G-buffer capture is suppressed when oitFrameState==RESOLVED\n",
			deferredN, fpOpaqueN, legacyN, oitN );
		if ( !vk_deferred_lighting_active() && deferredN > 0u ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][path] WARNING: deferred_opaque selections while lighting inactive — check path ready\n"
				S_COLOR_WHITE );
		}
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
	ri.Cvar_CheckRange( r_hybridCompare, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybridCompare,
		"Deferred vs Forward+ hybrid compare (Clustered Hybrid M2):\n"
		" 0 = off\n"
		" 1 = split left deferred / right Forward+\n"
		" 2 = abs RGB difference\n"
		" 3 = relative luma difference\n"
		" 4 = diffuse-only compare (tint)\n"
		" 5 = specular-only compare (tint)\n"
		" 6 = cluster index mismatch (magenta)\n"
		" 7 = light-list membership mismatch\n"
		" 8 = shadow term difference\n"
		"Requires deferred path ready (mode 1/3). See docs/CLUSTERED_LIGHTING.md." );
	ri.Cvar_SetGroup( r_hybridCompare, CVG_RENDERER );

	ri.Cvar_Get( "r_hybridCompareWarn", "0.05", CVAR_ARCHIVE_ND | CVAR_CHEAT );
	ri.Cvar_Get( "r_hybridCompareFail", "0.25", CVAR_ARCHIVE_ND | CVAR_CHEAT );

	r_materialPathDebug = ri.Cvar_Get( "r_materialPathDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_materialPathDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialPathDebug, "Log R_SelectMaterialRenderPath decisions (cheat)." );
	ri.Cvar_SetGroup( r_materialPathDebug, CVG_RENDERER );

	r_materialPathReason = ri.Cvar_Get( "r_materialPathReason", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_materialPathReason, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialPathReason, "When 1, print material path fallback reason." );
	ri.Cvar_SetGroup( r_materialPathReason, CVG_RENDERER );

	r_gbufferCompact = ri.Cvar_Get( "r_gbufferCompact", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_gbufferCompact, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gbufferCompact,
		"Compact G-buffer 2.0 prep (latched). Dual-writes octahedral into material.ba (direct MRT + depth fill);\n"
		"AO in normal.a; lighting decodes oct. See docs/GBUFFER_2.md." );
	ri.Cvar_SetGroup( r_gbufferCompact, CVG_RENDERER );

	if ( !s_statusCmdRegistered ) {
		ri.Cmd_AddCommand( "render_path_status", R_RenderPath_Status_f );
		s_statusCmdRegistered = qtrue;
		ri.Printf( PRINT_ALL, "[VK][path] R_SelectSurfaceRenderPath ready (r_renderPathDebug, r_hybridCompare, render_path_status)\n" );
	}
}

renderPath_t R_SelectMaterialRenderPath(
	const shader_t *shader,
	unsigned materialFeatureFlags,
	const char **outReason )
{
	const char *reason = "deferred_standard";
	renderPath_t path;
	const int mode = r_renderMode ? r_renderMode->integer : 0;

	if ( materialFeatureFlags & R_MAT_FEAT_FORWARD_ONLY ) {
		path = RENDER_PATH_FORWARD_PLUS_OPAQUE;
		if ( materialFeatureFlags & R_MAT_FEAT_TRANSMISSION ) {
			reason = "unsupported_deferred:transmission";
		} else if ( materialFeatureFlags & R_MAT_FEAT_REFRACTION ) {
			reason = "unsupported_deferred:refraction";
		} else if ( materialFeatureFlags & R_MAT_FEAT_ANISOTROPY ) {
			reason = "unsupported_deferred:anisotropy";
		} else if ( materialFeatureFlags & R_MAT_FEAT_WATER ) {
			reason = "unsupported_deferred:water";
		} else if ( materialFeatureFlags & R_MAT_FEAT_SKIN ) {
			reason = "unsupported_deferred:skin";
		} else if ( materialFeatureFlags & R_MAT_FEAT_LAYERED ) {
			reason = "unsupported_deferred:layered";
		} else {
			reason = "unsupported_deferred:complex_coat";
		}
		goto done;
	}

	if ( R_Path_IsTransparentShader( shader ) ) {
		path = ( r_oit && r_oit->integer ) ? RENDER_PATH_OIT : RENDER_PATH_FORWARD_PLUS_TRANSPARENT;
		reason = "transparent";
		goto done;
	}

	if ( ( mode == 1 || mode == 3 || mode == 4 ) && vk_deferred_lighting_path_ready() ) {
		path = RENDER_PATH_DEFERRED_OPAQUE;
		reason = "deferred_standard";
		goto done;
	}

	path = RENDER_PATH_FORWARD_PLUS_OPAQUE;
	reason = "forward_plus_fallback";

done:
	if ( outReason ) {
		*outReason = reason;
	}
	if ( ( r_materialPathDebug && r_materialPathDebug->integer ) ||
		( r_materialPathReason && r_materialPathReason->integer ) ) {
		ri.Printf( PRINT_ALL, "[VK][materialPath] shader=%s path=%s flags=0x%x reason=%s\n",
			( shader && shader->name[0] ) ? shader->name : "(null)",
			R_RenderPath_Name( path ), materialFeatureFlags, reason );
	}
	R_RenderPath_Note( path );
	return path;
}

void R_RenderPath_GetOpaqueCounts( uint32_t *outDeferred, uint32_t *outForwardPlus )
{
	if ( outDeferred ) {
		*outDeferred = s_pathCounts[RENDER_PATH_DEFERRED_OPAQUE];
	}
	if ( outForwardPlus ) {
		*outForwardPlus = s_pathCounts[RENDER_PATH_FORWARD_PLUS_OPAQUE];
	}
}
