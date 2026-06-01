/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_beta_petri.h"

#define JSON_IMPLEMENTATION
#include "../qcommon/json.h"

#define BETA_PETRI_MAX_PLACES      16
#define BETA_PETRI_MAX_TRANSITIONS   8
#define BETA_PETRI_MAX_ARC         4
#define BETA_PETRI_ID_LEN          24
#define BETA_PETRI_MSG_LEN         48
#define BETA_PETRI_PATH_LEN        MAX_OSPATH

typedef struct {
	char id[BETA_PETRI_ID_LEN];
	qboolean marked;
} betaPetriPlace_t;

typedef struct {
	char id[BETA_PETRI_ID_LEN];
	char msgType[BETA_PETRI_MSG_LEN];
	char msgSourceRole[BETA_PETRI_ID_LEN];
	char msgTargetRole[BETA_PETRI_ID_LEN];
	int numInput;
	int numOutput;
	char input[BETA_PETRI_MAX_ARC][BETA_PETRI_ID_LEN];
	char output[BETA_PETRI_MAX_ARC][BETA_PETRI_ID_LEN];
} betaPetriTransition_t;

static cvar_t *cl_betaPetri;
static qboolean beta_petriLoaded;
static char beta_petriBasename[BETA_PETRI_PATH_LEN];
static char beta_petriGoalPlace[BETA_PETRI_ID_LEN];
static int beta_petriPlaceCount;
static int beta_petriTransitionCount;
static betaPetriPlace_t beta_petriPlaces[BETA_PETRI_MAX_PLACES];
static betaPetriTransition_t beta_petriTransitions[BETA_PETRI_MAX_TRANSITIONS];

static int CL_BetaPetri_FindPlace( const char *id ) {
	int i;

	for ( i = 0; i < beta_petriPlaceCount; i++ ) {
		if ( !Q_stricmp( beta_petriPlaces[i].id, id ) ) {
			return i;
		}
	}
	return -1;
}

static void CL_BetaPetri_Clear( void ) {
	beta_petriLoaded = qfalse;
	beta_petriBasename[0] = '\0';
	beta_petriGoalPlace[0] = '\0';
	beta_petriPlaceCount = 0;
	beta_petriTransitionCount = 0;
}

static void CL_BetaPetri_ParseStringArray( const char *json, const char *jsonEnd,
	char dest[][BETA_PETRI_ID_LEN], int maxCount, int *outCount ) {
	const char *value;

	*outCount = 0;
	if ( !json || JSON_ValueGetType( json, jsonEnd ) != JSONTYPE_ARRAY ) {
		return;
	}

	for ( value = JSON_ArrayGetFirstValue( json, jsonEnd );
		value && *outCount < maxCount;
		value = JSON_ArrayGetNextValue( value, jsonEnd ) ) {
		JSON_ValueGetString( value, jsonEnd, dest[*outCount], BETA_PETRI_ID_LEN );
		if ( dest[*outCount][0] ) {
			(*outCount)++;
		}
	}
}

static qboolean CL_BetaPetri_ParsePlaceObject( const char *obj, const char *jsonEnd ) {
	const char *idVal;

	if ( beta_petriPlaceCount >= BETA_PETRI_MAX_PLACES ) {
		return qfalse;
	}

	idVal = JSON_ObjectGetNamedValue( obj, jsonEnd, "id" );
	if ( !idVal ) {
		return qfalse;
	}

	JSON_ValueGetString( idVal, jsonEnd, beta_petriPlaces[beta_petriPlaceCount].id,
		BETA_PETRI_ID_LEN );
	beta_petriPlaces[beta_petriPlaceCount].marked = qfalse;
	beta_petriPlaceCount++;
	return qtrue;
}

