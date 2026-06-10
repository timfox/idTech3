/*
===========================================================================
Unit tests for x3DPRA physics and scene models.
===========================================================================
*/

#include "x3dpra/x3dpra.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

static void test_wavenumber( void )
{
	const float k0 = X3dpra_Wavenumber();
	expect_true( k0 > 50.0f && k0 < 51.0f, "k0 ~ 2pi/0.125" );
}

static void test_attenuation_circle_object( void )
{
	x3dpra_material_t m = X3dpra_ObjectMaterial( X3DPRA_OBJ_CIRCLE );
	expect_true( fabsf( m.alpha - 15.8f ) < 0.5f, "circle alpha ~15.8" );
	expect_true( fabsf( X3dpra_AttenuationFromPermittivity( 10.0f, 1.0f ) - m.alpha ) < 1.0f,
		"alpha from permittivity" );
}

static void test_contrast( void )
{
	float re;
	float im;
	X3dpra_ContrastFromPermittivity( 10.0f, 1.0f, &re, &im );
	expect_true( re > 4.0f && re < 5.0f, "Re(chi) for eps=10" );
	expect_true( im > 0.3f && im < 0.35f, "Im(chi) for eps=10+1j" );
}

static void test_ground_truth_voxel( void )
{
	x3dpra_object_t obj;
	x3dpra_vec3_t center;
	x3dpra_vec3_t outside;

	obj.kind = X3DPRA_OBJ_CIRCLE;
	obj.height_m = 0.8f;
	obj.mat = X3dpra_ObjectMaterial( X3DPRA_OBJ_CIRCLE );
	center.x = 0.0f;
	center.y = 0.0f;
	center.z = 0.0f;
	outside.x = 0.4f;
	outside.y = 0.0f;
	outside.z = 0.0f;
	expect_true( X3dpra_VoxelAlphaGroundTruth( &obj, &center ) > 10.0f, "center voxel in cylinder" );
	expect_true( X3dpra_VoxelAlphaGroundTruth( &obj, &outside ) < 1e-3f, "outside voxel empty" );
}

static void test_fresnel_mask( void )
{
	expect_true( X3dpra_FresnelMask( 0.4f, 0.4f, 0.9f, 0.2f ) == qtrue, "on-LOS voxel in ellipse" );
	expect_true( X3dpra_FresnelMask( 0.8f, 0.8f, 0.9f, 0.01f ) == qfalse, "off-LOS voxel masked" );
}

static void test_tv_huber( void )
{
	float vol[8] = { 0, 1, 0, 0, 0, 0, 0, 0 };
	const float tv = X3dpra_TvHuber3D( vol, 2, 2, 2, 0.1f );
	expect_true( tv > 0.0f, "TV huber positive on edge" );
}

static void test_forward_weight_row( void )
{
	x3dpra_node_t nodes[2];
	x3dpra_grid_t grid;
	float weights[64];
	int vidx[64];
	int n;
	const float k0 = X3dpra_Wavenumber();

	X3dpra_DefaultGrid( &grid );
	grid.nx = grid.ny = 4;
	grid.nz = 2;
	grid.dx = X3DPRA_DOI_X_M / (float)grid.nx;
	grid.dy = X3DPRA_DOI_Y_M / (float)grid.ny;
	grid.dz = X3DPRA_DOI_Z_M / (float)grid.nz;

	nodes[0].pos.x = -0.4f;
	nodes[0].pos.y = 0.0f;
	nodes[0].pos.z = 0.0f;
	nodes[1].pos.x = 0.4f;
	nodes[1].pos.y = 0.0f;
	nodes[1].pos.z = 0.0f;

	expect_true( X3dpra_LinkCount( 2 ) == 1, "one link for 2 nodes" );
	n = X3dpra_BuildWeightRow( nodes, 2, 0, &grid, 0.2f, weights, vidx, 64, k0, X3DPRA_C0_DB );
	expect_true( n > 0, "non-empty weight row" );
}

static void test_grid_voxels( void )
{
	x3dpra_grid_t g;
	X3dpra_DefaultGrid( &g );
	expect_true( g.nx * g.ny * g.nz == 54000, "60x60x15 voxels" );
}

int main( void )
{
	test_wavenumber();
	test_attenuation_circle_object();
	test_contrast();
	test_ground_truth_voxel();
	test_fresnel_mask();
	test_tv_huber();
	test_forward_weight_row();
	test_grid_voxels();

	printf( "test_x3dpra: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
