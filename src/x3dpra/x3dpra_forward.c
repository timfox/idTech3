/*
===========================================================================
x3DPRA forward model: sparse weight rows and link enumeration.
===========================================================================
*/

#include "x3dpra/x3dpra.h"

#include <math.h>
#include <string.h>

int X3dpra_LinkCount( int num_nodes )
{
	if ( num_nodes <= 1 ) {
		return 0;
	}
	return num_nodes * ( num_nodes - 1 ) / 2;
}

static void x3dpra_link_endpoints( int link_idx, int num_nodes, int *mt, int *mr )
{
	int k = 0;
	int i;
	int j;

	for ( i = 0; i < num_nodes; i++ ) {
		for ( j = i + 1; j < num_nodes; j++ ) {
			if ( k == link_idx ) {
				*mt = i;
				*mr = j;
				return;
			}
			k++;
		}
	}
	*mt = 0;
	*mr = 0;
}

int X3dpra_BuildWeightRow(
	const x3dpra_node_t *nodes,
	int num_nodes,
	int link_idx,
	const x3dpra_grid_t *grid,
	float delta_d,
	float *out_weights,
	int *out_voxel_idx,
	int max_entries,
	float k0,
	float c0 )
{
	int mt;
	int mr;
	const x3dpra_node_t *tx;
	const x3dpra_node_t *rx;
	float r_link;
	int ix;
	int iy;
	int iz;
	int count = 0;
	const float dv = grid->dx * grid->dy * grid->dz;

	if ( !nodes || !grid || !out_weights || !out_voxel_idx || max_entries <= 0 ) {
		return 0;
	}
	if ( link_idx < 0 || link_idx >= X3dpra_LinkCount( num_nodes ) ) {
		return 0;
	}

	x3dpra_link_endpoints( link_idx, num_nodes, &mt, &mr );
	tx = &nodes[mt];
	rx = &nodes[mr];
	r_link = sqrtf(
		( rx->pos.x - tx->pos.x ) * ( rx->pos.x - tx->pos.x ) +
		( rx->pos.y - tx->pos.y ) * ( rx->pos.y - tx->pos.y ) +
		( rx->pos.z - tx->pos.z ) * ( rx->pos.z - tx->pos.z ) );

	for ( iz = 0; iz < grid->nz; iz++ ) {
		for ( iy = 0; iy < grid->ny; iy++ ) {
			for ( ix = 0; ix < grid->nx; ix++ ) {
				x3dpra_vec3_t vox;
				float r_mt_n;
				float r_mr_n;
				float psi;
				float w;

				X3dpra_VoxelCenter( grid, ix, iy, iz, &vox );
				r_mt_n = sqrtf(
					( vox.x - tx->pos.x ) * ( vox.x - tx->pos.x ) +
					( vox.y - tx->pos.y ) * ( vox.y - tx->pos.y ) +
					( vox.z - tx->pos.z ) * ( vox.z - tx->pos.z ) );
				r_mr_n = sqrtf(
					( vox.x - rx->pos.x ) * ( vox.x - rx->pos.x ) +
					( vox.y - rx->pos.y ) * ( vox.y - rx->pos.y ) +
					( vox.z - rx->pos.z ) * ( vox.z - rx->pos.z ) );

				if ( !X3dpra_FresnelMask( r_mt_n, r_mr_n, r_link, delta_d ) ) {
					continue;
				}

				psi = X3dpra_KernelPsi( tx, rx, &vox, dv, k0, c0 );
				w = X3dpra_WeightEntry( psi, k0 );
				if ( fabsf( w ) <= 1e-12f ) {
					continue;
				}
				if ( count >= max_entries ) {
					return count;
				}
				out_voxel_idx[count] = X3dpra_VoxelIndex( ix, iy, iz, grid->nx, grid->ny );
				out_weights[count] = w;
				count++;
			}
		}
	}
	return count;
}

float X3dpra_ForwardLink(
	const x3dpra_node_t *nodes,
	int num_nodes,
	int link_idx,
	const x3dpra_grid_t *grid,
	float delta_d,
	const float *alpha,
	int num_voxels,
	float k0,
	float c0 )
{
	float weights[256];
	int vidx[256];
	int n;
	int i;
	float sum = 0.0f;

	n = X3dpra_BuildWeightRow( nodes, num_nodes, link_idx, grid, delta_d,
		weights, vidx, 256, k0, c0 );
	for ( i = 0; i < n; i++ ) {
		if ( vidx[i] >= 0 && vidx[i] < num_voxels ) {
			sum += weights[i] * alpha[vidx[i]];
		}
	}
	return sum;
}