static qboolean CL_BetaPetri_ParseTransitionObject( const char *obj, const char *jsonEnd ) {
	betaPetriTransition_t *tr;
	const char *idVal;
	const char *msgObj;
	const char *typeVal;
	const char *srcVal;
	const char *tgtVal;

	if ( beta_petriTransitionCount >= BETA_PETRI_MAX_TRANSITIONS ) {
		return qfalse;
	}

	tr = &beta_petriTransitions[beta_petriTransitionCount];
	idVal = JSON_ObjectGetNamedValue( obj, jsonEnd, "id" );
	if ( !idVal ) {
		return qfalse;
	}
	JSON_ValueGetString( idVal, jsonEnd, tr->id, BETA_PETRI_ID_LEN );

	msgObj = JSON_ObjectGetNamedValue( obj, jsonEnd, "message" );
	if ( msgObj && JSON_ValueGetType( msgObj, jsonEnd ) == JSONTYPE_OBJECT ) {
		typeVal = JSON_ObjectGetNamedValue( msgObj, jsonEnd, "type" );
		srcVal = JSON_ObjectGetNamedValue( msgObj, jsonEnd, "sourceRole" );
		tgtVal = JSON_ObjectGetNamedValue( msgObj, jsonEnd, "targetRole" );
		if ( typeVal ) {
			JSON_ValueGetString( typeVal, jsonEnd, tr->msgType, BETA_PETRI_MSG_LEN );
		}
		if ( srcVal ) {
			JSON_ValueGetString( srcVal, jsonEnd, tr->msgSourceRole, BETA_PETRI_ID_LEN );
		}
		if ( tgtVal ) {
			JSON_ValueGetString( tgtVal, jsonEnd, tr->msgTargetRole, BETA_PETRI_ID_LEN );
		}
	}

	CL_BetaPetri_ParseStringArray( JSON_ObjectGetNamedValue( obj, jsonEnd, "input" ),
		jsonEnd, tr->input, BETA_PETRI_MAX_ARC, &tr->numInput );
	CL_BetaPetri_ParseStringArray( JSON_ObjectGetNamedValue( obj, jsonEnd, "output" ),
		jsonEnd, tr->output, BETA_PETRI_MAX_ARC, &tr->numOutput );

	beta_petriTransitionCount++;
	return qtrue;
}

static qboolean CL_BetaPetri_ParseRoot( char *buf, int len ) {
	const char *jsonEnd;
	const char *root;
	const char *placesArr;
	const char *transArr;
	const char *initialArr;
	const char *elem;

	jsonEnd = buf + len;
	root = JSON_SkipSeparators( buf, jsonEnd );
	if ( !root || JSON_ValueGetType( root, jsonEnd ) != JSONTYPE_OBJECT ) {
		return qfalse;
	}

	placesArr = JSON_ObjectGetNamedValue( root, jsonEnd, "places" );
	if ( placesArr && JSON_ValueGetType( placesArr, jsonEnd ) == JSONTYPE_ARRAY ) {
		for ( elem = JSON_ArrayGetFirstValue( placesArr, jsonEnd );
			elem;
			elem = JSON_ArrayGetNextValue( elem, jsonEnd ) ) {
			if ( !CL_BetaPetri_ParsePlaceObject( elem, jsonEnd ) ) {
				return qfalse;
			}
		}
	}

	transArr = JSON_ObjectGetNamedValue( root, jsonEnd, "transitions" );
	if ( transArr && JSON_ValueGetType( transArr, jsonEnd ) == JSONTYPE_ARRAY ) {
		for ( elem = JSON_ArrayGetFirstValue( transArr, jsonEnd );
			elem;
			elem = JSON_ArrayGetNextValue( elem, jsonEnd ) ) {
			if ( !CL_BetaPetri_ParseTransitionObject( elem, jsonEnd ) ) {
				return qfalse;
			}
		}
	}

	initialArr = JSON_ObjectGetNamedValue( root, jsonEnd, "initial" );
	if ( initialArr ) {
		int i;
		int count = 0;
		char scratch[BETA_PETRI_MAX_ARC][BETA_PETRI_ID_LEN];

		CL_BetaPetri_ParseStringArray( initialArr, jsonEnd, scratch,
			BETA_PETRI_MAX_ARC, &count );
		for ( i = 0; i < count; i++ ) {
			int idx = CL_BetaPetri_FindPlace( scratch[i] );
			if ( idx >= 0 ) {
				beta_petriPlaces[idx].marked = qtrue;
			}
		}
	} else if ( beta_petriPlaceCount > 0 ) {
		beta_petriPlaces[0].marked = qtrue;
	}

	return ( beta_petriPlaceCount > 0 );
}

