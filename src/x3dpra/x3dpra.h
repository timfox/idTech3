#pragma once

/*
===========================================================================
x3DPRA — 3D Extended Phaseless Rytov Approximation for device-free RF imaging.
Ma et al., arXiv:2606.06933 (ISAC / RSS tomography).
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define X3DPRA_FREQ_GHZ           2.4f
#define X3DPRA_LAMBDA0_M          0.125f
#define X3DPRA_C0_DB                8.685889638f
#define X3DPRA_DOI_X_M              0.9f
#define X3DPRA_DOI_Y_M              0.9f
#define X3DPRA_DOI_Z_M              0.3f
#define X3DPRA_VOXEL_NX             60
#define X3DPRA_VOXEL_NY             60
#define X3DPRA_VOXEL_NZ              15
#define X3DPRA_MAX_LINKS          1200
#define X3DPRA_MAX_NODES            48

typedef struct {
	float x;
	float y;
	float z;
} x3dpra_vec3_t;

typedef struct {
	x3dpra_vec3_t pos;
} x3dpra_node_t;

typedef struct {
	int mt;
	int mr;
	float delta_y_db;
} x3dpra_link_meas_t;

typedef struct {
	float eps_r;
	float eps_i;
	float alpha;
} x3dpra_material_t;

typedef enum {
	X3DPRA_OBJ_CIRCLE = 0,
	X3DPRA_OBJ_SQUARE,
	X3DPRA_OBJ_TWO_CYLINDERS
} x3dpra_object_kind_t;

typedef struct {
	x3dpra_object_kind_t kind;
	x3dpra_material_t mat;
	float height_m;
} x3dpra_object_t;

typedef struct {
	int nx;
	int ny;
	int nz;
	float dx;
	float dy;
	float dz;
} x3dpra_grid_t;

typedef struct {
	float half_loss;
	float tv_gamma;
	float huber_tau;
	int max_iter;
} x3dpra_opt_params_t;

float X3dpra_Wavenumber( void );
float X3dpra_AttenuationFromPermittivity( float eps_r, float eps_i );
void X3dpra_ContrastFromPermittivity( float eps_r, float eps_i, float *re_chi, float *im_chi );
float X3dpra_DipoleHeight( float theta_rad, float R_ohm );

float X3dpra_ScalarGreen3D( float r_mt_n, float k0 );
float X3dpra_KernelPsi( const x3dpra_node_t *tx, const x3dpra_node_t *rx,
	const x3dpra_vec3_t *voxel, float dv, float k0, float c0 );
qboolean X3dpra_FresnelMask( float r_mt_n, float r_mr_n, float r_mt_mr, float delta_d );

float X3dpra_WeightEntry( float psi_im, float k0 );
float X3dpra_AlphaFromImagContrast( float im_delta_chi, float k0 );

void X3dpra_DefaultGrid( x3dpra_grid_t *grid );
void X3dpra_DefaultOptParams( x3dpra_opt_params_t *opt );
void X3dpra_VoxelCenter( const x3dpra_grid_t *grid, int ix, int iy, int iz, x3dpra_vec3_t *out );

x3dpra_material_t X3dpra_ObjectMaterial( x3dpra_object_kind_t kind );
float X3dpra_VoxelAlphaGroundTruth( const x3dpra_object_t *obj, const x3dpra_vec3_t *r );

float X3dpra_LinearObjective( const float *y, const float *W_row, const float *alpha,
	int num_links, int num_voxels );
float X3dpra_HuberNorm3D( float gx, float gy, float gz, float tau );
float X3dpra_TvHuber3D( const float *alpha, int nx, int ny, int nz, float tau );

int X3dpra_VoxelIndex( int ix, int iy, int iz, int nx, int ny );

int X3dpra_LinkCount( int num_nodes );
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
	float c0 );
float X3dpra_ForwardLink(
	const x3dpra_node_t *nodes,
	int num_nodes,
	int link_idx,
	const x3dpra_grid_t *grid,
	float delta_d,
	const float *alpha,
	int num_voxels,
	float k0,
	float c0 );

#ifdef __cplusplus
}
#endif
