/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Compact BubbleSH dataset geometry + metric model (Ramesh et al., arXiv:2607.07275).
===========================================================================
*/

#include "bubblesh_model.h"

#include <math.h>
#include <stdlib.h>

#ifndef BUBBLESH_PI
#define BUBBLESH_PI 3.14159265358979323846f
#endif

static int BubbleSH_CompareFloat( const void *a, const void *b )
{
	const float fa = *(const float *)a;
	const float fb = *(const float *)b;
	if ( fa < fb ) {
		return -1;
	}
	if ( fa > fb ) {
		return 1;
	}
	return 0;
}

static float BubbleSH_ClampPositive( float v )
{
	if ( v <= 1.0e-6f ) {
		return 1.0e-6f;
	}
	return v;
}

static float BubbleSH_PercentileSorted( const float *sorted, int count, float p )
{
	float pos;
	int lo;
	int hi;
	float frac;

	if ( !sorted || count <= 0 ) {
		return 0.0f;
	}
	if ( count == 1 ) {
		return sorted[0];
	}
	if ( p <= 0.0f ) {
		return sorted[0];
	}
	if ( p >= 1.0f ) {
		return sorted[count - 1];
	}

	pos = p * (float)( count - 1 );
	lo = (int)floorf( pos );
	hi = (int)ceilf( pos );
	frac = pos - (float)lo;
	if ( hi <= lo ) {
		return sorted[lo];
	}
	return sorted[lo] * ( 1.0f - frac ) + sorted[hi] * frac;
}

static float BubbleSH_TimeStepForConfig( int diameter_mm, int void_fraction_pct )
{
	if ( diameter_mm == 5 ) {
		return 1.0e-4f;
	}
	if ( void_fraction_pct == 15 || void_fraction_pct == 25 || void_fraction_pct == 35 ) {
		return 1.0e-3f;
	}
	return 1.0e-4f;
}

int BubbleSH_CoefficientCount( int sh_order )
{
	if ( sh_order < 0 ) {
		sh_order = 0;
	}
	return ( sh_order + 1 ) * ( sh_order + 1 );
}

float BubbleSH_DomainSizeMm( int bubble_count, float diameter_mm, float void_fraction_pct )
{
	const float radius_mm = diameter_mm * 0.5f;
	const float void_fraction = BubbleSH_ClampPositive( void_fraction_pct / 100.0f );
	const float bubble_volume = ( 4.0f / 3.0f ) * BUBBLESH_PI * radius_mm * radius_mm * radius_mm;
	const float total_volume = (float)bubble_count * bubble_volume / void_fraction;
	return cbrtf( total_volume );
}

qboolean BubbleSH_FillConfig( int diameter_mm, int void_fraction_pct, int sh_order, bubblesh_config_t *out )
{
	int coeff_count;

	if ( !out ) {
		return qfalse;
	}
	if ( diameter_mm < 4 || diameter_mm > 6 ) {
		return qfalse;
	}
	if ( void_fraction_pct < 5 || void_fraction_pct > 40 || ( void_fraction_pct % 5 ) != 0 ) {
		return qfalse;
	}
	if ( sh_order < 0 ) {
		sh_order = 0;
	}

	coeff_count = BubbleSH_CoefficientCount( sh_order );
	out->diameter_mm = diameter_mm;
	out->void_fraction_pct = void_fraction_pct;
	out->bubble_count = BUBBLESH_BUBBLE_COUNT;
	out->sh_order = sh_order;
	out->sh_coeff_count = coeff_count;
	out->state_dim = 6 + coeff_count;
	out->dt_seconds = BubbleSH_TimeStepForConfig( diameter_mm, void_fraction_pct );
	out->domain_size_mm = BubbleSH_DomainSizeMm( BUBBLESH_BUBBLE_COUNT, (float)diameter_mm, (float)void_fraction_pct );
	out->coord_compression_ratio = (float)( BUBBLESH_MESH_NODES_MIN * 3 ) / (float)coeff_count;
	return qtrue;
}

float BubbleSH_RelativeAverageDisplacementError( float mean_displacement_error, float arc_length )
{
	return mean_displacement_error / BubbleSH_ClampPositive( arc_length );
}

float BubbleSH_RelativeFinalDisplacementError( float final_displacement_error, float arc_length )
{
	return final_displacement_error / BubbleSH_ClampPositive( arc_length );
}

float BubbleSH_RelativeAverageChamferDistance( float mean_chamfer_distance, float total_surface_change )
{
	return mean_chamfer_distance / BubbleSH_ClampPositive( total_surface_change );
}

float BubbleSH_Wasserstein1( const float *predicted, const float *truth, int count )
{
	float *pred_sorted;
	float *truth_sorted;
	float accum;
	int i;

	if ( !predicted || !truth || count <= 0 ) {
		return 0.0f;
	}

	pred_sorted = (float *)malloc( sizeof( float ) * (size_t)count );
	truth_sorted = (float *)malloc( sizeof( float ) * (size_t)count );
	if ( !pred_sorted || !truth_sorted ) {
		free( pred_sorted );
		free( truth_sorted );
		return 0.0f;
	}

	for ( i = 0; i < count; i++ ) {
		pred_sorted[i] = predicted[i];
		truth_sorted[i] = truth[i];
	}
	qsort( pred_sorted, (size_t)count, sizeof( float ), BubbleSH_CompareFloat );
	qsort( truth_sorted, (size_t)count, sizeof( float ), BubbleSH_CompareFloat );

	accum = 0.0f;
	for ( i = 0; i < count; i++ ) {
		accum += fabsf( pred_sorted[i] - truth_sorted[i] );
	}

	free( pred_sorted );
	free( truth_sorted );
	return accum / (float)count;
}

float BubbleSH_InterquartileRange( const float *values, int count )
{
	float *sorted;
	float q1;
	float q3;
	int i;

	if ( !values || count <= 0 ) {
		return 0.0f;
	}

	sorted = (float *)malloc( sizeof( float ) * (size_t)count );
	if ( !sorted ) {
		return 0.0f;
	}
	for ( i = 0; i < count; i++ ) {
		sorted[i] = values[i];
	}
	qsort( sorted, (size_t)count, sizeof( float ), BubbleSH_CompareFloat );
	q1 = BubbleSH_PercentileSorted( sorted, count, 0.25f );
	q3 = BubbleSH_PercentileSorted( sorted, count, 0.75f );
	free( sorted );
	return q3 - q1;
}

float BubbleSH_NormalizedWasserstein1( const float *predicted, const float *truth, int count )
{
	const float raw_w1 = BubbleSH_Wasserstein1( predicted, truth, count );
	const float iqr = BubbleSH_InterquartileRange( truth, count );
	return raw_w1 / BubbleSH_ClampPositive( iqr );
}