static qboolean CL_BetaPetri_MessageMatches( const betaPetriTransition_t *tr,
	const char *type, const char *source, const char *target ) {
	if ( !tr->msgType[0] || !type || Q_stricmp( tr->msgType, type ) ) {
		return qfalse;
	}
	if ( tr->msgSourceRole[0] && source && !Q_stristr( source, tr->msgSourceRole ) ) {
		return qfalse;
	}
	if ( tr->msgTargetRole[0] && target && !Q_stristr( target, tr->msgTargetRole ) ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean CL_BetaPetri_CanFire( const betaPetriTransition_t *tr ) {
	int i, idx;

	for ( i = 0; i < tr->numInput; i++ ) {
		idx = CL_BetaPetri_FindPlace( tr->input[i] );
		if ( idx < 0 || !beta_petriPlaces[idx].marked ) {
			return qfalse;
		}
	}
	return qtrue;
}

static void CL_BetaPetri_Fire( betaPetriTransition_t *tr ) {
	int i, idx;

	for ( i = 0; i < tr->numInput; i++ ) {
		idx = CL_BetaPetri_FindPlace( tr->input[i] );
		if ( idx >= 0 ) {
			beta_petriPlaces[idx].marked = qfalse;
		}
	}
	for ( i = 0; i < tr->numOutput; i++ ) {
		idx = CL_BetaPetri_FindPlace( tr->output[i] );
		if ( idx >= 0 ) {
			beta_petriPlaces[idx].marked = qtrue;
		}
	}
	if ( cl_betaPetri && cl_betaPetri->integer ) {
		Com_Printf( "Beta Petri: fired transition %s\n", tr->id );
	}
}

void CL_BetaPetri_OnGameplayEvent( const char *type, const char *source, const char *target ) {
	int i;

	if ( !beta_petriLoaded || !type || !type[0] ) {
		return;
	}

	for ( i = 0; i < beta_petriTransitionCount; i++ ) {
		if ( CL_BetaPetri_MessageMatches( &beta_petriTransitions[i], type, source, target ) &&
			CL_BetaPetri_CanFire( &beta_petriTransitions[i] ) ) {
			CL_BetaPetri_Fire( &beta_petriTransitions[i] );
		}
	}
}

qboolean CL_BetaPetri_GoalReached( void ) {
	int idx;

	if ( !beta_petriLoaded || !beta_petriGoalPlace[0] ) {
		return qfalse;
	}
	idx = CL_BetaPetri_FindPlace( beta_petriGoalPlace );
	return ( idx >= 0 && beta_petriPlaces[idx].marked );
}

void CL_BetaPetri_SetGoalPlace( const char *placeId ) {
	if ( placeId && placeId[0] ) {
		Q_strncpyz( beta_petriGoalPlace, placeId, sizeof( beta_petriGoalPlace ) );
	} else {
		beta_petriGoalPlace[0] = '\0';
	}
}

qboolean CL_BetaPetri_Load( const char *basename ) {
	char path[BETA_PETRI_PATH_LEN];
	fileHandle_t f;
	int len;
	char *buf;

	if ( !basename || !basename[0] ) {
		return qfalse;
	}

	CL_BetaPetri_Unload();
	Com_sprintf( path, sizeof( path ), "beta_traces/%s.petrinet.json", basename );

	len = FS_FOpenFileRead( path, &f, qtrue );
	if ( len < 0 || f == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Petri net not found: %s\n", path );
		return qfalse;
	}

	buf = (char *)Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	FS_FCloseFile( f );
	buf[len] = '\0';

	if ( !CL_BetaPetri_ParseRoot( buf, len ) ) {
		Z_Free( buf );
		Com_Printf( S_COLOR_RED "Error: invalid Petri net JSON: %s\n", path );
		CL_BetaPetri_Clear();
		return qfalse;
	}

	Z_Free( buf );
	Q_strncpyz( beta_petriBasename, basename, sizeof( beta_petriBasename ) );
	beta_petriLoaded = qtrue;
	Com_Printf( "Beta Petri: loaded %s (%d places, %d transitions)\n",
		path, beta_petriPlaceCount, beta_petriTransitionCount );
	return qtrue;
}

void CL_BetaPetri_Unload( void ) {
	CL_BetaPetri_Clear();
}

static void CL_BetaPetri_Status_f( void ) {
	int i;

	if ( !beta_petriLoaded ) {
		Com_Printf( "Beta Petri: no net loaded\n" );
		return;
	}

	Com_Printf( "Beta Petri: %s (cl_betaPetri=%d)\n",
		beta_petriBasename, cl_betaPetri ? cl_betaPetri->integer : 0 );
	if ( beta_petriGoalPlace[0] ) {
		Com_Printf( "  goal=%s reached=%d\n", beta_petriGoalPlace,
			CL_BetaPetri_GoalReached() );
	}
	for ( i = 0; i < beta_petriPlaceCount; i++ ) {
		Com_Printf( "  place %s: %s\n", beta_petriPlaces[i].id,
			beta_petriPlaces[i].marked ? "marked" : "empty" );
	}
	for ( i = 0; i < beta_petriTransitionCount; i++ ) {
		Com_Printf( "  transition %s: msg=%s fireable=%d\n",
			beta_petriTransitions[i].id,
			beta_petriTransitions[i].msgType,
			CL_BetaPetri_CanFire( &beta_petriTransitions[i] ) );
	}
}

static void CL_BetaPetri_Load_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_petri_load <basename>\n" );
		return;
	}
	CL_BetaPetri_Load( Cmd_Argv( 1 ) );
}

