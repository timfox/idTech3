/*
===========================================================================
x3DPRA simulation scenes and ground-truth attenuation (Section V).
===========================================================================
*/

#include "x3dpra/x3dpra.h"

#include <math.h>

void X3dpra_DefaultGrid( x3dpra_grid_t *grid )
{
	if ( !grid ) {
		return;
	}
	grid->nx = X3DPRA_VOXEL_NX;
	grid->ny = X3DPRA_VOXEL_NY;
	grid->nz = X3DPRA_VOXEL_NZ;
	grid->dx = X3DPRA_DOI_X_M / (float)grid->nx;
	grid->dy = X3DPRA_DOI_Y_M / (float)grid->ny;
	grid->dz = X3DPRA_DOI_Z_M / (float)grid->nz;
}

void X3dpra_DefaultOptParams( x3dpra_opt_params_t *opt )
{
	if ( !opt ) {
		return;
	}
	opt->half_loss = 0.5f;
	opt->tv_gamma = 0.02f;
	opt->huber_tau = 0.05f;
	opt->max_iter = 200;
}

void X3dpra_VoxelCenter( const x3dpra_grid_t *grid, int ix, int iy, int iz, x3dpra_vec3_t *out )
{
	if ( !grid || !out ) {
		return;
	}
	out->x = -X3DPRA_DOI_X_M * 0.5f + ( (float)ix + 0.5f ) * grid->dx;
	out->y = -X3DPRA_DOI_Y_M * 0.5f + ( (float)iy + 0.5f ) * grid->dy;
	out->z = -X3DPRA_DOI_Z_M * 0.5f + ( (float)iz + 0.5f ) * grid->dz;
}

int X3dpra_VoxelIndex( int ix, int iy, int iz, int nx, int ny )
{
	return iz * nx * ny + iy * nx + ix;
}

x3dpra_material_t X3dpra_ObjectMaterial( x3dpra_object_kind_t kind )
{
	x3dpra_material_t m;

	switch ( kind ) {
	case X3DPRA_OBJ_SQUARE:
		m.eps_r = 8.0f;
		m.eps_i = 0.8f;
		m.alpha = 14.2f;
		break;
	case X3DPRA_OBJ_TWO_CYLINDERS:
		m.eps_r = 15.0f;
		m.eps_i = 1.5f;
		m.alpha = 19.5f;
		break;
	case X3DPRA_OBJ_CIRCLE:
	default:
		m.eps_r = 10.0f;
		m.eps_i = 1.0f;
		m.alpha = 15.8f;
		break;
	}
	return m;
}

static qboolean x3dpra_in_circle_xy( float x, float y, float cx, float cy, float radius )
{
	const float dx = x - cx;
	const float dy = y - cy;
	return ( dx * dx + dy * dy <= radius * radius ) ? qtrue : qfalse;
}

float X3dpra_VoxelAlphaGroundTruth( const x3dpra_object_t *obj, const x3dpra_vec3_t *r )
{
	if ( !obj || !r ) {
		return 0.0f;
	}

	switch ( obj->kind ) {
	case X3DPRA_OBJ_CIRCLE:
		if ( x3dpra_in_circle_xy( r->x, r->y, 0.0f, 0.0f, 0.15f ) &&
			fabsf( r->z ) <= obj->height_m * 0.5f ) {
			return obj->mat.alpha;
		}
		break;
	case X3DPRA_OBJ_SQUARE:
		if ( fabsf( r->x ) <= 0.18f && fabsf( r->y ) <= 0.18f &&
			fabsf( r->z ) <= obj->height_m * 0.5f ) {
			return obj->mat.alpha;
		}
		break;
	case X3DPRA_OBJ_TWO_CYLINDERS:
		if ( x3dpra_in_circle_xy( r->x, r->y, 0.0f, 0.0f, 0.2f ) ) {
			if ( r->z >= -0.325f && r->z <= -0.075f ) {
				return obj->mat.alpha;
			}
			if ( r->z >= 0.075f && r->z <= 0.325f ) {
				return obj->mat.alpha;
			}
		}
		break;
	default:
		break;
	}
	return 0.0f;
}
