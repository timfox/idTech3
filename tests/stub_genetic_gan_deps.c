/*
 * Stubs for unit_genetic_gan tests.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}

static cvar_t s_geneticGan;
static char s_geneticGanStr[8] = "1";
static cvar_t s_geneticGanDim;
static char s_geneticGanDimStr[8] = "16";
static cvar_t s_geneticGanMutationRate;
static char s_geneticGanMutationRateStr[16] = "0.05";
static cvar_t s_geneticGanCrossoverBlend;
static char s_geneticGanCrossoverBlendStr[16] = "0.5";
static cvar_t s_syncJob;
static char s_syncJobStr[32] = "0";
static cvar_t s_syncSlot;
static char s_syncSlotStr[32] = "-1";
static cvar_t s_syncCount;
static char s_syncCountStr[32] = "0";
static cvar_t s_dummy;
static char s_dummyStr[64] = "0";

static void GG_StubBindCvar( cvar_t *cv, const char *name, char *buf, size_t bufsz, const char *value )
{
	Q_strncpyz( buf, value ? value : "0", bufsz );
	cv->name = (char *)(uintptr_t)name;
	cv->string = buf;
	cv->value = (float)atof( buf );
	cv->integer = atoi( buf );
}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags )
{
	(void)flags;
	if ( !Q_stricmp( var_name, "cl_geneticGan" ) ) {
		if ( !s_geneticGan.string ) {
			GG_StubBindCvar( &s_geneticGan, "cl_geneticGan", s_geneticGanStr, sizeof( s_geneticGanStr ), "1" );
		}
		return &s_geneticGan;
	}
	if ( !Q_stricmp( var_name, "cl_geneticGanDim" ) ) {
		if ( !s_geneticGanDim.string ) {
			GG_StubBindCvar( &s_geneticGanDim, "cl_geneticGanDim", s_geneticGanDimStr, sizeof( s_geneticGanDimStr ),
				value ? value : "16" );
		}
		return &s_geneticGanDim;
	}
	if ( !Q_stricmp( var_name, "cl_geneticGanMutationRate" ) ) {
		if ( !s_geneticGanMutationRate.string ) {
			GG_StubBindCvar( &s_geneticGanMutationRate, "cl_geneticGanMutationRate",
				s_geneticGanMutationRateStr, sizeof( s_geneticGanMutationRateStr ), value ? value : "0.05" );
		}
		return &s_geneticGanMutationRate;
	}
	if ( !Q_stricmp( var_name, "cl_geneticGanCrossoverBlend" ) ) {
		if ( !s_geneticGanCrossoverBlend.string ) {
			GG_StubBindCvar( &s_geneticGanCrossoverBlend, "cl_geneticGanCrossoverBlend",
				s_geneticGanCrossoverBlendStr, sizeof( s_geneticGanCrossoverBlendStr ), value ? value : "0.5" );
		}
		return &s_geneticGanCrossoverBlend;
	}
	if ( !Q_stricmp( var_name, "cl_geneticGanSyncJob" ) ) {
		if ( !s_syncJob.string ) {
			GG_StubBindCvar( &s_syncJob, "cl_geneticGanSyncJob", s_syncJobStr, sizeof( s_syncJobStr ), "0" );
		}
		return &s_syncJob;
	}
	if ( !Q_stricmp( var_name, "cl_geneticGanSyncSlot" ) ) {
		if ( !s_syncSlot.string ) {
			GG_StubBindCvar( &s_syncSlot, "cl_geneticGanSyncSlot", s_syncSlotStr, sizeof( s_syncSlotStr ), "-1" );
		}
		return &s_syncSlot;
	}
	if ( !Q_stricmp( var_name, "cl_geneticGanSyncCount" ) ) {
		if ( !s_syncCount.string ) {
			GG_StubBindCvar( &s_syncCount, "cl_geneticGanSyncCount", s_syncCountStr, sizeof( s_syncCountStr ), "0" );
		}
		return &s_syncCount;
	}
	GG_StubBindCvar( &s_dummy, var_name, s_dummyStr, sizeof( s_dummyStr ), value ? value : "0" );
	return &s_dummy;
}

void Cvar_CheckRange( cvar_t *cv, const char *min, const char *max, cvarValidator_t type )
{
	(void)cv;
	(void)min;
	(void)max;
	(void)type;
}

void Cvar_SetDescription( cvar_t *cv, const char *desc )
{
	(void)cv;
	(void)desc;
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

int Cmd_Argc( void )
{
	return 0;
}

const char *Cmd_Argv( int arg )
{
	(void)arg;
	return "";
}

char *Cmd_ArgsFrom( int arg )
{
	(void)arg;
	return (char *)"";
}