static void CL_BetaPetri_Validate_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_petri_validate <basename>\n" );
		return;
	}
	if ( CL_BetaPetri_Load( Cmd_Argv( 1 ) ) ) {
		Com_Printf( S_COLOR_GREEN "Beta Petri: validate OK\n" );
		CL_BetaPetri_Unload();
	}
}

void CL_BetaPetri_RegisterCommands( void ) {
	Cmd_AddCommand( "beta_petri_load", CL_BetaPetri_Load_f );
	Cmd_AddCommand( "beta_petri_status", CL_BetaPetri_Status_f );
	Cmd_AddCommand( "beta_petri_validate", CL_BetaPetri_Validate_f );
}

void CL_BetaPetri_RemoveCommands( void ) {
	Cmd_RemoveCommand( "beta_petri_load" );
	Cmd_RemoveCommand( "beta_petri_status" );
	Cmd_RemoveCommand( "beta_petri_validate" );
}

void CL_BetaPetri_Init( void ) {
	cl_betaPetri = Cvar_Get( "cl_betaPetri", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_betaPetri,
		"When 1, log Petri transition fires during beta_test (high-level adaptive replay)." );
	CL_BetaPetri_RegisterCommands();
	if ( cl_betaPetri->integer ) {
		Com_Printf( "Beta Petri: cl_betaPetri=1 (beta_petri_load / beta_petri_status)\n" );
	}
}

void CL_BetaPetri_Shutdown( void ) {
	CL_BetaPetri_Unload();
	CL_BetaPetri_RemoveCommands();
}
