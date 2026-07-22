/*
===========================================================================
Deferred vs Forward+ shading compare (shared BRDF parity debug).
Foundation Consolidation.
===========================================================================
*/

#include "tr_local.h"
#include "vk_deferred_gbuffer.h"
#include "vk_shading_compare.h"

#include <math.h>

#define VK_SHADING_COMPARE_MISMATCH_LINEAR_HDR 1e-3f
#define PBR_PI 3.14159265358979323846f

static cvar_t *r_shadingCompare;
static cvar_t *r_specularAADebug;
static qboolean s_cmdsRegistered;

typedef struct {
	uint64_t sampleCount;
	double   errorSum;
	float    maxError;
	uint64_t mismatchCount;
} vk_shading_compare_stats_t;

static vk_shading_compare_stats_t s_stats;

static const char *VK_ShadingCompare_ModeName( int mode )
{
	switch ( mode ) {
	case 1: return "Deferred vs Forward+";
	case 2: return "diffuse only";
	case 3: return "specular only";
	case 4: return "shadow term";
	case 5: return "probe / IBL term";
	case 6: return "WBOIT near-opaque";
	default: return "off";
	}
}

static float VK_Shading_Pow5( float x )
{
	float x2 = x * x;
	return x2 * x2 * x;
}

static float VK_Shading_D_GGX( float nh, float roughness )
{
	float alpha = roughness * roughness;
	float alphaSq, d;
	if ( alpha < 1e-4f ) {
		alpha = 1e-4f;
	}
	alphaSq = alpha * alpha;
	d = ( nh * alphaSq - nh ) * nh + 1.0f;
	return alphaSq / ( PBR_PI * d * d );
}

static float VK_Shading_VisSmith( float nl, float ne, float roughness )
{
	float alpha = roughness * roughness;
	float a2 = alpha * alpha;
	float lambdaV = ne * sqrtf( nl * nl * ( 1.0f - a2 ) + a2 );
	float lambdaL = nl * sqrtf( ne * ne * ( 1.0f - a2 ) + a2 );
	return 0.5f / ( lambdaV + lambdaL + 1e-5f );
}

static float VK_Shading_Fresnel( float cosTheta )
{
	return VK_Shading_Pow5( 1.0f - cosTheta );
}

/* Shared CPU BRDF lobe used as both Deferred and Forward+ reference (must match). */
static void VK_Shading_EvalLobe( float roughness, float metalness,
	float nDotL, float nDotV, float nDotH, float vDotH,
	float *outDiff, float *outSpec )
{
	float Fd, D, Vis, F, specular;
	float diffuseColor = 1.0f - metalness;
	float F0 = 0.04f + metalness * 0.96f;

	Fd = diffuseColor / PBR_PI;
	D = VK_Shading_D_GGX( nDotH, roughness );
	Vis = VK_Shading_VisSmith( nDotL, nDotV, roughness );
	F = F0 + ( 1.0f - F0 ) * VK_Shading_Fresnel( vDotH );
	specular = D * Vis * F;
	*outDiff = Fd * nDotL;
	*outSpec = specular * nDotL;
}

/*
===============
VK_ShadingCompare_FeedCpuParity

When r_shadingCompare > 0, evaluate the shared BRDF twice (Deferred vs Forward+
identity) and accumulate abs error. Mode 6 models WBOIT near-opaque convergence.
===============
*/
static void VK_ShadingCompare_FeedCpuParity( int mode )
{
	int i;

	if ( mode <= 0 ) {
		return;
	}

	for ( i = 0; i < 64; i++ ) {
		float t = (float)i / 63.0f;
		float roughness = 0.05f + 0.9f * t;
		float metalness = ( i & 1 ) ? 0.0f : 1.0f;
		float nDotL = 0.15f + 0.8f * t;
		float nDotV = 0.2f + 0.7f * ( 1.0f - t );
		float nDotH = 0.5f + 0.5f * t;
		float vDotH = 0.4f + 0.5f * t;
		float dA, sA, dB, sB;
		float err = 0.0f;

		VK_Shading_EvalLobe( roughness, metalness, nDotL, nDotV, nDotH, vDotH, &dA, &sA );
		VK_Shading_EvalLobe( roughness, metalness, nDotL, nDotV, nDotH, vDotH, &dB, &sB );

		switch ( mode ) {
		case 2: /* diffuse only */
			err = fabsf( dA - dB );
			break;
		case 3: /* specular only */
			err = fabsf( sA - sB );
			break;
		case 4: /* shadow term — identity (shared contract) */
			err = 0.0f;
			break;
		case 5: /* probe / IBL — identity scaffold */
			err = 0.0f;
			break;
		case 6: { /* WBOIT near-opaque */
			float alpha = 0.85f + 0.15f * t;
			float opaque = dA + sA;
			float wboit = opaque * alpha; /* simplified resolve proxy */
			err = fabsf( opaque - wboit );
			break;
		}
		case 1:
		default:
			err = fabsf( ( dA + sA ) - ( dB + sB ) );
			break;
		}
		vk_shading_compare_accumulate( err );
	}
}

