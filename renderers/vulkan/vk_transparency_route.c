/*
===========================================================================
Raster Ultra 1.4 — transparency classification + refractive exclusion helpers.
===========================================================================
*/

#include "tr_local.h"
#include "vk_transparency_route.h"

static cvar_t *r_transparencyDebug;
static cvar_t *r_refractiveExcludeOit;
static qboolean s_inited;

const char *vk_transparency_class_name( vkTransparencyClass_t cls )
{
	switch ( cls ) {
	case VK_XPARENT_ALPHA_TESTED: return "alpha_tested";
	case VK_XPARENT_SORTED_ALPHA: return "sorted_alpha";
	case VK_XPARENT_WBOIT: return "wboit";
	case VK_XPARENT_ADDITIVE: return "additive";
	case VK_XPARENT_MODULATE: return "modulate";
	case VK_XPARENT_REFRACTIVE: return "refractive";
	case VK_XPARENT_WATER: return "water";
	case VK_XPARENT_GLASS: return "glass";
	case VK_XPARENT_DISTORTION_ONLY: return "distortion_only";
	case VK_XPARENT_PARTICLE: return "particle";
	case VK_XPARENT_DECAL: return "decal";
	case VK_XPARENT_UI: return "ui";
	default: return "unknown";
	}
}

qboolean vk_transparency_is_additive( const shader_t *shader )
{
	unsigned stageBits, src, dst;

	if ( !shader || !shader->stages[0] ) {
		return qfalse;
	}
	stageBits = shader->stages[0]->stateBits;
	src = stageBits & GLS_SRCBLEND_BITS;
	dst = stageBits & GLS_DSTBLEND_BITS;
	return ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) ? qtrue : qfalse;
}

qboolean vk_transparency_is_refractive( const shader_t *shader )
{
	if ( !shader ) {
		return qfalse;
	}
	if ( shader->hasScreenMap ) {
		return qtrue;
	}
	/* Heuristic: water/glass naming when screenMap absent but material is refractive. */
	if ( shader->name[0] ) {
		if ( Q_stristr( shader->name, "water" ) || Q_stristr( shader->name, "glass" ) ||
			Q_stristr( shader->name, "portal" ) || Q_stristr( shader->name, "refract" ) ) {
			if ( shader->sort >= SS_BLEND0 ) {
				return qtrue;
			}
		}
	}
	return qfalse;
}

vkTransparencyClass_t vk_transparency_classify_shader( const shader_t *shader )
{
	unsigned stageBits, src, dst;
	qboolean oitOn;

	if ( !shader ) {
		return VK_XPARENT_SORTED_ALPHA;
	}
	if ( shader->isSky ) {
		return VK_XPARENT_SORTED_ALPHA;
	}

	/* Alpha test / stochastic clip: opaque-ish cutout. */
	if ( shader->stages[0] && ( shader->stages[0]->stateBits & GLS_ATEST_BITS ) ) {
		return VK_XPARENT_ALPHA_TESTED;
	}

	if ( vk_transparency_is_refractive( shader ) ) {
		if ( shader->name[0] && Q_stristr( shader->name, "water" ) ) {
			return VK_XPARENT_WATER;
		}
		if ( shader->name[0] && Q_stristr( shader->name, "glass" ) ) {
			return VK_XPARENT_GLASS;
		}
		return VK_XPARENT_REFRACTIVE;
	}

	if ( !shader->stages[0] ) {
		return VK_XPARENT_SORTED_ALPHA;
	}
	stageBits = shader->stages[0]->stateBits;
	src = stageBits & GLS_SRCBLEND_BITS;
	dst = stageBits & GLS_DSTBLEND_BITS;

	if ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) {
		if ( shader->entityMergable ) {
			return VK_XPARENT_PARTICLE;
		}
		return VK_XPARENT_ADDITIVE;
	}
	if ( src == GLS_SRCBLEND_ZERO &&
		( dst == GLS_DSTBLEND_SRC_COLOR || dst == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) {
		return VK_XPARENT_MODULATE;
	}

	oitOn = ( r_oit && r_oit->integer == 1 ) ? qtrue : qfalse;
	if ( oitOn && shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 ) {
		return VK_XPARENT_WBOIT;
	}
	if ( shader->sort >= SS_BLEND0 ) {
		return VK_XPARENT_SORTED_ALPHA;
	}
	return VK_XPARENT_SORTED_ALPHA;
}

qboolean vk_transparency_debug_active( void )
{
	return ( r_transparencyDebug && r_transparencyDebug->integer ) ? qtrue : qfalse;
}

static void VK_TransparencyRoute_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"transparencyRoute: oit=%d classify=%d refractiveExclude=%d debug=%d\n"
		"  classes: alpha_tested sorted_alpha wboit additive modulate refractive "
		"water glass distortion particle decal ui\n",
		r_oit ? r_oit->integer : 0,
		r_oitClassify ? r_oitClassify->integer : 0,
		r_refractiveExcludeOit ? r_refractiveExcludeOit->integer : 0,
		r_transparencyDebug ? r_transparencyDebug->integer : 0 );
}

void vk_transparency_route_init( void )
{
	if ( s_inited ) {
		return;
	}
	r_transparencyDebug = ri.Cvar_Get( "r_transparencyDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_transparencyDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_transparencyDebug,
		"Raster Ultra 1.4 transparency routing debug:\n"
		" 0 - off\n"
		" 1 - log classify counts (developer)\n"
		" 2 - force refractive OIT exclusion visualization via oitDebug" );
	ri.Cvar_SetGroup( r_transparencyDebug, CVG_RENDERER );

	r_refractiveExcludeOit = ri.Cvar_Get( "r_refractiveExcludeOit", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_refractiveExcludeOit, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_refractiveExcludeOit,
		"Exclude screenMap/water/glass refractive shaders from WBOIT/MBOIT;\n"
		"draw them sorted after OIT resolve using opaque scene color (Raster Ultra 1.4)." );
	ri.Cvar_SetGroup( r_refractiveExcludeOit, CVG_RENDERER );

	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "transparency_route_status", VK_TransparencyRoute_Status_f );
	}
	s_inited = qtrue;
	ri.Printf( PRINT_ALL, "[VK][Xparent] transparency routing initialized (refractiveExcludeOit=%d)\n",
		r_refractiveExcludeOit->integer );
}

void vk_transparency_route_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "transparency_route_status" );
	}
	s_inited = qfalse;
}

/* Used by tr_backend filter — keep cvar readable without header export of cvar. */
qboolean vk_transparency_refractive_exclude_oit( void )
{
	return ( !r_refractiveExcludeOit || r_refractiveExcludeOit->integer ) ? qtrue : qfalse;
}
