/*
===========================================================================
x3DPRA console commands and Python pipeline launcher.
===========================================================================
*/

#include "x3dpra/x3dpra_console.h"
#include "x3dpra/x3dpra.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_x3dpra_enable;
static cvar_t *cl_x3dpra_repo;
static cvar_t *cl_x3dpra_python;
static cvar_t *cl_x3dpra_fresnel_delta;
static cvar_t *cl_x3dpra_tv_gamma;
static cvar_t *cl_x3dpra_max_iter;
static qboolean x3dpra_console_registered = qfalse;

static void X3dpra_Cmd_Info_f( void )
{
	const float k0 = X3dpra_Wavenumber();
	x3dpra_grid_t grid;

	X3dpra_DefaultGrid( &grid );
	Com_Printf( "[x3DPRA] 3D Extended Phaseless Rytov Approximation (arXiv:2606.06933)\n" );
	Com_Printf( "[x3DPRA] %.1f GHz | lambda0=%.3f m | k0=%.2f rad/m\n",
		X3DPRA_FREQ_GHZ, X3DPRA_LAMBDA0_M, k0 );
	Com_Printf( "[x3DPRA] DOI %.1f x %.1f x %.1f m | grid %dx%dx%d\n",
		X3DPRA_DOI_X_M, X3DPRA_DOI_Y_M, X3DPRA_DOI_Z_M,
		grid.nx, grid.ny, grid.nz );
	Com_Printf( "[x3DPRA] phaseless RSS | background subtraction | TVReg 3D\n" );
	Com_Printf( "[x3DPRA] cvars: cl_x3dpra_enable cl_x3dpra_repo cl_x3dpra_python "
		"cl_x3dpra_fresnel_delta cl_x3dpra_tv_gamma cl_x3dpra_max_iter\n" );
	Com_Printf( "[x3DPRA] commands: x3dpra_info x3dpra_scene x3dpra_kernel_test x3dpra_reconstruct\n" );
}

static void X3dpra_Cmd_Scene_f( void )
{
	x3dpra_object_t objs[3];
	int i;

	if ( !cl_x3dpra_enable || !cl_x3dpra_enable->integer ) {
		Com_Printf( "[x3DPRA] disabled (cl_x3dpra_enable 0)\n" );
		return;
	}

	objs[0].kind = X3DPRA_OBJ_CIRCLE;
	objs[0].height_m = 0.8f;
	objs[0].mat = X3dpra_ObjectMaterial( X3DPRA_OBJ_CIRCLE );
	objs[1].kind = X3DPRA_OBJ_SQUARE;
	objs[1].height_m = 0.8f;
	objs[1].mat = X3dpra_ObjectMaterial( X3DPRA_OBJ_SQUARE );
	objs[2].kind = X3DPRA_OBJ_TWO_CYLINDERS;
	objs[2].height_m = 0.25f;
	objs[2].mat = X3dpra_ObjectMaterial( X3DPRA_OBJ_TWO_CYLINDERS );

	Com_Printf( "[x3DPRA] Section V simulation objects:\n" );
	for ( i = 0; i < 3; i++ ) {
		Com_Printf( "  [%d] kind=%d eps=%.1f+%.1fj alpha=%.1f height=%.2f m\n",
			i, (int)objs[i].kind, objs[i].mat.eps_r, objs[i].mat.eps_i,
			objs[i].mat.alpha, objs[i].height_m );
	}
}

static void X3dpra_Cmd_KernelTest_f( void )
{
	x3dpra_node_t tx;
	x3dpra_node_t rx;
	x3dpra_vec3_t vox;
	const float k0 = X3dpra_Wavenumber();
	const float dv = 0.015f * 0.015f * 0.02f;
	float psi;
	float w;
	qboolean mask;

	tx.pos.x = -0.45f;
	tx.pos.y = 0.0f;
	tx.pos.z = 0.0f;
	rx.pos.x = 0.45f;
	rx.pos.y = 0.0f;
	rx.pos.z = 0.0f;
	vox.x = 0.0f;
	vox.y = 0.0f;
	vox.z = 0.0f;

	psi = X3dpra_KernelPsi( &tx, &rx, &vox, dv, k0, X3DPRA_C0_DB );
	w = X3dpra_WeightEntry( psi, k0 );
	mask = X3dpra_FresnelMask( 0.45f, 0.45f, 0.9f, cl_x3dpra_fresnel_delta ? cl_x3dpra_fresnel_delta->value : 0.2f );

	Com_Printf( "[x3DPRA] kernel test: Im(psi)=%.6e weight=%.6e fresnel=%s\n",
		psi, w, mask ? "inside" : "outside" );
}

