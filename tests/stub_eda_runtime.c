/*
 * Minimal runtime stubs for g_eda.c unit tests.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t g_eda_stub;
static cvar_t g_edaLog_stub;
static char g_eda_string[4] = "1";
static char g_edaLog_string[4] = "0";
static int g_eda_enabled = 1;
static int g_eda_log_enabled = 0;

static int g_telemetry_count;
static char g_telemetry_name[64];
static double g_telemetry_value;

static void StubEDA_UpdateCvar( cvar_t *var, const char *name, char *string, int value, int flags ) {
	memset( var, 0, sizeof( *var ) );
	string[0] = (char)( value ? '1' : '0' );
	string[1] = '\0';
	var->name = (char *)name;
	var->string = string;
	var->resetString = string;
	var->flags = flags;
	var->value = value ? 1.0f : 0.0f;
	var->integer = value ? 1 : 0;
}

void StubEDA_SetEnabled( int enabled ) {
	g_eda_enabled = enabled ? 1 : 0;
	StubEDA_UpdateCvar( &g_eda_stub, "g_eda", g_eda_string, g_eda_enabled, CVAR_ARCHIVE_ND );
}

void StubEDA_SetLog( int enabled ) {
	g_eda_log_enabled = enabled ? 1 : 0;
	StubEDA_UpdateCvar( &g_edaLog_stub, "g_edaLog", g_edaLog_string, g_eda_log_enabled, CVAR_ARCHIVE_ND );
}

void StubEDA_ResetTelemetry( void ) {
	g_telemetry_count = 0;
	g_telemetry_name[0] = '\0';
	g_telemetry_value = 0.0;
}

int StubEDA_TelemetryCount( void ) {
	return g_telemetry_count;
}

const char *StubEDA_LastTelemetryName( void ) {
	return g_telemetry_name;
}

double StubEDA_LastTelemetryValue( void ) {
	return g_telemetry_value;
}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags ) {
	(void)value;
	if ( var_name && strcmp( var_name, "g_eda" ) == 0 ) {
		StubEDA_UpdateCvar( &g_eda_stub, "g_eda", g_eda_string, g_eda_enabled, flags );
		return &g_eda_stub;
	}
	if ( var_name && strcmp( var_name, "g_edaLog" ) == 0 ) {
		StubEDA_UpdateCvar( &g_edaLog_stub, "g_edaLog", g_edaLog_string, g_eda_log_enabled, flags );
		return &g_edaLog_stub;
	}
	return NULL;
}

void Cvar_SetDescription( cvar_t *var, const char *var_description ) {
	if ( var ) {
		var->description = (char *)var_description;
	}
}

void QDECL Com_DPrintf( const char *fmt, ... ) {
	(void)fmt;
}

void EngineTelemetry_Record( const char *name, double value ) {
	g_telemetry_count++;
	if ( name ) {
		Q_strncpyz( g_telemetry_name, name, sizeof( g_telemetry_name ) );
	} else {
		g_telemetry_name[0] = '\0';
	}
	g_telemetry_value = value;
}
