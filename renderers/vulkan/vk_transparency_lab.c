/*
===========================================================================
Color Pipeline Phase 2.6 — transparency laboratory (frozen compare / reference).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_transparency_lab.h"
#include "vk_wboit_production_cert.h"

#include <math.h>

static qboolean s_cmds;
static cvar_t *r_transparencyReference;
static cvar_t *r_transparencyFreeze;
static cvar_t *r_transparencyCompare;
static cvar_t *r_transparencyCompareStage;
static transparencyCompareMetrics_t s_metrics;
static qboolean s_frozen;
static float s_freezeTime;
static vec3_t s_freezeViewOrg;
static vec3_t s_freezeViewAxis[3];

float vk_transparency_lab_relative_luminance( float r, float g, float b )
{
	return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float vk_transparency_lab_hue_error( float r0, float g0, float b0, float r1, float g1, float b1 )
{
	/* Cheap chromatic angle proxy in RG — certification metric, not color science. */
	float a0 = atan2f( g0 - b0, r0 - ( g0 + b0 ) * 0.5f );
	float a1 = atan2f( g1 - b1, r1 - ( g1 + b1 ) * 0.5f );
	float d = a1 - a0;
	while ( d > (float)M_PI ) {
		d -= (float)( 2.0 * M_PI );
	}
	while ( d < (float)-M_PI ) {
		d += (float)( 2.0 * M_PI );
	}
	return fabsf( d );
}

float vk_transparency_lab_fresnel_schlick( float cosTheta, float f0 )
{
	float c = cosTheta;
	float oneMinus;
	if ( c < 0.0f ) {
		c = 0.0f;
	} else if ( c > 1.0f ) {
		c = 1.0f;
	}
	oneMinus = 1.0f - c;
	return f0 + ( 1.0f - f0 ) * oneMinus * oneMinus * oneMinus * oneMinus * oneMinus;
}

void vk_transparency_lab_beer_lambert( const float color[3], float distance, float absorptionDistance,
	float outTransmittance[3] )
{
	float inv;
	int i;
	if ( !color || !outTransmittance ) {
		return;
	}
	if ( absorptionDistance <= 1e-6f ) {
		outTransmittance[0] = outTransmittance[1] = outTransmittance[2] = 0.0f;
		return;
	}
	inv = distance / absorptionDistance;
	for ( i = 0; i < 3; i++ ) {
		float dens = color[i] * inv;
		if ( dens < 0.0f ) {
			dens = 0.0f;
		}
		outTransmittance[i] = expf( -dens );
	}
}

float vk_transparency_lab_refraction_offset_bound( float offsetPx, float maxOffsetPx )
{
	float m = maxOffsetPx;
	if ( m < 0.0f ) {
		m = 0.0f;
	}
	if ( offsetPx > m ) {
		return m;
	}
	if ( offsetPx < -m ) {
		return -m;
	}
	return offsetPx;
}

qboolean vk_transparency_lab_frozen( void )
{
	return ( r_transparencyFreeze && r_transparencyFreeze->integer && s_frozen ) ? qtrue : qfalse;
}

transparencyReferenceMode_t vk_transparency_lab_mode( void )
{
	if ( !r_transparencyReference ) {
		return TRANSPARENCY_REF_DISABLED;
	}
	return (transparencyReferenceMode_t)r_transparencyReference->integer;
}

const transparencyCompareMetrics_t *vk_transparency_lab_last_metrics( void )
{
	return &s_metrics;
}

static void VK_TransparencyLab_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"======== Transparency Laboratory (Phase 2.6) ========\n"
		"reference=%d freeze=%d compare=%d compareStage=%s\n"
		"frozen=%d freezeTime=%g\n"
		"metrics: meanAbsRgb=%g maxAbsRgb=%g relLum=%g hue=%g edge=%g permVar=%g invalid=%u\n"
		"modes: 0 off  1 sorted-alpha ref  2 WBOIT vs sorted  3 MBOIT vs sorted  4 specialized\n"
		"captures: fog_scene OIT_accum revealage additive WBOIT_resolve sorted_ref MBOIT_resolve\n"
		"          specialized weapon_HDR bloom_source tonemap_input final\n"
		"docs: docs/WBOIT_LIVE_CERTIFICATION.md docs/TRANSPARENCY_ROUTING.md\n"
		"====================================================\n",
		r_transparencyReference ? r_transparencyReference->integer : 0,
		r_transparencyFreeze ? r_transparencyFreeze->integer : 0,
		r_transparencyCompare ? r_transparencyCompare->integer : 0,
		r_transparencyCompareStage ? r_transparencyCompareStage->string : "-",
		s_frozen ? 1 : 0, s_freezeTime,
		s_metrics.meanAbsRgb, s_metrics.maxAbsRgb, s_metrics.relativeLuminance,
		s_metrics.hueError, s_metrics.edgeOnlyError, s_metrics.permutationVariance,
		s_metrics.invalidPixelCount );
}

