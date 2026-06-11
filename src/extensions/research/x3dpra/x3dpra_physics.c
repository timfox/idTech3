/*
===========================================================================
x3DPRA electromagnetic physics: attenuation, contrast, Green's function, kernel.
Unified: Im(G) weights, C0 scaling, no ad-hoc path loss.
===========================================================================
*/

#include "x3dpra/x3dpra.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define X3DPRA_ZETA0 376.730313668f
#define X3DPRA_DIPOLE_R 73.0f

float X3dpra_Wavenumber( void )
{
	return 2.0f * (float)M_PI / X3DPRA_LAMBDA0_M;
}

float X3dpra_AttenuationFromPermittivity( float eps_r, float eps_i )
{
	const float delta = eps_i / eps_r;
	return 2.0f * (float)M_PI * delta * sqrtf( eps_r ) / X3DPRA_LAMBDA0_M;
}

void X3dpra_ContrastFromPermittivity( float eps_r, float eps_i, float *re_chi, float *im_chi )
{
	const float srt = sqrtf( eps_r );
	if ( re_chi ) {
		*re_chi = 2.0f * ( srt - 1.0f );
	}
	if ( im_chi ) {
		*im_chi = eps_i / srt;
	}
}

float X3dpra_DipoleHeight( float theta_rad, float R_ohm )
{
	const float scale = X3DPRA_LAMBDA0_M * sqrtf( R_ohm / ( (float)M_PI * X3DPRA_ZETA0 ) );
	return scale * sinf( theta_rad );
}

float X3dpra_ScalarGreen3D( float dist, float k0 )
{
	/* Im(G) = sin(kr)/(4*pi*r) — real part available via cos if needed. */
	if ( dist < 1e-6f ) {
		return 0.0f;
	}
	return sinf( k0 * dist ) / ( 4.0f * (float)M_PI * dist );
}

static float x3dpra_incident_field( const x3dpra_node_t *tx, const x3dpra_vec3_t *r, float k0 )
{
	const float dx = r->x - tx->pos.x;
	const float dy = r->y - tx->pos.y;
	const float dz = r->z - tx->pos.z;
	const float dist = sqrtf( dx * dx + dy * dy + dz * dz );
	float theta;

	(void)k0;
	if ( dist < 1e-6f ) {
		return 1.0f;
	}
	theta = acosf( dz / dist );
	return ( 1.0f / dist ) * X3dpra_DipoleHeight( theta, X3DPRA_DIPOLE_R );
}

static float x3dpra_theta_at( const x3dpra_node_t *rx, const x3dpra_vec3_t *from )
{
	const float dx = from->x - rx->pos.x;
	const float dy = from->y - rx->pos.y;
	const float dz = from->z - rx->pos.z;
	const float dist = sqrtf( dx * dx + dy * dy + dz * dz );
	if ( dist < 1e-6f ) {
		return (float)M_PI * 0.5f;
	}
	return acosf( dz / dist );
}

float X3dpra_KernelPsi( const x3dpra_node_t *tx, const x3dpra_node_t *rx,
	const x3dpra_vec3_t *voxel, float dv, float k0, float c0 )
{
	const float ei_rx = x3dpra_incident_field( tx, &rx->pos, k0 );
	const float ei_vox = x3dpra_incident_field( tx, voxel, k0 );
	const float dx = rx->pos.x - voxel->x;
	const float dy = rx->pos.y - voxel->y;
	const float dz = rx->pos.z - voxel->z;
	const float dist = sqrtf( dx * dx + dy * dy + dz * dz );
	const float g_im = X3dpra_ScalarGreen3D( dist, k0 );
	const float h_rx = X3dpra_DipoleHeight( x3dpra_theta_at( rx, &tx->pos ), X3DPRA_DIPOLE_R );
	const float h_vox = X3dpra_DipoleHeight( x3dpra_theta_at( rx, voxel ), X3DPRA_DIPOLE_R );
	const float psi_im = h_vox * g_im * ei_vox * dv / ( h_rx * ei_rx + 1e-12f );

	(void)c0;
	return psi_im;
}

qboolean X3dpra_FresnelMask( float r_mt_n, float r_mr_n, float r_mt_mr, float delta_d )
{
	return ( r_mt_n + r_mr_n < r_mt_mr + delta_d ) ? qtrue : qfalse;
}

float X3dpra_WeightEntry( float psi_im, float k0 )
{
	/* W_ln = C0 * k0^2 * Im(psi) / k0 = C0 * k0 * Im(psi) */
	return X3DPRA_C0_DB * k0 * psi_im;
}

float X3dpra_AlphaFromImagContrast( float im_delta_chi, float k0 )
{
	return -k0 * im_delta_chi;
}
