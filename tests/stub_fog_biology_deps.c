/*
 * Stubs for unit_fog_biology tests.
 */
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}

static cvar_t s_fogBiology;
static cvar_t s_fogBiologyAuto;
static cvar_t s_fogBiologySite;
static cvar_t s_fogBiologyCoastKm;
static cvar_t s_fogBiologyWindMarine;

static cvar_t s_fogBiologyCoastAuto;
static cvar_t s_fogBiologyCoastAxis;
static cvar_t s_fogBiologyCoastOrigin;
static cvar_t s_fogBiologyCoastUnitsPerKm;

static cvar_t s_syncPhase;
static char s_syncPhaseStr[32] = "clear";
static cvar_t s_syncMarine;
static char s_syncMarineStr[32] = "0";
static cvar_t s_syncShannon;
static char s_syncShannonStr[32] = "0";
static cvar_t s_syncDeposition;
static char s_syncDepositionStr[32] = "1";
static cvar_t s_syncPathogen;
static char s_syncPathogenStr[32] = "0";
static cvar_t s_syncCoastKm;
static char s_syncCoastKmStr[32] = "0";

static void FB_StubBindCvar( cvar_t *cv, char *buf, size_t bufsz, const char *value )
{
	Q_strncpyz( buf, value ? value : "0", bufsz );
	cv->string = buf;
	cv->value = (float)atof( buf );
	cv->integer = atoi( buf );
}

static cvar_t *FB_StubSyncCvar( cvar_t *cv, char *buf, size_t bufsz, const char *value )
{
	if ( !cv->string ) {
		FB_StubBindCvar( cv, buf, bufsz, value );
	}
	return cv;
}

void Cmd_AddCommand( const char *cmd, void ( *f )( void ) )
{
	(void)cmd;
	(void)f;
}

void Cmd_RemoveCommand( const char *cmd )
{
	(void)cmd;
}

cvar_t *Cvar_Get( const char *name, const char *value, int flags )
{
	(void)flags;
	if ( !Q_stricmp( name, "r_fogBiology" ) ) {
		s_fogBiology.name = "r_fogBiology";
		s_fogBiology.value = atof( value );
		s_fogBiology.integer = 1;
		return &s_fogBiology;
	}
	if ( !Q_stricmp( name, "r_fogBiologyAuto" ) ) {
		s_fogBiologyAuto.name = "r_fogBiologyAuto";
		s_fogBiologyAuto.value = atof( value );
		s_fogBiologyAuto.integer = atoi( value );
		return &s_fogBiologyAuto;
	}
	if ( !Q_stricmp( name, "r_fogBiologySite" ) ) {
		s_fogBiologySite.name = "r_fogBiologySite";
		s_fogBiologySite.value = atof( value );
		s_fogBiologySite.integer = atoi( value );
		return &s_fogBiologySite;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastKm" ) ) {
		if ( !s_fogBiologyCoastKm.string ) {
			s_fogBiologyCoastKm.name = "r_fogBiologyCoastKm";
			s_fogBiologyCoastKm.value = atof( value );
			s_fogBiologyCoastKm.integer = (int)s_fogBiologyCoastKm.value;
			s_fogBiologyCoastKm.string = "10";
		}
		return &s_fogBiologyCoastKm;
	}
	if ( !Q_stricmp( name, "r_fogBiologyWindMarine" ) ) {
		s_fogBiologyWindMarine.name = "r_fogBiologyWindMarine";
		s_fogBiologyWindMarine.value = atof( value );
		s_fogBiologyWindMarine.integer = (int)s_fogBiologyWindMarine.value;
		return &s_fogBiologyWindMarine;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastAuto" ) ) {
		s_fogBiologyCoastAuto.name = "r_fogBiologyCoastAuto";
		s_fogBiologyCoastAuto.value = atof( value );
		s_fogBiologyCoastAuto.integer = atoi( value );
		return &s_fogBiologyCoastAuto;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastAxis" ) ) {
		s_fogBiologyCoastAxis.name = "r_fogBiologyCoastAxis";
		s_fogBiologyCoastAxis.value = atof( value );
		s_fogBiologyCoastAxis.integer = atoi( value );
		return &s_fogBiologyCoastAxis;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastOrigin" ) ) {
		s_fogBiologyCoastOrigin.name = "r_fogBiologyCoastOrigin";
		s_fogBiologyCoastOrigin.value = atof( value );
		s_fogBiologyCoastOrigin.integer = (int)s_fogBiologyCoastOrigin.value;
		return &s_fogBiologyCoastOrigin;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastUnitsPerKm" ) ) {
		s_fogBiologyCoastUnitsPerKm.name = "r_fogBiologyCoastUnitsPerKm";
		s_fogBiologyCoastUnitsPerKm.value = atof( value );
		s_fogBiologyCoastUnitsPerKm.integer = (int)s_fogBiologyCoastUnitsPerKm.value;
		return &s_fogBiologyCoastUnitsPerKm;
	}
	if ( !Q_stricmp( name, "r_fogBiologySyncPhase" ) ) {
		return FB_StubSyncCvar( &s_syncPhase, s_syncPhaseStr, sizeof( s_syncPhaseStr ), value );
	}
	if ( !Q_stricmp( name, "r_fogBiologySyncMarine" ) ) {
		return FB_StubSyncCvar( &s_syncMarine, s_syncMarineStr, sizeof( s_syncMarineStr ), value );
	}
	if ( !Q_stricmp( name, "r_fogBiologySyncShannon" ) ) {
		return FB_StubSyncCvar( &s_syncShannon, s_syncShannonStr, sizeof( s_syncShannonStr ), value );
	}
	if ( !Q_stricmp( name, "r_fogBiologySyncDeposition" ) ) {
		return FB_StubSyncCvar( &s_syncDeposition, s_syncDepositionStr, sizeof( s_syncDepositionStr ), value );
	}
	if ( !Q_stricmp( name, "r_fogBiologySyncPathogen" ) ) {
		return FB_StubSyncCvar( &s_syncPathogen, s_syncPathogenStr, sizeof( s_syncPathogenStr ), value );
	}
	if ( !Q_stricmp( name, "r_fogBiologySyncCoastKm" ) ) {
		return FB_StubSyncCvar( &s_syncCoastKm, s_syncCoastKmStr, sizeof( s_syncCoastKmStr ), value );
	}
	{
		static cvar_t cv;
		static char cvStr[32];
		FB_StubBindCvar( &cv, cvStr, sizeof( cvStr ), value );
		return &cv;
	}
}

