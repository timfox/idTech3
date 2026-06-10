/*
===========================================================================
x3DPRA optimization helpers: data fidelity + 3D Huber TV (TVReg-style).
===========================================================================
*/

#include "x3dpra/x3dpra.h"

#include <math.h>

float X3dpra_LinearObjective( const float *y, const float *W_row, const float *alpha,
	int num_links, int num_voxels )
{
	int l;
	int n;
	float sum = 0.0f;

	if ( !y || !W_row || !alpha || num_links <= 0 || num_voxels <= 0 ) {
		return 0.0f;
	}

	for ( l = 0; l < num_links; l++ ) {
		float pred = 0.0f;
		const float *row = &W_row[l * num_voxels];
		for ( n = 0; n < num_voxels; n++ ) {
			pred += row[n] * alpha[n];
		}
		{
			const float d = y[l] - pred;
			sum += d * d;
		}
	}
	return 0.5f * sum;
}

float X3dpra_HuberNorm3D( float gx, float gy, float gz, float tau )
{
	const float n2 = sqrtf( gx * gx + gy * gy + gz * gz );
	if ( n2 >= tau ) {
		return n2 - 0.5f * tau;
	}
	return ( n2 * n2 ) / ( 2.0f * tau );
}

float X3dpra_TvHuber3D( const float *alpha, int nx, int ny, int nz, float tau )
{
	int ix;
	int iy;
	int iz;
	float tv = 0.0f;

	if ( !alpha || nx < 2 || ny < 2 || nz < 2 ) {
		return 0.0f;
	}

	for ( iz = 0; iz < nz; iz++ ) {
		for ( iy = 0; iy < ny; iy++ ) {
			for ( ix = 0; ix < nx; ix++ ) {
				const int idx = X3dpra_VoxelIndex( ix, iy, iz, nx, ny );
				if ( ix + 1 < nx ) {
					const int j = X3dpra_VoxelIndex( ix + 1, iy, iz, nx, ny );
					tv += X3dpra_HuberNorm3D( alpha[j] - alpha[idx], 0.0f, 0.0f, tau );
				}
				if ( iy + 1 < ny ) {
					const int j = X3dpra_VoxelIndex( ix, iy + 1, iz, nx, ny );
					tv += X3dpra_HuberNorm3D( 0.0f, alpha[j] - alpha[idx], 0.0f, tau );
				}
				if ( iz + 1 < nz ) {
					const int j = X3dpra_VoxelIndex( ix, iy, iz + 1, nx, ny );
					tv += X3dpra_HuberNorm3D( 0.0f, 0.0f, alpha[j] - alpha[idx], tau );
				}
			}
		}
	}
	return tv;
}
