/*
===========================================================================
Unit tests for VGS scaffold (McGraw MIG 2024) + Alg. 1 projector.
===========================================================================
*/

#include "vgs/vgs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int tests_run;
static int tests_failed;

static void expect_true( int cond, const char *msg )
{
	tests_run++;
	if ( !cond ) {
		tests_failed++;
		fprintf( stderr, "FAIL: %s\n", msg );
	}
}

static void fill_unit_cube( float *p )
{
	/* Fig. 2 z-order: bit0=x, bit1=y, bit2=z on [0,1]^3 */
	int i;
	for ( i = 0; i < 8; i++ ) {
		p[i * 3 + 0] = (float)( i & 1 );
		p[i * 3 + 1] = (float)( ( i >> 1 ) & 1 );
		p[i * 3 + 2] = (float)( ( i >> 2 ) & 1 );
	}
}

static void test_constants( void )
{
	expect_true( fabsf( Vgs_DefaultAlpha() - 0.5f ) < 1e-6f, "alpha 0.5" );
	expect_true( Vgs_DefaultIters() == 3, "vgs_it 3" );
	expect_true( Vgs_FacePartitionCount() == 3, "face partitions 3" );
	expect_true( Vgs_VgsPartitionCount() == 1, "VGS partitions 1" );
	expect_true( Vgs_FaceConstraintBytes() == 8, "face 8 bytes" );
}

static void test_stages( void )
{
	expect_true( Vgs_StageCount() == 6, "6 stages" );
	expect_true( Vgs_GetStage( 1 ) && strstr( Vgs_GetStage( 1 )->name, "vgs" ), "vgs stage" );
}

static void test_gaps( void )
{
	int i;
	int softblobPartialOrAbsent = 0;

	expect_true( Vgs_GapCount() == 5, "5 gaps" );
	for ( i = 0; i < Vgs_GapCount(); i++ ) {
		const vgs_gap_t *g = Vgs_GetGap( i );
		if ( !g ) {
			continue;
		}
		if ( strstr( g->feature, "VGS" ) &&
			 ( g->status == VGS_STATUS_ABSENT || g->status == VGS_STATUS_PARTIAL ) ) {
			softblobPartialOrAbsent = 1;
		}
	}
	expect_true( softblobPartialOrAbsent, "VGS marked absent/partial vs softblob" );
}

static void test_advice( void )
{
	const char *a = Vgs_SelectAdvice( "soft" );
	const char *b = Vgs_SelectAdvice( "fracture" );
	expect_true( a && strlen( a ) > 20, "soft advice" );
	expect_true( b && strlen( b ) > 20, "fracture advice" );
	expect_true( Vgs_PaperCite() && strstr( Vgs_PaperCite(), "3677388" ), "cite DOI" );
}

static void test_project_sheared( void )
{
	float p[24];
	float w[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
	float v0;
	float vol;
	float e0[3], e1[3], e2[3];
	float d01, d02, d12;
	float n0, n1, n2;
	int rc;

	fill_unit_cube( p );
	v0 = Vgs_ParallelepipedVolume( p );
	expect_true( fabsf( v0 - 1.0f ) < 1e-4f, "unit cube volume 1" );

	/* Mild shear: push +x on top face (z=1) */
	p[12] += 0.15f; /* corner 4 */
	p[15] += 0.15f; /* corner 5 */
	p[18] += 0.15f; /* corner 6 */
	p[21] += 0.15f; /* corner 7 */

	rc = Vgs_ProjectVoxel( 0.5f, 1.0f, 3, p, w, 0.25f, v0 );
	expect_true( rc == 0, "project ok" );

	vol = Vgs_ParallelepipedVolume( p );
	expect_true( fabsf( vol - v0 ) < 0.05f, "volume restored ≈ V0" );

	/* Edge directions from corner 0 */
	e0[0] = p[3] - p[0];
	e0[1] = p[4] - p[1];
	e0[2] = p[5] - p[2];
	e1[0] = p[6] - p[0];
	e1[1] = p[7] - p[1];
	e1[2] = p[8] - p[2];
	e2[0] = p[12] - p[0];
	e2[1] = p[13] - p[1];
	e2[2] = p[14] - p[2];
	n0 = sqrtf( e0[0] * e0[0] + e0[1] * e0[1] + e0[2] * e0[2] );
	n1 = sqrtf( e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2] );
	n2 = sqrtf( e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2] );
	expect_true( n0 > 1e-4f && n1 > 1e-4f && n2 > 1e-4f, "edges nonzero" );
	d01 = ( e0[0] * e1[0] + e0[1] * e1[1] + e0[2] * e1[2] ) / ( n0 * n1 );
	d02 = ( e0[0] * e2[0] + e0[1] * e2[1] + e0[2] * e2[2] ) / ( n0 * n2 );
	d12 = ( e1[0] * e2[0] + e1[1] * e2[1] + e1[2] * e2[2] ) / ( n1 * n2 );
	expect_true( fabsf( d01 ) < 0.15f, "e0·e1 near orthogonal" );
	expect_true( fabsf( d02 ) < 0.15f, "e0·e2 near orthogonal" );
	expect_true( fabsf( d12 ) < 0.15f, "e1·e2 near orthogonal" );
}

int main( void )
{
	test_constants();
	test_stages();
	test_gaps();
	test_advice();
	test_project_sheared();

	printf( "unit_vgs: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? 1 : 0;
}