void Cvar_SetDescription( cvar_t *var, const char *description )
{
	(void)var;
	(void)description;
}

void Cvar_SetValue( const char *name, float value )
{
	if ( name && !Q_stricmp( name, "r_fogBiologyCoastKm" ) ) {
		s_fogBiologyCoastKm.value = value;
		s_fogBiologyCoastKm.integer = (int)value;
		return;
	}
	if ( name && !Q_stricmp( name, "r_fogBiologyCoastAuto" ) ) {
		s_fogBiologyCoastAuto.value = value;
		s_fogBiologyCoastAuto.integer = (int)value;
		return;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastOrigin" ) ) {
		s_fogBiologyCoastOrigin.value = value;
		s_fogBiologyCoastOrigin.integer = (int)value;
		return;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastUnitsPerKm" ) ) {
		s_fogBiologyCoastUnitsPerKm.value = value;
		s_fogBiologyCoastUnitsPerKm.integer = (int)value;
		return;
	}
	if ( !Q_stricmp( name, "r_fogBiologyCoastAxis" ) ) {
		s_fogBiologyCoastAxis.value = value;
		s_fogBiologyCoastAxis.integer = (int)value;
		return;
	}
}

cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force )
{
	cvar_t *cv;

	(void)force;
	cv = Cvar_Get( var_name, value, 0 );
	if ( cv && cv->string && value ) {
		size_t bufsz = 32;
		if ( cv == &s_syncPhase ) {
			bufsz = sizeof( s_syncPhaseStr );
		} else if ( cv == &s_syncMarine ) {
			bufsz = sizeof( s_syncMarineStr );
		} else if ( cv == &s_syncShannon ) {
			bufsz = sizeof( s_syncShannonStr );
		} else if ( cv == &s_syncDeposition ) {
			bufsz = sizeof( s_syncDepositionStr );
		} else if ( cv == &s_syncPathogen ) {
			bufsz = sizeof( s_syncPathogenStr );
		} else if ( cv == &s_syncCoastKm ) {
			bufsz = sizeof( s_syncCoastKmStr );
		}
		FB_StubBindCvar( cv, cv->string, bufsz, value );
	}
	return cv;
}

float Cvar_VariableValue( const char *var_name )
{
	cvar_t *cv;

	if ( !Q_stricmp( var_name, "r_volumetricFog" ) ) {
		return 0.0f;
	}
	if ( !Q_stricmp( var_name, "r_fogBiologyCoastKm" ) ) {
		return s_fogBiologyCoastKm.value;
	}
	cv = Cvar_Get( var_name, "0", 0 );
	if ( cv && cv->string ) {
		return (float)atof( cv->string );
	}
	return 0.0f;
}

const char *Cvar_VariableString( const char *var_name )
{
	cvar_t *cv;

	cv = Cvar_Get( var_name, "", 0 );
	if ( cv && cv->string ) {
		return cv->string;
	}
	return "";
}

int Cvar_VariableIntegerValue( const char *var_name )
{
	return (int)Cvar_VariableValue( var_name );
}