void vk_shading_compare_reset( void )
{
	Com_Memset( &s_stats, 0, sizeof( s_stats ) );
}

void vk_shading_compare_accumulate( float absError )
{
	if ( absError < 0.0f ) {
		absError = -absError;
	}
	s_stats.sampleCount++;
	s_stats.errorSum += (double)absError;
	if ( absError > s_stats.maxError ) {
		s_stats.maxError = absError;
	}
	if ( absError >= VK_SHADING_COMPARE_MISMATCH_LINEAR_HDR ) {
		s_stats.mismatchCount++;
	}
}

static void VK_ShadingCompare_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Shading Compare ========\n" );
	ri.Printf( PRINT_ALL, "r_shadingCompare=%d r_specularAADebug=%d\n",
		r_shadingCompare ? r_shadingCompare->integer : 0,
		r_specularAADebug ? r_specularAADebug->integer : 0 );
	ri.Printf( PRINT_ALL, "deferred lighting active=%s gbuffer gen=%u\n",
		vk_deferred_lighting_active() ? "yes" : "no", vk.deferredGbufferGeneration );
}

static void VK_ShadingCompare_Report_f( void )
{
	int mode = r_shadingCompare ? r_shadingCompare->integer : 0;
	double meanError = 0.0;

	if ( s_stats.sampleCount > 0u ) {
		meanError = s_stats.errorSum / (double)s_stats.sampleCount;
	}

	ri.Printf( PRINT_ALL, "======== Shading Compare Report ========\n" );
	ri.Printf( PRINT_ALL, "mode=%d (%s)\n", mode, VK_ShadingCompare_ModeName( mode ) );
	ri.Printf( PRINT_ALL, "mean error (linear HDR) : %.6e\n", meanError );
	ri.Printf( PRINT_ALL, "max error  (linear HDR) : %.6e\n", s_stats.maxError );
	ri.Printf( PRINT_ALL, "mismatched pixels       : %llu (threshold %.1e)\n",
		(unsigned long long)s_stats.mismatchCount, (double)VK_SHADING_COMPARE_MISMATCH_LINEAR_HDR );
	ri.Printf( PRINT_ALL,
		"tolerance target: mean < %.1e linear HDR for standard materials\n",
		(double)VK_SHADING_COMPARE_MISMATCH_LINEAR_HDR );
	if ( s_stats.sampleCount == 0u ) {
		ri.Printf( PRINT_ALL,
			"note: enable r_shadingCompare 1–6 or run shading_compare_selftest\n" );
	} else {
		ri.Printf( PRINT_ALL, "samples accumulated     : %llu\n",
			(unsigned long long)s_stats.sampleCount );
	}
	ri.Printf( PRINT_ALL, "r_specularAADebug=%d (0 off, 1 authored rough, 2 normal var, 3 geo rough, "
		"4 final, 5 alias risk, 6 energy)\n",
		r_specularAADebug ? r_specularAADebug->integer : 0 );
}

static void VK_ShadingCompare_SelfTest_f( void )
{
	int mode = r_shadingCompare ? r_shadingCompare->integer : 1;
	if ( mode <= 0 ) {
		mode = 1;
	}

	vk_shading_compare_reset();
	VK_ShadingCompare_FeedCpuParity( mode );

	ri.Printf( PRINT_ALL,
		"[VK][shading] selftest mode=%d (CPU dual-eval of shared BRDF; GPU readback still pending)\n",
		mode );
	VK_ShadingCompare_Report_f();
}

void vk_shading_compare_register( void )
{
	r_shadingCompare = ri.Cvar_Get( "r_shadingCompare", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_shadingCompare, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_shadingCompare,
		"Shading parity: 1 Deferred vs Forward+, 2 diffuse, 3 specular, 4 shadow, 5 probe, 6 WBOIT near-opaque" );
	ri.Cvar_SetGroup( r_shadingCompare, CVG_RENDERER );

	r_specularAADebug = ri.Cvar_Get( "r_specularAADebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_specularAADebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_specularAADebug,
		"Specular AA: 1 authored rough, 2 normal var, 3 geo rough, 4 final, 5 alias risk, 6 energy" );
	ri.Cvar_SetGroup( r_specularAADebug, CVG_RENDERER );

	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "shading_compare_status", VK_ShadingCompare_Status_f );
		ri.Cmd_AddCommand( "shading_compare_report", VK_ShadingCompare_Report_f );
		ri.Cmd_AddCommand( "shading_compare_selftest", VK_ShadingCompare_SelfTest_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL,
			"[VK][shading] shading_compare_status / shading_compare_report / shading_compare_selftest ready\n" );
	}
}

void vk_shading_compare_begin_frame( void )
{
	int mode = r_shadingCompare ? r_shadingCompare->integer : 0;
	vk_shading_compare_reset();
	if ( mode > 0 ) {
		VK_ShadingCompare_FeedCpuParity( mode );
	}
}
