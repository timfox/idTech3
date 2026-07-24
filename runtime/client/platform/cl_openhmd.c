/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OpenHMD VR: runtime-loaded head tracking and software stereo for HMDs.
===========================================================================
*/

#include "client.h"
#include "cl_openhmd.h"

#ifdef USE_OPENHMD

#include <dlfcn.h>
#include <string.h>
#include <math.h>

#include "openhmd.h"

typedef ohmd_context *(*pfn_ohmd_ctx_create)( void );
typedef void ( *pfn_ohmd_ctx_destroy )( ohmd_context *ctx );
typedef const char *( *pfn_ohmd_ctx_get_error )( ohmd_context *ctx );
typedef void ( *pfn_ohmd_ctx_update )( ohmd_context *ctx );
typedef int ( *pfn_ohmd_ctx_probe )( ohmd_context *ctx );
typedef const char *( *pfn_ohmd_list_gets )( ohmd_context *ctx, int index, ohmd_string_value type );
typedef int ( *pfn_ohmd_list_geti )( ohmd_context *ctx, int index, ohmd_int_value type, int *out );
typedef ohmd_device *( *pfn_ohmd_list_open_device )( ohmd_context *ctx, int index );
typedef int ( *pfn_ohmd_close_device )( ohmd_device *device );
typedef int ( *pfn_ohmd_device_getf )( ohmd_device *device, ohmd_float_value type, float *out );
typedef int ( *pfn_ohmd_device_setf )( ohmd_device *device, ohmd_float_value type, const float *in );
typedef void ( *pfn_ohmd_get_version )( int *major, int *minor, int *patch );

static struct {
	void *lib;
	pfn_ohmd_ctx_create ctx_create;
	pfn_ohmd_ctx_destroy ctx_destroy;
	pfn_ohmd_ctx_get_error ctx_get_error;
	pfn_ohmd_ctx_update ctx_update;
	pfn_ohmd_ctx_probe ctx_probe;
	pfn_ohmd_list_gets list_gets;
	pfn_ohmd_list_geti list_geti;
	pfn_ohmd_list_open_device list_open_device;
	pfn_ohmd_close_device close_device;
	pfn_ohmd_device_getf device_getf;
	pfn_ohmd_device_setf device_setf;
	pfn_ohmd_get_version get_version;
} ohmdApi;

static ohmd_context *s_ctx;
static ohmd_device *s_device;
static qboolean s_libOk;
static qboolean s_active;
static int s_probeCount;
static float s_ipd = 0.063f;
static float s_fov = 90.0f;
static vec3_t s_angles;
static vec3_t s_recenterDelta;
static char s_vendor[OHMD_STR_SIZE];
static char s_product[OHMD_STR_SIZE];

static cvar_t *vr_openhmd;
static cvar_t *vr_openhmdDevice;
static cvar_t *vr_openhmdStereo;
static cvar_t *vr_openhmdMouse;
static cvar_t *vr_openhmdIpd;
static cvar_t *vr_openhmdWorldScale;
static cvar_t *vr_openhmdFov;
static cvar_t *vr_openhmdRoll;