static void X3dpra_RunPython( const char *script, int first_arg )
{
	char cmd[1024];
	const char *repo;
	const char *py;
	int argc = Cmd_Argc();
	int i;
	int pos;

	if ( !cl_x3dpra_enable || !cl_x3dpra_enable->integer ) {
		Com_Printf( "[x3DPRA] disabled (cl_x3dpra_enable 0)\n" );
		return;
	}

	repo = ( cl_x3dpra_repo && cl_x3dpra_repo->string[0] ) ? cl_x3dpra_repo->string : ".";
	py = ( cl_x3dpra_python && cl_x3dpra_python->string[0] ) ? cl_x3dpra_python->string : "python3";

	pos = Com_sprintf( cmd, sizeof( cmd ), "cd \"%s\" && \"%s\" tools/x3dpra/%s",
		repo, py, script );
	if ( pos <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "[x3DPRA] command too long\n" );
		return;
	}
	for ( i = first_arg; i < argc; i++ ) {
		pos = (int)strlen( cmd );
		if ( pos >= (int)sizeof( cmd ) - 2 ) {
			break;
		}
		Q_strcat( cmd, sizeof( cmd ), " \"" );
		Q_strcat( cmd, sizeof( cmd ), Cmd_Argv( i ) );
		Q_strcat( cmd, sizeof( cmd ), "\"" );
	}

	if ( cl_x3dpra_fresnel_delta ) {
		Q_strcat( cmd, sizeof( cmd ), va( " --fresnel-delta %g", cl_x3dpra_fresnel_delta->value ) );
	}
	if ( cl_x3dpra_tv_gamma ) {
		Q_strcat( cmd, sizeof( cmd ), va( " --tv-gamma %g", cl_x3dpra_tv_gamma->value ) );
	}
	if ( cl_x3dpra_max_iter ) {
		Q_strcat( cmd, sizeof( cmd ), va( " --max-iter %d", cl_x3dpra_max_iter->integer ) );
	}

	pos = (int)strlen( cmd );
	if ( pos <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "[x3DPRA] command too long\n" );
		return;
	}

	Com_Printf( "[x3DPRA] %s\n", cmd );
	if ( system( cmd ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "[x3DPRA] python pipeline failed\n" );
	}
}

static void X3dpra_Cmd_Reconstruct_f( void )
{
	X3dpra_RunPython( "reconstruct.py", 1 );
}

void X3dpra_ConsoleInit( void )
{
	if ( x3dpra_console_registered ) {
		return;
	}

	cl_x3dpra_enable = Cvar_Get( "cl_x3dpra_enable", "1", CVAR_ARCHIVE );
	cl_x3dpra_repo = Cvar_Get( "cl_x3dpra_repo", "", CVAR_ARCHIVE );
	cl_x3dpra_python = Cvar_Get( "cl_x3dpra_python", "python3", CVAR_ARCHIVE );
	cl_x3dpra_fresnel_delta = Cvar_Get( "cl_x3dpra_fresnel_delta", "0.2", CVAR_ARCHIVE );
	cl_x3dpra_tv_gamma = Cvar_Get( "cl_x3dpra_tv_gamma", "0.02", CVAR_ARCHIVE );
	cl_x3dpra_max_iter = Cvar_Get( "cl_x3dpra_max_iter", "150", CVAR_ARCHIVE );

	Cvar_SetDescription( cl_x3dpra_enable,
		"Enable x3DPRA device-free RF imaging console tools." );
	Cvar_SetDescription( cl_x3dpra_repo,
		"Repo root for tools/x3dpra Python reconstruction pipeline." );
	Cvar_SetDescription( cl_x3dpra_python,
		"Python interpreter for x3dpra reconstruct/simulate scripts." );
	Cvar_SetDescription( cl_x3dpra_fresnel_delta,
		"Fresnel ellipse width delta_d (m) for sparse weight mask (Eq. 26)." );
	Cvar_SetDescription( cl_x3dpra_tv_gamma,
		"TV regularization weight gamma for Python reconstruct.py." );
	Cvar_SetDescription( cl_x3dpra_max_iter,
		"Maximum iterations for Python TVReg/TVAL3 solvers." );

	Cmd_AddCommand( "x3dpra_info", X3dpra_Cmd_Info_f );
	Cmd_AddCommand( "x3dpra_scene", X3dpra_Cmd_Scene_f );
	Cmd_AddCommand( "x3dpra_kernel_test", X3dpra_Cmd_KernelTest_f );
	Cmd_AddCommand( "x3dpra_reconstruct", X3dpra_Cmd_Reconstruct_f );

	x3dpra_console_registered = qtrue;

	if ( cl_x3dpra_enable->integer ) {
		Com_Printf( "[x3DPRA] enabled (2.4 GHz RSS tomography, DOI %.1f m cube slice)\n",
			X3DPRA_DOI_X_M );
	}
}