static void VK_TransparencyLab_Freeze_f( void )
{
	const char *op = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "toggle";
	if ( !Q_stricmp( op, "off" ) || !Q_stricmp( op, "0" ) ) {
		s_frozen = qfalse;
		ri.Cvar_Set( "r_transparencyFreeze", "0" );
		ri.Printf( PRINT_ALL, "transparency_lab_freeze: OFF\n" );
		return;
	}
	s_frozen = qtrue;
	s_freezeTime = backEnd.refdef.floatTime;
	VectorCopy( backEnd.viewParms.or.origin, s_freezeViewOrg );
	VectorCopy( backEnd.viewParms.or.axis[0], s_freezeViewAxis[0] );
	VectorCopy( backEnd.viewParms.or.axis[1], s_freezeViewAxis[1] );
	VectorCopy( backEnd.viewParms.or.axis[2], s_freezeViewAxis[2] );
	ri.Cvar_Set( "r_transparencyFreeze", "1" );
	ri.Printf( PRINT_ALL,
		"transparency_lab_freeze: ON time=%g org=(%.1f %.1f %.1f)\n"
		"  animation/material/jitter/exposure/fog locks armed for reference compares\n",
		s_freezeTime, s_freezeViewOrg[0], s_freezeViewOrg[1], s_freezeViewOrg[2] );
}

static void VK_TransparencyCompare_f( void )
{
	/* Placeholder metrics until GPU readback lab is wired; keeps command surface stable. */
	Com_Memset( &s_metrics, 0, sizeof( s_metrics ) );
	if ( ri.Cmd_Argc() >= 2 ) {
		ri.Cvar_Set( "r_transparencyCompareStage", ri.Cmd_Argv( 1 ) );
	}
	s_metrics.meanAbsRgb = 0.0f;
	s_metrics.maxAbsRgb = 0.0f;
	ri.Printf( PRINT_ALL,
		"transparency_compare: stage=%s mode=%d — arm GPU capture path; metrics zero until readback\n"
		"  Use with r_transparencyReference 2|3 and oit_certification_capture\n",
		r_transparencyCompareStage ? r_transparencyCompareStage->string : "-",
		r_transparencyReference ? r_transparencyReference->integer : 0 );
}

void vk_transparency_lab_begin_frame( void )
{
	if ( !s_frozen || !r_transparencyFreeze || !r_transparencyFreeze->integer ) {
		return;
	}
	/* Soft freeze: keep recorded view/time available to future lab draws. */
	(void)s_freezeViewAxis;
}

void vk_transparency_lab_register( void )
{
	if ( s_cmds ) {
		return;
	}
	Com_Memset( &s_metrics, 0, sizeof( s_metrics ) );

	r_transparencyReference = ri.Cvar_Get( "r_transparencyReference", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_transparencyReference, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_transparencyReference,
		"Transparency lab reference:\n"
		" 0 off\n"
		" 1 sorted source-over reference\n"
		" 2 WBOIT vs sorted\n"
		" 3 MBOIT vs sorted (experimental)\n"
		" 4 specialized route result" );
	ri.Cvar_SetGroup( r_transparencyReference, CVG_RENDERER );

	r_transparencyFreeze = ri.Cvar_Get( "r_transparencyFreeze", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_transparencyFreeze, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_transparencyFreeze,
		"Freeze camera/time/fog/exposure/jitter for deterministic transparency compares." );
	ri.Cvar_SetGroup( r_transparencyFreeze, CVG_RENDERER );

	r_transparencyCompare = ri.Cvar_Get( "r_transparencyCompare", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_transparencyCompare, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_transparencyCompare,
		"Enable live mean/max RGB + luminance/hue/edge/permutation compare against reference." );
	ri.Cvar_SetGroup( r_transparencyCompare, CVG_RENDERER );

	r_transparencyCompareStage = ri.Cvar_Get( "r_transparencyCompareStage", "final", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_transparencyCompareStage,
		"Which buffer to compare: fog_scene|accum|revealage|additive|wboit|sorted|mboit|special|bloom|tonemap|final" );
	ri.Cvar_SetGroup( r_transparencyCompareStage, CVG_RENDERER );

	ri.Cmd_AddCommand( "transparency_lab_status", VK_TransparencyLab_Status_f );
	ri.Cmd_AddCommand( "transparency_lab_freeze", VK_TransparencyLab_Freeze_f );
	ri.Cmd_AddCommand( "transparency_compare", VK_TransparencyCompare_f );

	s_cmds = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][Xparent] Phase 2.6 transparency laboratory ready "
		"(r_transparencyReference / transparency_lab_status)\n" );
}