#define OHMD_LOAD( name ) \
	do { \
		void *_sym = dlsym( ohmdApi.lib, "ohmd_" #name ); \
		if ( !_sym ) { \
			Com_Printf( "OpenHMD: missing symbol ohmd_%s (%s)\n", #name, dlerror() ); \
			return qfalse; \
		} \
		memcpy( &ohmdApi.name, &_sym, sizeof( ohmdApi.name ) ); \
	} while ( 0 )

static qboolean OHMD_LoadLibrary( void )
{
	const char *candidates[] = {
		"libopenhmd.so.0",
		"libopenhmd.so",
		NULL
	};
	int i;

	if ( ohmdApi.lib ) {
		return qtrue;
	}

	for ( i = 0; candidates[i]; i++ ) {
		ohmdApi.lib = dlopen( candidates[i], RTLD_NOW | RTLD_LOCAL );
		if ( ohmdApi.lib ) {
			Com_Printf( "OpenHMD: loaded %s\n", candidates[i] );
			break;
		}
	}
	if ( !ohmdApi.lib ) {
		Com_Printf( "OpenHMD: libopenhmd not found (install libopenhmd0). VR disabled.\n" );
		return qfalse;
	}

	OHMD_LOAD( ctx_create );
	OHMD_LOAD( ctx_destroy );
	OHMD_LOAD( ctx_get_error );
	OHMD_LOAD( ctx_update );
	OHMD_LOAD( ctx_probe );
	OHMD_LOAD( list_gets );
	OHMD_LOAD( list_geti );
	OHMD_LOAD( list_open_device );
	OHMD_LOAD( close_device );
	OHMD_LOAD( device_getf );
	OHMD_LOAD( device_setf );
	{
		void *sym = dlsym( ohmdApi.lib, "ohmd_get_version" );
		if ( sym ) {
			memcpy( &ohmdApi.get_version, &sym, sizeof( ohmdApi.get_version ) );
		}
	}

	s_libOk = qtrue;
	return qtrue;
}

static void OHMD_QuatToAngles( const float q[4], vec3_t angles )
{
	const float x = q[0], y = q[1], z = q[2], w = q[3];
	float sinr_cosp, cosr_cosp, sinp, siny_cosp, cosy_cosp;
	float pitch, yaw, roll;

	sinr_cosp = 2.0f * ( w * x + y * z );
	cosr_cosp = 1.0f - 2.0f * ( x * x + y * y );
	roll = atan2f( sinr_cosp, cosr_cosp );

	sinp = 2.0f * ( w * y - z * x );
	if ( fabsf( sinp ) >= 1.0f ) {
		pitch = copysignf( (float)M_PI / 2.0f, sinp );
	} else {
		pitch = asinf( sinp );
	}

	siny_cosp = 2.0f * ( w * z + x * y );
	cosy_cosp = 1.0f - 2.0f * ( y * y + z * z );
	yaw = atan2f( siny_cosp, cosy_cosp );

	/* OpenHMD Y-up → Quake Z-up view angles (degrees). */
	angles[PITCH] = -pitch * ( 180.0f / (float)M_PI );
	angles[YAW] = yaw * ( 180.0f / (float)M_PI );
	angles[ROLL] = vr_openhmdRoll && vr_openhmdRoll->integer
		? ( -roll * ( 180.0f / (float)M_PI ) )
		: 0.0f;
}

static void OHMD_CloseDevice( void )
{
	if ( s_device && ohmdApi.close_device ) {
		ohmdApi.close_device( s_device );
	}
	s_device = NULL;
	s_active = qfalse;
	s_vendor[0] = '\0';
	s_product[0] = '\0';
}

static qboolean OHMD_OpenDeviceIndex( int index )
{
	float ipd;
	float fov;
	const char *vendor;
	const char *product;

	if ( !s_ctx || !s_libOk ) {
		return qfalse;
	}
	OHMD_CloseDevice();

	if ( index < 0 || index >= s_probeCount ) {
		Com_Printf( "OpenHMD: device index %d out of range (0..%d)\n", index, s_probeCount - 1 );
		return qfalse;
	}

	s_device = ohmdApi.list_open_device( s_ctx, index );
	if ( !s_device ) {
		Com_Printf( "OpenHMD: failed to open device %d: %s\n", index,
			ohmdApi.ctx_get_error( s_ctx ) );
		return qfalse;
	}

	vendor = ohmdApi.list_gets( s_ctx, index, OHMD_VENDOR );
	product = ohmdApi.list_gets( s_ctx, index, OHMD_PRODUCT );
	Q_strncpyz( s_vendor, vendor ? vendor : "?", sizeof( s_vendor ) );
	Q_strncpyz( s_product, product ? product : "?", sizeof( s_product ) );

	if ( ohmdApi.device_getf( s_device, OHMD_EYE_IPD, &ipd ) == 0 && ipd > 0.01f ) {
		s_ipd = ipd;
	}
	if ( vr_openhmdIpd && vr_openhmdIpd->value > 0.01f ) {
		s_ipd = vr_openhmdIpd->value;
		ohmdApi.device_setf( s_device, OHMD_EYE_IPD, &s_ipd );
	}
	if ( ohmdApi.device_getf( s_device, OHMD_LEFT_EYE_FOV, &fov ) == 0 && fov > 1.0f ) {
		s_fov = fov;
	}

	VectorClear( s_recenterDelta );
	s_active = qtrue;
	Com_Printf( "OpenHMD: opened [%d] %s %s (IPD=%.3fm FOV=%.1f)\n",
		index, s_vendor, s_product, s_ipd, s_fov );
	return qtrue;
}

static void OHMD_UpdateStereoSeparation( void )
{
	float zproj;
	float eyeOffset;
	float sep;

	if ( !OHMD_WantStereo() ) {
		return;
	}
	zproj = Cvar_VariableValue( "r_zproj" );
	if ( zproj < 0.1f ) {
		zproj = 4.0f;
	}
	eyeOffset = s_ipd * ( vr_openhmdWorldScale ? vr_openhmdWorldScale->value : 32.0f );
	if ( eyeOffset < 0.01f ) {
		eyeOffset = 0.01f;
	}
	/* r_stereoSeparation stores zProj / eyeOffset (see R_SetupProjection). */
	sep = zproj / eyeOffset;
	Cvar_SetValue( "r_stereoSeparation", sep );
	if ( vr_openhmdFov && vr_openhmdFov->integer && s_fov > 10.0f ) {
		Cvar_SetValue( "cg_fov", s_fov );
		Cvar_SetValue( "fov", s_fov );
	}
}

static void OHMD_Status_f( void )
{
	int maj = 0, min = 0, patch = 0;

	Com_Printf( "=== ohmd_status ===\n" );
	Com_Printf( "  compiled=1 lib=%s active=%d stereo=%d\n",
		s_libOk ? "yes" : "no",
		s_active ? 1 : 0,
		OHMD_WantStereo() ? 1 : 0 );
	if ( ohmdApi.get_version ) {
		ohmdApi.get_version( &maj, &min, &patch );
		Com_Printf( "  libVersion=%d.%d.%d\n", maj, min, patch );
	}
	Com_Printf( "  probeCount=%d device=%d\n", s_probeCount,
		vr_openhmdDevice ? vr_openhmdDevice->integer : 0 );
	if ( s_active ) {
		Com_Printf( "  hmd=%s %s\n", s_vendor, s_product );
		Com_Printf( "  ipd=%.4f fov=%.1f\n", s_ipd, s_fov );
		Com_Printf( "  angles=%.1f %.1f %.1f\n",
			s_angles[PITCH], s_angles[YAW], s_angles[ROLL] );
		Com_Printf( "  r_stereoSeparation=%s\n", Cvar_VariableString( "r_stereoSeparation" ) );
	}
	if ( !s_libOk ) {
		Com_Printf( "  hint: sudo apt install libopenhmd0\n" );
	}
}

static void OHMD_List_f( void )
{
	int i, deviceClass, flags;

	if ( !s_libOk && !OHMD_LoadLibrary() ) {
		return;
	}
	if ( !s_ctx ) {
		s_ctx = ohmdApi.ctx_create();
		if ( !s_ctx ) {
			Com_Printf( "OpenHMD: ctx_create failed\n" );
			return;
		}
	}
	s_probeCount = ohmdApi.ctx_probe( s_ctx );
	Com_Printf( "OpenHMD devices (%d):\n", s_probeCount );
	for ( i = 0; i < s_probeCount; i++ ) {
		deviceClass = -1;
		flags = 0;
		ohmdApi.list_geti( s_ctx, i, OHMD_DEVICE_CLASS, &deviceClass );
		ohmdApi.list_geti( s_ctx, i, OHMD_DEVICE_FLAGS, &flags );
		Com_Printf( "  [%d] %s %s  class=%d flags=0x%x%s\n",
			i,
			ohmdApi.list_gets( s_ctx, i, OHMD_VENDOR ),
			ohmdApi.list_gets( s_ctx, i, OHMD_PRODUCT ),
			deviceClass, flags,
			( flags & OHMD_DEVICE_FLAGS_NULL_DEVICE ) ? " (null)" : "" );
	}
}

static void OHMD_Open_f( void )
{
	int index = 0;

	if ( Cmd_Argc() >= 2 ) {
		index = atoi( Cmd_Argv( 1 ) );
	} else if ( vr_openhmdDevice ) {
		index = vr_openhmdDevice->integer;
	}
	if ( !s_libOk && !OHMD_LoadLibrary() ) {
		return;
	}
	if ( !s_ctx ) {
		s_ctx = ohmdApi.ctx_create();
	}
	if ( !s_ctx ) {
		Com_Printf( "OpenHMD: ctx_create failed\n" );
		return;
	}
	s_probeCount = ohmdApi.ctx_probe( s_ctx );
	if ( OHMD_OpenDeviceIndex( index ) ) {
		Cvar_Set( "vr_openhmd", "1" );
		OHMD_UpdateStereoSeparation();
	}
}

static void OHMD_Close_f( void )
{
	OHMD_CloseDevice();
	Com_Printf( "OpenHMD: device closed\n" );
}

static void OHMD_Recenter_f( void )
{
	if ( !s_active ) {
		Com_Printf( "OpenHMD: no active device\n" );
		return;
	}
	s_recenterDelta[PITCH] = -s_angles[PITCH];
	s_recenterDelta[YAW] = -s_angles[YAW];
	s_recenterDelta[ROLL] = -s_angles[ROLL];
	Com_Printf( "OpenHMD: recentered\n" );
}

void OHMD_Init( void )
{
	vr_openhmd = Cvar_Get( "vr_openhmd", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmd,
		"Enable OpenHMD head tracking (requires libopenhmd). 1=auto-open device." );
	vr_openhmdDevice = Cvar_Get( "vr_openhmdDevice", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmdDevice, "OpenHMD device index from ohmd_list." );
	vr_openhmdStereo = Cvar_Get( "vr_openhmdStereo", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmdStereo,
		"When HMD active, render STEREO_LEFT+RIGHT with r_stereoSeparation from IPD." );
	vr_openhmdMouse = Cvar_Get( "vr_openhmdMouse", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmdMouse, "Allow mouse look while OpenHMD tracking is active." );
	vr_openhmdIpd = Cvar_Get( "vr_openhmdIpd", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmdIpd, "Override IPD in metres (0=use device)." );
	vr_openhmdWorldScale = Cvar_Get( "vr_openhmdWorldScale", "32", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmdWorldScale,
		"Metres→Quake units for IPD→r_stereoSeparation (default 32)." );
	vr_openhmdFov = Cvar_Get( "vr_openhmdFov", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmdFov, "When 1, push HMD FOV into cg_fov/fov." );
	vr_openhmdRoll = Cvar_Get( "vr_openhmdRoll", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( vr_openhmdRoll, "Apply HMD roll to viewangles." );

	Cmd_AddCommand( "ohmd_status", OHMD_Status_f );
	Cmd_AddCommand( "ohmd_list", OHMD_List_f );
	Cmd_AddCommand( "ohmd_open", OHMD_Open_f );
	Cmd_AddCommand( "ohmd_close", OHMD_Close_f );
	Cmd_AddCommand( "ohmd_recenter", OHMD_Recenter_f );

	VectorClear( s_angles );
	VectorClear( s_recenterDelta );

	if ( !OHMD_LoadLibrary() ) {
		return;
	}
	s_ctx = ohmdApi.ctx_create();
	if ( !s_ctx ) {
		Com_Printf( "OpenHMD: ctx_create failed\n" );
		return;
	}
	s_probeCount = ohmdApi.ctx_probe( s_ctx );
	Com_Printf( "OpenHMD: probed %d device(s)\n", s_probeCount );

	if ( vr_openhmd->integer && s_probeCount > 0 ) {
		OHMD_OpenDeviceIndex( vr_openhmdDevice->integer );
		OHMD_UpdateStereoSeparation();
	}
}

void OHMD_Shutdown( void )
{
	OHMD_CloseDevice();
	if ( s_ctx && ohmdApi.ctx_destroy ) {
		ohmdApi.ctx_destroy( s_ctx );
	}
	s_ctx = NULL;
	if ( ohmdApi.lib ) {
		dlclose( ohmdApi.lib );
		ohmdApi.lib = NULL;
	}
	s_libOk = qfalse;
	Cmd_RemoveCommand( "ohmd_status" );
	Cmd_RemoveCommand( "ohmd_list" );
	Cmd_RemoveCommand( "ohmd_open" );
	Cmd_RemoveCommand( "ohmd_close" );
	Cmd_RemoveCommand( "ohmd_recenter" );
}

void OHMD_Frame( void )
{
	float quat[4];

	if ( !s_libOk || !s_ctx ) {
		return;
	}

	if ( vr_openhmd->modified ) {
		vr_openhmd->modified = qfalse;
		if ( vr_openhmd->integer && !s_active && s_probeCount > 0 ) {
			OHMD_OpenDeviceIndex( vr_openhmdDevice->integer );
		} else if ( !vr_openhmd->integer && s_active ) {
			OHMD_CloseDevice();
		}
		OHMD_UpdateStereoSeparation();
	}
	if ( vr_openhmdIpd && vr_openhmdIpd->modified && s_device ) {
		vr_openhmdIpd->modified = qfalse;
		if ( vr_openhmdIpd->value > 0.01f ) {
			s_ipd = vr_openhmdIpd->value;
			ohmdApi.device_setf( s_device, OHMD_EYE_IPD, &s_ipd );
			OHMD_UpdateStereoSeparation();
		}
	}

	ohmdApi.ctx_update( s_ctx );
	if ( !s_active || !s_device || !vr_openhmd->integer ) {
		return;
	}

	if ( ohmdApi.device_getf( s_device, OHMD_ROTATION_QUAT, quat ) == 0 ) {
		OHMD_QuatToAngles( quat, s_angles );
	}
}

qboolean OHMD_IsActive( void )
{
	return ( s_active && vr_openhmd && vr_openhmd->integer ) ? qtrue : qfalse;
}

qboolean OHMD_WantStereo( void )
{
	return ( OHMD_IsActive() && vr_openhmdStereo && vr_openhmdStereo->integer ) ? qtrue : qfalse;
}

qboolean OHMD_BlockMouseLook( void )
{
	return ( OHMD_IsActive() && ( !vr_openhmdMouse || !vr_openhmdMouse->integer ) ) ? qtrue : qfalse;
}

void OHMD_ApplyViewAngles( void )
{
	if ( !OHMD_IsActive() ) {
		return;
	}
	cl.viewangles[PITCH] = s_angles[PITCH] + s_recenterDelta[PITCH];
	cl.viewangles[YAW] = s_angles[YAW] + s_recenterDelta[YAW];
	if ( vr_openhmdRoll && vr_openhmdRoll->integer ) {
		cl.viewangles[ROLL] = s_angles[ROLL] + s_recenterDelta[ROLL];
	}
}

float OHMD_GetIPD( void )
{
	return s_ipd;
}

void OHMD_GetOrientationEuler( vec3_t anglesOut )
{
	VectorCopy( s_angles, anglesOut );
	anglesOut[PITCH] += s_recenterDelta[PITCH];
	anglesOut[YAW] += s_recenterDelta[YAW];
	anglesOut[ROLL] += s_recenterDelta[ROLL];
}

#else /* !USE_OPENHMD */

void OHMD_Init( void ) {}
void OHMD_Shutdown( void ) {}
void OHMD_Frame( void ) {}
qboolean OHMD_IsActive( void ) { return qfalse; }
qboolean OHMD_WantStereo( void ) { return qfalse; }
qboolean OHMD_BlockMouseLook( void ) { return qfalse; }
void OHMD_ApplyViewAngles( void ) {}
float OHMD_GetIPD( void ) { return 0.063f; }
void OHMD_GetOrientationEuler( vec3_t anglesOut ) { VectorClear( anglesOut ); }

#endif /* USE_OPENHMD */
