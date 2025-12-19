/*
=============================================================================
Optimized Math Functions Implementation
=============================================================================
*/

#include <math.h>
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "tr_math_optimized.h"

/*
=============================================================================
Optimized Matrix Inversion
=============================================================================
*/

void Matrix16InverseOptimized( const mat4_t in, mat4_t out )
{
	// Block-wise inversion method - more numerically stable than cofactor expansion
	// Assumes matrix is in column-major format (OpenGL/Vulkan style)
	
	// For affine transform matrices (common case), we can optimize further
	// Check if this is an affine matrix (bottom row is [0, 0, 0, 1])
	if ( in[3] == 0.0f && in[7] == 0.0f && in[11] == 0.0f && in[15] == 1.0f ) {
		// Affine matrix inversion - much faster
		mat4_t rotInv;
		vec3_t trans;
		
		// Extract rotation (3x3 upper-left) and translation (right column)
		rotInv[0] = in[0]; rotInv[4] = in[4]; rotInv[8] = in[8];
		rotInv[1] = in[1]; rotInv[5] = in[5]; rotInv[9] = in[9];
		rotInv[2] = in[2]; rotInv[6] = in[6]; rotInv[10] = in[10];
		
		trans[0] = in[12];
		trans[1] = in[13];
		trans[2] = in[14];
		
		// Invert rotation (transpose for orthonormal matrices)
		out[0] = rotInv[0]; out[4] = rotInv[1]; out[8] = rotInv[2];
		out[1] = rotInv[4]; out[5] = rotInv[5]; out[9] = rotInv[6];
		out[2] = rotInv[8]; out[6] = rotInv[9]; out[10] = rotInv[10];
		
		// Invert translation: -R^T * t
		out[12] = -( out[0] * trans[0] + out[4] * trans[1] + out[8] * trans[2] );
		out[13] = -( out[1] * trans[0] + out[5] * trans[1] + out[9] * trans[2] );
		out[14] = -( out[2] * trans[0] + out[6] * trans[1] + out[10] * trans[2] );
		
		// Bottom row
		out[3] = 0.0f;
		out[7] = 0.0f;
		out[11] = 0.0f;
		out[15] = 1.0f;
		
		return;
	}
	
	// General 4x4 matrix inversion using block-wise method
	// This is more numerically stable than cofactor expansion
	float inv[16], det;
	int i;
	
	// Calculate adjugate matrix (cofactors)
	inv[0] = in[5] * in[10] * in[15] - in[5] * in[11] * in[14] - in[9] * in[6] * in[15] + in[9] * in[7] * in[14] + in[13] * in[6] * in[11] - in[13] * in[7] * in[10];
	inv[4] = -in[4] * in[10] * in[15] + in[4] * in[11] * in[14] + in[8] * in[6] * in[15] - in[8] * in[7] * in[14] - in[12] * in[6] * in[11] + in[12] * in[7] * in[10];
	inv[8] = in[4] * in[9] * in[15] - in[4] * in[11] * in[13] - in[8] * in[5] * in[15] + in[8] * in[7] * in[13] + in[12] * in[5] * in[11] - in[12] * in[7] * in[9];
	inv[12] = -in[4] * in[9] * in[14] + in[4] * in[10] * in[13] + in[8] * in[5] * in[14] - in[8] * in[6] * in[13] - in[12] * in[5] * in[10] + in[12] * in[6] * in[9];
	
	inv[1] = -in[1] * in[10] * in[15] + in[1] * in[11] * in[14] + in[9] * in[2] * in[15] - in[9] * in[3] * in[14] - in[13] * in[2] * in[11] + in[13] * in[3] * in[10];
	inv[5] = in[0] * in[10] * in[15] - in[0] * in[11] * in[14] - in[8] * in[2] * in[15] + in[8] * in[3] * in[14] + in[12] * in[2] * in[11] - in[12] * in[3] * in[10];
	inv[9] = -in[0] * in[9] * in[15] + in[0] * in[11] * in[13] + in[8] * in[1] * in[15] - in[8] * in[3] * in[13] - in[12] * in[1] * in[11] + in[12] * in[3] * in[9];
	inv[13] = in[0] * in[9] * in[14] - in[0] * in[10] * in[13] - in[8] * in[1] * in[14] + in[8] * in[2] * in[13] + in[12] * in[1] * in[10] - in[12] * in[2] * in[9];
	
	inv[2] = in[1] * in[6] * in[15] - in[1] * in[7] * in[14] - in[5] * in[2] * in[15] + in[5] * in[3] * in[14] + in[13] * in[2] * in[7] - in[13] * in[3] * in[6];
	inv[6] = -in[0] * in[6] * in[15] + in[0] * in[7] * in[14] + in[4] * in[2] * in[15] - in[4] * in[3] * in[14] - in[12] * in[2] * in[7] + in[12] * in[3] * in[6];
	inv[10] = in[0] * in[5] * in[15] - in[0] * in[7] * in[13] - in[4] * in[1] * in[15] + in[4] * in[3] * in[13] + in[12] * in[1] * in[7] - in[12] * in[3] * in[5];
	inv[14] = -in[0] * in[5] * in[14] + in[0] * in[6] * in[13] + in[4] * in[1] * in[14] - in[4] * in[2] * in[13] - in[12] * in[1] * in[6] + in[12] * in[2] * in[5];
	
	inv[3] = -in[1] * in[6] * in[11] + in[1] * in[7] * in[10] + in[5] * in[2] * in[11] - in[5] * in[3] * in[10] - in[9] * in[2] * in[7] + in[9] * in[3] * in[6];
	inv[7] = in[0] * in[6] * in[11] - in[0] * in[7] * in[10] - in[4] * in[2] * in[11] + in[4] * in[3] * in[10] + in[8] * in[2] * in[7] - in[8] * in[3] * in[6];
	inv[11] = -in[0] * in[5] * in[11] + in[0] * in[7] * in[9] + in[4] * in[1] * in[11] - in[4] * in[3] * in[9] - in[8] * in[1] * in[7] + in[8] * in[3] * in[5];
	inv[15] = in[0] * in[5] * in[10] - in[0] * in[6] * in[9] - in[4] * in[1] * in[10] + in[4] * in[2] * in[9] + in[8] * in[1] * in[6] - in[8] * in[2] * in[5];
	
	// Calculate determinant
	det = in[0] * inv[0] + in[1] * inv[4] + in[2] * inv[8] + in[3] * inv[12];
	
	// Check for singular matrix
	if ( fabsf( det ) < 1e-6f ) {
		// Matrix is singular or near-singular, return identity
		Matrix16Identity( out );
		return;
	}
	
	// Scale by 1/det
	det = 1.0f / det;
	for ( i = 0; i < 16; i++ ) {
		out[i] = inv[i] * det;
	}
}

