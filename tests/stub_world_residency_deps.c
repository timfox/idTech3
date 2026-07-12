/*
 * Stubs for unit_world_residency (WorldOpen, cm_stream, WorldProc, cvars).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "qcommon/cm_stream.h"
#include "world/world_open.h"
#include "world/world_proc.h"
#include "world/sector_graph.h"

int com_frameTime = 0;

void SectorGraph_Init( void ) {}
void SectorGraph_Shutdown( void ) {}
void SectorGraph_SetGpuReachFn( sectorGraphGpuReach_f fn ) { (void)fn; }
qboolean SectorGraph_StreamReachEnabled( void ) { return qfalse; }
qboolean SectorGraph_ComputeEnabled( void ) { return qfalse; }
qboolean SectorGraph_VerifyEnabled( void ) { return qfalse; }
void SectorGraph_UpdateReachability( const vec3_t viewOrigin, const vec3_t *extraOrigins,
	int extraOriginCount, float sectorSize, float unloadRadius, int maxHops ) {
	(void)viewOrigin; (void)extraOrigins; (void)extraOriginCount;
	(void)sectorSize; (void)unloadRadius; (void)maxHops;
}
qboolean SectorGraph_IsReachable( int cellX, int cellY ) { (void)cellX; (void)cellY; return qtrue; }
void SectorGraph_ReachTest_f( void ) {}
void SectorGraph_Status_f( void ) {}

static cvar_t s_cvars[32];
static char s_cvarNames[32][64];
static char s_cvarStrings[32][64];
static int s_cvarCount;

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}

cvar_t *Cvar_Get( const char *name, const char *value, int flags )
{
	cvar_t *cv;
	(void)flags;
	if ( s_cvarCount >= (int)( sizeof( s_cvars ) / sizeof( s_cvars[0] ) ) ) {
		return &s_cvars[0];
	}
	cv = &s_cvars[s_cvarCount];
	memset( cv, 0, sizeof( *cv ) );
	Q_strncpyz( s_cvarNames[s_cvarCount], name, sizeof( s_cvarNames[0] ) );
	Q_strncpyz( s_cvarStrings[s_cvarCount], value, sizeof( s_cvarStrings[0] ) );
	cv->name = s_cvarNames[s_cvarCount];
	cv->string = s_cvarStrings[s_cvarCount];
	cv->value = atof( value );
	cv->integer = atoi( value );
	s_cvarCount++;
	return cv;
}

void Cvar_SetDescription( cvar_t *var, const char *description )
{
	(void)var;
	(void)description;
}

void Cmd_AddCommand( const char *cmd_name, xcommand_t function )
{
	(void)cmd_name;
	(void)function;
}

void Cmd_RemoveCommand( const char *cmd_name )
{
	(void)cmd_name;
}

float Cvar_VariableValue( const char *var_name )
{
	int i;

	for ( i = 0; i < s_cvarCount; i++ ) {
		if ( s_cvars[i].name && !Q_stricmp( s_cvars[i].name, var_name ) ) {
			return s_cvars[i].value;
		}
	}
	if ( !Q_stricmp( var_name, "r_openWorldSectorSize" ) ) {
		return 4096.0f;
	}
	return 0.0f;
}

int Cvar_VariableIntegerValue( const char *var_name )
{
	return (int)Cvar_VariableValue( var_name );
}

void CM_Stream_WorldToCell( const vec3_t origin, float sectorSize, int *cellX, int *cellY )
{
	if ( cellX ) {
		*cellX = (int)floor( origin[0] / sectorSize );
	}
	if ( cellY ) {
		*cellY = (int)floor( origin[1] / sectorSize );
	}
}

void CM_Stream_BuildLoadedList( char *buf, int bufsize )
{
	if ( buf && bufsize > 0 ) {
		buf[0] = '\0';
	}
}

qboolean CM_Stream_LoadSector( int cellX, int cellY )
{
	(void)cellX;
	(void)cellY;
	return qtrue;
}

void CM_Stream_UnloadSector( int cellX, int cellY )
{
	(void)cellX;
	(void)cellY;
}

void CM_Stream_UpdateView( const vec3_t viewOrigin, float radius, float sectorSize, qboolean mergeCollision )
{
	(void)viewOrigin;
	(void)radius;
	(void)sectorSize;
	(void)mergeCollision;
}

int WorldOpen_GetSectorCount( void )
{
	return 0;
}

const worldOpenSector_t *WorldOpen_GetSector( int index )
{
	(void)index;
	return NULL;
}

qboolean WorldOpen_LoadSector( int cellX, int cellY, worldOpenLayerMask_t layerMask )
{
	(void)cellX;
	(void)cellY;
	(void)layerMask;
	return qtrue;
}

void WorldOpen_UnloadSectorLayers( int cellX, int cellY, worldOpenLayerMask_t layerMask )
{
	(void)cellX;
	(void)cellY;
	(void)layerMask;
}

worldProcSample_t WorldProc_SampleSector( int cellX, int cellY, float sectorSize )
{
	worldProcSample_t s;
	(void)sectorSize;
	memset( &s, 0, sizeof( s ) );
	s.regionId = ( cellX + cellY ) & 7;
	s.paletteIndex = ( cellX * 3 + cellY * 5 ) & 15;
	return s;
}

int WorldProc_RegionAtSector( int cellX, int cellY, float sectorSize )
{
	(void)sectorSize;
	return ( cellX + cellY ) & 7;
}
