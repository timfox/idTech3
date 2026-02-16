#include "q_shared.h"
#include "qcommon.h"
#include "js_debug.h"

#ifdef USE_DUKTAPE

#include <duktape.h>
#ifndef DEDICATED
#include "../client/client.h"
#endif

#define MAX_JS_TRACKED_SCRIPTS 64
#define MAX_JS_EVENT_CALLBACKS 64
#define JS_STASH_EVENTS "\xff""id3.events"

static duk_context *s_jsContext;
static int s_jsTrackedCount;
static char s_jsTrackedScripts[MAX_JS_TRACKED_SCRIPTS][MAX_OSPATH];

static cvar_t *js_allowEvents;
static cvar_t *js_allowExec;
static cvar_t *js_cvarSetMode;
static cvar_t *js_allowFileWrite;

static qboolean JsDebug_OpenState( void );
static qboolean JsDebug_LoadScript( const char *scriptPath );

static void JsDebug_InitPolicyCvars( void ) {
	if ( !js_allowEvents ) {
		js_allowEvents = Cvar_Get( "js_allowEvents", "1", CVAR_ARCHIVE_ND );
	}
	if ( !js_allowExec ) {
		js_allowExec = Cvar_Get( "js_allowExec", "1", CVAR_ARCHIVE_ND );
	}
	if ( !js_cvarSetMode ) {
		js_cvarSetMode = Cvar_Get( "js_cvarSetMode", "1", CVAR_ARCHIVE_ND );
	}
	if ( !js_allowFileWrite ) {
		js_allowFileWrite = Cvar_Get( "js_allowFileWrite", "0", CVAR_ARCHIVE_ND );
	}
}

static qboolean JsDebug_IsSupportedEvent( const char *eventName ) {
	if ( !eventName || !eventName[0] ) {
		return qfalse;
	}
	return ( !Q_stricmp( eventName, "frame" ) ||
		!Q_stricmp( eventName, "map_load" ) ||
		!Q_stricmp( eventName, "client_connect" ) ||
		!Q_stricmp( eventName, "ui_open" ) ||
		!Q_stricmp( eventName, "ui_close" ) ||
		!Q_stricmp( eventName, "menu_changed" ) ||
		!Q_stricmp( eventName, "input_key" ) ||
		!Q_stricmp( eventName, "mouse_move" ) ||
		!Q_stricmp( eventName, "console_open" ) );
}

static qboolean JsDebug_IsAllowedScriptPath( const char *path ) {
	return ( !Q_strncmp( path, "ui/", 3 ) ||
		!Q_strncmp( path, "client/", 7 ) ||
		!Q_strncmp( path, "frontend/", 9 ) ||
		!Q_strncmp( path, "scripts/js/", 11 ) );
}

static const char *JsDebug_TypeName( int type ) {
	switch ( type ) {
		case DUK_TYPE_NONE:
			return "none";
		case DUK_TYPE_UNDEFINED:
			return "undefined";
		case DUK_TYPE_NULL:
			return "null";
		case DUK_TYPE_BOOLEAN:
			return "boolean";
		case DUK_TYPE_NUMBER:
			return "number";
		case DUK_TYPE_STRING:
			return "string";
		case DUK_TYPE_OBJECT:
			return "object";
		case DUK_TYPE_BUFFER:
			return "buffer";
		case DUK_TYPE_POINTER:
			return "pointer";
		case DUK_TYPE_LIGHTFUNC:
			return "lightfunc";
		default:
			return "unknown";
	}
}

static int JsDebug_ClampInt( int value, int minValue, int maxValue ) {
	if ( value < minValue ) {
		return minValue;
	}
	if ( value > maxValue ) {
		return maxValue;
	}
	return value;
}

static qboolean JsDebug_IsSafePath( const char *path ) {
	int i;

	if ( !path || !path[0] ) {
		return qfalse;
	}
	if ( path[0] == '/' || path[0] == '\\' ) {
		return qfalse;
	}
	if ( strstr( path, ".." ) ) {
		return qfalse;
	}
	if ( strchr( path, ':' ) ) {
		return qfalse;
	}
	for ( i = 0; path[i]; i++ ) {
		if ( (unsigned char)path[i] < 32 ) {
			return qfalse;
		}
	}
	return qtrue;
}

static qboolean JsDebug_IsExecAllowed( cbufExec_t when ) {
	const int mode = JsDebug_ClampInt( js_allowExec ? js_allowExec->integer : 0, 0, 3 );

	if ( mode <= 0 ) {
		return qfalse;
	}
	if ( mode == 1 ) {
		return when == EXEC_APPEND;
	}
	if ( mode == 2 ) {
		return when == EXEC_APPEND || when == EXEC_INSERT;
	}
	return qtrue;
}

static qboolean JsDebug_IsCvarSetAllowed( const char *name ) {
	const unsigned flags = Cvar_Flags( name );
	const int mode = JsDebug_ClampInt( js_cvarSetMode ? js_cvarSetMode->integer : 0, 0, 3 );

	if ( mode <= 0 ) {
		return qfalse;
	}

	if ( flags == CVAR_NONEXISTENT ) {
		return mode >= 2;
	}

	if ( mode < 3 ) {
		if ( flags & ( CVAR_ROM | CVAR_INIT | CVAR_PROTECTED | CVAR_PRIVATE | CVAR_CHEAT | CVAR_LATCH ) ) {
			return qfalse;
		}
		if ( flags & ( CVAR_USERINFO | CVAR_SERVERINFO | CVAR_SYSTEMINFO ) ) {
			return qfalse;
		}
	}

	return qtrue;
}

static qboolean JsDebug_GetEventCallbacksArray( duk_context *ctx, const char *eventName, qboolean create ) {
	duk_push_global_stash( ctx );
	if ( !duk_get_prop_string( ctx, -1, JS_STASH_EVENTS ) || !duk_is_object( ctx, -1 ) ) {
		duk_pop( ctx );
		if ( !create ) {
			duk_pop( ctx );
			return qfalse;
		}
		duk_push_object( ctx );
		duk_dup( ctx, -1 );
		duk_put_prop_string( ctx, -3, JS_STASH_EVENTS );
	}
	duk_remove( ctx, -2 );

	if ( !duk_get_prop_string( ctx, -1, eventName ) || !duk_is_array( ctx, -1 ) ) {
		duk_pop( ctx );
		if ( !create ) {
			duk_pop( ctx );
			return qfalse;
		}
		duk_push_array( ctx );
		duk_dup( ctx, -1 );
		duk_put_prop_string( ctx, -3, eventName );
	}
	duk_remove( ctx, -2 );
	return qtrue;
}

static int JsDebug_EventCallbackCount( const char *eventName ) {
	duk_uarridx_t len;

	if ( !s_jsContext ) {
		return 0;
	}

	if ( !JsDebug_GetEventCallbacksArray( s_jsContext, eventName, qfalse ) ) {
		return 0;
	}

	len = duk_get_length( s_jsContext, -1 );
	duk_pop( s_jsContext );
	return (int)len;
}

static void JsDebug_ClearTrackedScripts( void ) {
	s_jsTrackedCount = 0;
}

static void JsDebug_CloseState( void ) {
	if ( s_jsContext ) {
		duk_destroy_heap( s_jsContext );
		s_jsContext = NULL;
	}
}

static void JsDebug_PrintJsError( const char *prefix ) {
	const char *msg = duk_safe_to_string( s_jsContext, -1 );
	Com_Printf( S_COLOR_RED "JavaScript: %s: %s\n", prefix, msg ? msg : "(unknown error)" );
	duk_pop( s_jsContext );
}

static duk_ret_t Js_Binding_Print( duk_context *ctx ) {
	const int argc = duk_get_top( ctx );
	int i;

	if ( argc <= 0 ) {
		Com_Printf( "\n" );
		return 0;
	}

	for ( i = 0; i < argc; i++ ) {
		const char *part = duk_safe_to_string( ctx, i );
		Com_Printf( "%s%s", i > 0 ? " " : "", part ? part : "(null)" );
	}
	Com_Printf( "\n" );
	return 0;
}

static duk_ret_t Js_Binding_CvarGet( duk_context *ctx ) {
	const char *name = duk_require_string( ctx, 0 );
	const char *value = Cvar_VariableString( name );
	duk_push_string( ctx, value ? value : "" );
	return 1;
}

static duk_ret_t Js_Binding_CvarSet( duk_context *ctx ) {
	const char *name = duk_require_string( ctx, 0 );
	const char *value = duk_safe_to_string( ctx, 1 );

	if ( !JsDebug_IsCvarSetAllowed( name ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "cvarSet denied for '%s' (js_cvarSetMode=%d)", name, js_cvarSetMode ? js_cvarSetMode->integer : 0 );
	}

	Cvar_Set( name, value ? value : "" );
	return 0;
}

static duk_ret_t Js_Binding_Exec( duk_context *ctx ) {
	const char *cmd = duk_require_string( ctx, 0 );
	const char *mode = ( duk_get_top( ctx ) > 1 ) ? duk_safe_to_string( ctx, 1 ) : "append";
	cbufExec_t when = EXEC_APPEND;

	if ( mode && !Q_stricmp( mode, "now" ) ) {
		when = EXEC_NOW;
	} else if ( mode && !Q_stricmp( mode, "insert" ) ) {
		when = EXEC_INSERT;
	}

	if ( !JsDebug_IsExecAllowed( when ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "exec denied for mode '%s' (js_allowExec=%d)", mode ? mode : "append", js_allowExec ? js_allowExec->integer : 0 );
	}

	Cbuf_ExecuteText( when, cmd );
	return 0;
}

static duk_ret_t Js_Binding_ReadFile( duk_context *ctx ) {
	const char *path = duk_require_string( ctx, 0 );
	void *buffer = NULL;
	const int len = FS_ReadFile( path, &buffer );

	if ( !JsDebug_IsSafePath( path ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "unsafe path '%s'", path );
	}

	if ( len < 0 ) {
		duk_push_null( ctx );
		return 1;
	}

	if ( buffer && len > 0 ) {
		duk_push_lstring( ctx, (const char *)buffer, (duk_size_t)len );
		FS_FreeFile( buffer );
		return 1;
	}

	if ( buffer ) {
		FS_FreeFile( buffer );
	}

	duk_push_string( ctx, "" );
	return 1;
}

static duk_ret_t Js_Binding_WriteFile( duk_context *ctx ) {
	const char *path = duk_require_string( ctx, 0 );
	duk_size_t dataLen = 0;
	const char *data = duk_require_lstring( ctx, 1, &dataLen );

	if ( !js_allowFileWrite || !js_allowFileWrite->integer ) {
		return duk_error( ctx, DUK_ERR_ERROR, "writeFile denied (js_allowFileWrite=0)" );
	}
	if ( !JsDebug_IsSafePath( path ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "unsafe path '%s'", path );
	}

	FS_WriteFile( path, data, (int)dataLen );
	duk_push_true( ctx );
	return 1;
}

static duk_ret_t Js_Binding_AppendFile( duk_context *ctx ) {
	const char *path = duk_require_string( ctx, 0 );
	duk_size_t dataLen = 0;
	const char *data = duk_require_lstring( ctx, 1, &dataLen );
	fileHandle_t f;
	int written;

	if ( !js_allowFileWrite || !js_allowFileWrite->integer ) {
		return duk_error( ctx, DUK_ERR_ERROR, "appendFile denied (js_allowFileWrite=0)" );
	}
	if ( !JsDebug_IsSafePath( path ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "unsafe path '%s'", path );
	}

	f = FS_FOpenFileAppend( path );
	if ( f == FS_INVALID_HANDLE ) {
		duk_push_false( ctx );
		return 1;
	}

	written = FS_Write( data, (int)dataLen, f );
	FS_FCloseFile( f );

	duk_push_boolean( ctx, written == (int)dataLen );
	return 1;
}

static duk_ret_t Js_Binding_Include( duk_context *ctx ) {
	const char *path = duk_require_string( ctx, 0 );

	if ( !JsDebug_IsSafePath( path ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "unsafe path '%s'", path );
	}

	if ( !JsDebug_LoadScript( path ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "include failed: %s", path );
	}

	duk_push_true( ctx );
	return 1;
}

#ifndef DEDICATED
static duk_ret_t Js_Binding_TextureLoad( duk_context *ctx ) {
	const char *path = duk_require_string( ctx, 0 );
	const qboolean noMip = ( duk_get_top( ctx ) > 1 ) ? duk_to_boolean( ctx, 1 ) : qfalse;
	qhandle_t shader;

	if ( !path || !path[0] ) {
		return duk_error( ctx, DUK_ERR_ERROR, "textureLoad: empty path" );
	}

	shader = noMip ? re.RegisterShaderNoMip( path ) : re.RegisterShader( path );
	duk_push_int( ctx, shader );
	return 1;
}

static duk_ret_t Js_Binding_TextureReload( duk_context *ctx ) {
	const char *path = duk_require_string( ctx, 0 );
	qhandle_t shader;

	if ( !path || !path[0] ) {
		return duk_error( ctx, DUK_ERR_ERROR, "textureReload: empty path" );
	}

	shader = re.RegisterShader( path );
	duk_push_int( ctx, shader );
	return 1;
}

static duk_ret_t Js_Binding_MaterialRegister( duk_context *ctx ) {
	const char *name = duk_require_string( ctx, 0 );
	qhandle_t shader;

	if ( !name || !name[0] ) {
		return duk_error( ctx, DUK_ERR_ERROR, "materialRegister: empty name" );
	}

	shader = re.RegisterShader( name );
	duk_push_int( ctx, shader );
	return 1;
}

static duk_ret_t Js_Binding_HudDrawPic( duk_context *ctx ) {
	const float x = (float)duk_require_number( ctx, 0 );
	const float y = (float)duk_require_number( ctx, 1 );
	const float w = (float)duk_require_number( ctx, 2 );
	const float h = (float)duk_require_number( ctx, 3 );

	if ( duk_is_number( ctx, 4 ) ) {
		const qhandle_t shader = (qhandle_t)duk_to_int( ctx, 4 );
		SCR_DrawPic( x, y, w, h, shader );
	} else {
		const char *name = duk_require_string( ctx, 4 );
		SCR_DrawNamedPic( x, y, w, h, name );
	}
	return 0;
}

static duk_ret_t Js_Binding_HudDrawText( duk_context *ctx ) {
	const int x = duk_require_int( ctx, 0 );
	const int y = duk_require_int( ctx, 1 );
	const char *text = duk_require_string( ctx, 2 );
	const float size = ( duk_get_top( ctx ) > 3 ) ? (float)duk_to_number( ctx, 3 ) : 8.0f;

	SCR_DrawStringExt( x, y, size, text, g_color_table[ColorIndex( COLOR_WHITE )], qtrue, qfalse );
	return 0;
}
#endif

static duk_ret_t Js_Binding_On( duk_context *ctx ) {
	duk_uarridx_t len;
	duk_uarridx_t i;
	const char *eventName = duk_require_string( ctx, 0 );

	if ( !JsDebug_IsSupportedEvent( eventName ) ) {
		return duk_error( ctx, DUK_ERR_TYPE_ERROR, "unsupported event '%s'", eventName );
	}
	if ( !duk_is_function( ctx, 1 ) ) {
		return duk_error( ctx, DUK_ERR_TYPE_ERROR, "callback must be a function" );
	}
	if ( !js_allowEvents || !js_allowEvents->integer ) {
		return duk_error( ctx, DUK_ERR_ERROR, "event registration denied (js_allowEvents=0)" );
	}

	if ( !JsDebug_GetEventCallbacksArray( ctx, eventName, qtrue ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "failed to create callback list for '%s'", eventName );
	}
	len = duk_get_length( ctx, -1 );
	if ( len >= MAX_JS_EVENT_CALLBACKS ) {
		duk_pop( ctx );
		return duk_error( ctx, DUK_ERR_ERROR, "too many '%s' callbacks (max %d)", eventName, MAX_JS_EVENT_CALLBACKS );
	}

	for ( i = 0; i < len; i++ ) {
		duk_get_prop_index( ctx, -1, i );
		if ( duk_strict_equals( ctx, -1, 1 ) ) {
			duk_pop_2( ctx );
			duk_push_true( ctx );
			return 1;
		}
		duk_pop( ctx );
	}

	duk_dup( ctx, 1 );
	duk_put_prop_index( ctx, -2, len );
	duk_pop( ctx );

	duk_push_true( ctx );
	return 1;
}

static duk_ret_t Js_Binding_Off( duk_context *ctx ) {
	duk_uarridx_t len;
	duk_uarridx_t i;
	const char *eventName = duk_require_string( ctx, 0 );
	qboolean removeAll = ( duk_get_top( ctx ) < 2 || !duk_is_function( ctx, 1 ) );

	if ( !JsDebug_IsSupportedEvent( eventName ) ) {
		return duk_error( ctx, DUK_ERR_TYPE_ERROR, "unsupported event '%s'", eventName );
	}

	if ( !JsDebug_GetEventCallbacksArray( ctx, eventName, qfalse ) ) {
		duk_push_true( ctx );
		return 1;
	}
	len = duk_get_length( ctx, -1 );

	if ( removeAll ) {
		duk_set_length( ctx, -1, 0 );
		duk_pop( ctx );
		duk_push_true( ctx );
		return 1;
	}

	duk_push_array( ctx );
	for ( i = 0; i < len; i++ ) {
		duk_uarridx_t nextLen;
		duk_get_prop_index( ctx, -2, i );
		if ( duk_strict_equals( ctx, -1, 1 ) ) {
			duk_pop( ctx );
			continue;
		}
		nextLen = duk_get_length( ctx, -1 );
		duk_put_prop_index( ctx, -2, nextLen );
	}

	duk_push_global_stash( ctx );
	if ( duk_get_prop_string( ctx, -1, JS_STASH_EVENTS ) && duk_is_object( ctx, -1 ) ) {
		duk_dup( ctx, -3 );
		duk_put_prop_string( ctx, -2, eventName );
	}
	duk_pop_4( ctx );

	duk_push_true( ctx );
	return 1;
}

static void JsDebug_RegisterBindings( void ) {
	duk_push_global_object( s_jsContext );

	duk_push_c_function( s_jsContext, Js_Binding_Print, DUK_VARARGS );
	duk_put_prop_string( s_jsContext, -2, "print" );

	duk_push_object( s_jsContext );

	duk_push_c_function( s_jsContext, Js_Binding_Print, DUK_VARARGS );
	duk_put_prop_string( s_jsContext, -2, "print" );

	duk_push_c_function( s_jsContext, Js_Binding_CvarGet, 1 );
	duk_put_prop_string( s_jsContext, -2, "cvarGet" );

	duk_push_c_function( s_jsContext, Js_Binding_CvarSet, 2 );
	duk_put_prop_string( s_jsContext, -2, "cvarSet" );

	duk_push_c_function( s_jsContext, Js_Binding_Exec, DUK_VARARGS );
	duk_put_prop_string( s_jsContext, -2, "exec" );

	duk_push_c_function( s_jsContext, Js_Binding_ReadFile, 1 );
	duk_put_prop_string( s_jsContext, -2, "readFile" );

	duk_push_c_function( s_jsContext, Js_Binding_WriteFile, 2 );
	duk_put_prop_string( s_jsContext, -2, "writeFile" );

	duk_push_c_function( s_jsContext, Js_Binding_AppendFile, 2 );
	duk_put_prop_string( s_jsContext, -2, "appendFile" );

	duk_push_c_function( s_jsContext, Js_Binding_Include, 1 );
	duk_put_prop_string( s_jsContext, -2, "include" );

	duk_push_c_function( s_jsContext, Js_Binding_On, 2 );
	duk_put_prop_string( s_jsContext, -2, "on" );

	duk_push_c_function( s_jsContext, Js_Binding_Off, DUK_VARARGS );
	duk_put_prop_string( s_jsContext, -2, "off" );

#ifndef DEDICATED
	duk_push_c_function( s_jsContext, Js_Binding_TextureLoad, DUK_VARARGS );
	duk_put_prop_string( s_jsContext, -2, "textureLoad" );

	duk_push_c_function( s_jsContext, Js_Binding_TextureReload, 1 );
	duk_put_prop_string( s_jsContext, -2, "textureReload" );

	duk_push_c_function( s_jsContext, Js_Binding_MaterialRegister, 1 );
	duk_put_prop_string( s_jsContext, -2, "materialRegister" );

	duk_push_c_function( s_jsContext, Js_Binding_HudDrawPic, 5 );
	duk_put_prop_string( s_jsContext, -2, "hudDrawPic" );

	duk_push_c_function( s_jsContext, Js_Binding_HudDrawText, DUK_VARARGS );
	duk_put_prop_string( s_jsContext, -2, "hudDrawText" );
#endif

	duk_push_string( s_jsContext, "Duktape" );
	duk_put_prop_string( s_jsContext, -2, "engine" );

	duk_put_prop_string( s_jsContext, -2, "idtech3" );
	duk_pop( s_jsContext );
}

static qboolean JsDebug_OpenState( void ) {
	JsDebug_InitPolicyCvars();

	if ( s_jsContext ) {
		return qtrue;
	}

	s_jsContext = duk_create_heap_default();
	if ( !s_jsContext ) {
		Com_Printf( S_COLOR_RED "JavaScript: failed to initialize Duktape VM\n" );
		return qfalse;
	}

	JsDebug_RegisterBindings();
	return qtrue;
}

static qboolean JsDebug_EvalText( const char *label, const char *source, size_t sourceLen, qboolean printResult ) {
	const int evalResult = duk_peval_lstring( s_jsContext, source, sourceLen );

	if ( evalResult != 0 ) {
		JsDebug_PrintJsError( label );
		return qfalse;
	}

	if ( printResult && !duk_is_undefined( s_jsContext, -1 ) ) {
		const char *result = duk_safe_to_string( s_jsContext, -1 );
		Com_Printf( "JavaScript: %s\n", result ? result : "undefined" );
	}

	duk_pop( s_jsContext );
	return qtrue;
}

static qboolean JsDebug_LoadScript( const char *scriptPath ) {
	void *buffer = NULL;
	int len;

	if ( !JsDebug_OpenState() ) {
		return qfalse;
	}

	if ( !JsDebug_IsSafePath( scriptPath ) ) {
		Com_Printf( S_COLOR_RED "JavaScript: unsafe path '%s'\n", scriptPath );
		return qfalse;
	}
	if ( !JsDebug_IsAllowedScriptPath( scriptPath ) ) {
		Com_Printf( S_COLOR_RED "JavaScript: denied script path '%s' (allowed: ui/, client/, frontend/, scripts/js/)\n", scriptPath );
		return qfalse;
	}

	len = FS_ReadFile( scriptPath, &buffer );
	if ( len < 0 || !buffer ) {
		Com_Printf( S_COLOR_RED "JavaScript: %s: could not read file\n", scriptPath );
		return qfalse;
	}

	if ( !JsDebug_EvalText( scriptPath, (const char *)buffer, (size_t)len, qfalse ) ) {
		FS_FreeFile( buffer );
		return qfalse;
	}

	FS_FreeFile( buffer );
	return qtrue;
}

static void JsDebug_TrackScript( const char *scriptPath ) {
	int i;

	for ( i = 0; i < s_jsTrackedCount; i++ ) {
		if ( !Q_stricmp( s_jsTrackedScripts[i], scriptPath ) ) {
			return;
		}
	}

	if ( s_jsTrackedCount >= MAX_JS_TRACKED_SCRIPTS ) {
		Com_Printf( S_COLOR_YELLOW "JavaScript: tracked script limit reached (%d)\n", MAX_JS_TRACKED_SCRIPTS );
		return;
	}

	Q_strncpyz( s_jsTrackedScripts[s_jsTrackedCount], scriptPath, sizeof( s_jsTrackedScripts[0] ) );
	s_jsTrackedCount++;
}

void JsDebug_Frame( int msec, int realMsec ) {
	duk_uarridx_t len;
	duk_uarridx_t i;

	if ( !s_jsContext ) {
		return;
	}

	JsDebug_InitPolicyCvars();
	if ( !js_allowEvents || !js_allowEvents->integer ) {
		return;
	}

	if ( !JsDebug_GetEventCallbacksArray( s_jsContext, "frame", qfalse ) ) {
		return;
	}
	len = duk_get_length( s_jsContext, -1 );

	for ( i = 0; i < len; i++ ) {
		duk_get_prop_index( s_jsContext, -1, i );
		duk_push_int( s_jsContext, msec );
		duk_push_int( s_jsContext, realMsec );
		if ( duk_pcall( s_jsContext, 2 ) != DUK_EXEC_SUCCESS ) {
			const char *msg = duk_safe_to_string( s_jsContext, -1 );
			Com_Printf( S_COLOR_RED "JavaScript: frame callback error: %s\n", msg ? msg : "(unknown error)" );
		}
		duk_pop( s_jsContext );
	}

	duk_pop( s_jsContext );
}

void JsDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 ) {
	duk_uarridx_t len;
	duk_uarridx_t i;

	if ( !s_jsContext || !JsDebug_IsSupportedEvent( eventName ) || !Q_stricmp( eventName, "frame" ) ) {
		return;
	}

	JsDebug_InitPolicyCvars();
	if ( !js_allowEvents || !js_allowEvents->integer ) {
		return;
	}

	if ( !JsDebug_GetEventCallbacksArray( s_jsContext, eventName, qfalse ) ) {
		return;
	}
	len = duk_get_length( s_jsContext, -1 );

	for ( i = 0; i < len; i++ ) {
		duk_get_prop_index( s_jsContext, -1, i );
		duk_push_object( s_jsContext );

		duk_push_string( s_jsContext, eventName );
		duk_put_prop_string( s_jsContext, -2, "name" );
		if ( s0 ) {
			duk_push_string( s_jsContext, s0 );
			duk_put_prop_string( s_jsContext, -2, "s0" );
		}
		if ( s1 ) {
			duk_push_string( s_jsContext, s1 );
			duk_put_prop_string( s_jsContext, -2, "s1" );
		}
		duk_push_int( s_jsContext, i0 );
		duk_put_prop_string( s_jsContext, -2, "i0" );
		duk_push_int( s_jsContext, i1 );
		duk_put_prop_string( s_jsContext, -2, "i1" );

		if ( !Q_stricmp( eventName, "map_load" ) && s0 ) {
			duk_push_string( s_jsContext, s0 );
			duk_put_prop_string( s_jsContext, -2, "map" );
		}
		if ( !Q_stricmp( eventName, "client_connect" ) ) {
			if ( s0 ) {
				duk_push_string( s_jsContext, s0 );
				duk_put_prop_string( s_jsContext, -2, "address" );
			}
			duk_push_int( s_jsContext, i0 );
			duk_put_prop_string( s_jsContext, -2, "clientNum" );
		}
		if ( !Q_stricmp( eventName, "menu_changed" ) ) {
			if ( s0 ) {
				duk_push_string( s_jsContext, s0 );
				duk_put_prop_string( s_jsContext, -2, "menu" );
			}
			duk_push_int( s_jsContext, i0 );
			duk_put_prop_string( s_jsContext, -2, "menuId" );
		}
		if ( !Q_stricmp( eventName, "input_key" ) ) {
			if ( s0 ) {
				duk_push_string( s_jsContext, s0 );
				duk_put_prop_string( s_jsContext, -2, "key" );
			}
			duk_push_int( s_jsContext, i0 );
			duk_put_prop_string( s_jsContext, -2, "keyNum" );
			duk_push_boolean( s_jsContext, i1 ? qtrue : qfalse );
			duk_put_prop_string( s_jsContext, -2, "down" );
		}
		if ( !Q_stricmp( eventName, "mouse_move" ) ) {
			duk_push_int( s_jsContext, i0 );
			duk_put_prop_string( s_jsContext, -2, "dx" );
			duk_push_int( s_jsContext, i1 );
			duk_put_prop_string( s_jsContext, -2, "dy" );
		}
		if ( !Q_stricmp( eventName, "ui_open" ) || !Q_stricmp( eventName, "ui_close" ) || !Q_stricmp( eventName, "console_open" ) ) {
			duk_push_int( s_jsContext, i0 );
			duk_put_prop_string( s_jsContext, -2, "catcher" );
		}

		if ( duk_pcall( s_jsContext, 1 ) != DUK_EXEC_SUCCESS ) {
			const char *msg = duk_safe_to_string( s_jsContext, -1 );
			Com_Printf( S_COLOR_RED "JavaScript: %s callback error: %s\n", eventName, msg ? msg : "(unknown error)" );
		}
		duk_pop( s_jsContext );
	}

	duk_pop( s_jsContext );
}

void Cmd_JsReload_f( void ) {
	int argc = Cmd_Argc();
	int i;
	int successCount = 0;
	int failureCount = 0;

	if ( argc <= 1 ) {
		if ( s_jsTrackedCount <= 0 ) {
			JsDebug_CloseState();
			if ( JsDebug_OpenState() ) {
				Com_Printf( "JavaScript: runtime initialized (Duktape)\n" );
			}
			return;
		}

		JsDebug_CloseState();
		if ( !JsDebug_OpenState() ) {
			return;
		}

		for ( i = 0; i < s_jsTrackedCount; i++ ) {
			if ( JsDebug_LoadScript( s_jsTrackedScripts[i] ) ) {
				successCount++;
			} else {
				failureCount++;
			}
		}

		Com_Printf( "JavaScript: reloaded %d tracked script(s), %d failure(s)\n", successCount, failureCount );
		return;
	}

	JsDebug_CloseState();
	JsDebug_ClearTrackedScripts();

	if ( !JsDebug_OpenState() ) {
		return;
	}

	for ( i = 1; i < argc; i++ ) {
		const char *scriptPath = Cmd_Argv( i );

		if ( !scriptPath || !scriptPath[0] ) {
			continue;
		}

		if ( JsDebug_LoadScript( scriptPath ) ) {
			JsDebug_TrackScript( scriptPath );
			successCount++;
		} else {
			failureCount++;
		}
	}

	Com_Printf( "JavaScript: loaded %d script(s), %d failure(s)\n", successCount, failureCount );
}

void Cmd_JsList_f( void ) {
	int i;

	JsDebug_InitPolicyCvars();
	Com_Printf( "JavaScript: compile-time API Duktape\n" );
	Com_Printf( "JavaScript: policy js_allowEvents=%d js_allowExec=%d js_cvarSetMode=%d js_allowFileWrite=%d\n",
		js_allowEvents ? js_allowEvents->integer : 0,
		js_allowExec ? js_allowExec->integer : 0,
		js_cvarSetMode ? js_cvarSetMode->integer : 0,
		js_allowFileWrite ? js_allowFileWrite->integer : 0 );

	if ( !s_jsContext ) {
		Com_Printf( "JavaScript: runtime not initialized. Run js_reload first.\n" );
		return;
	}

	Com_Printf( "JavaScript: runtime initialized\n" );
	Com_Printf( "JavaScript: API namespace idtech3 (print, cvarGet, cvarSet, exec, readFile, writeFile, appendFile, include, on, off, textureLoad, textureReload, materialRegister, hudDrawPic, hudDrawText)\n" );
	Com_Printf( "JavaScript: script path policy ui/, client/, frontend/, scripts/js/\n" );
	Com_Printf( "JavaScript: callbacks frame=%d map_load=%d client_connect=%d ui_open=%d ui_close=%d menu_changed=%d input_key=%d mouse_move=%d console_open=%d\n",
		JsDebug_EventCallbackCount( "frame" ),
		JsDebug_EventCallbackCount( "map_load" ),
		JsDebug_EventCallbackCount( "client_connect" ),
		JsDebug_EventCallbackCount( "ui_open" ),
		JsDebug_EventCallbackCount( "ui_close" ),
		JsDebug_EventCallbackCount( "menu_changed" ),
		JsDebug_EventCallbackCount( "input_key" ),
		JsDebug_EventCallbackCount( "mouse_move" ),
		JsDebug_EventCallbackCount( "console_open" ) );
	Com_Printf( "JavaScript: tracked scripts (%d)\n", s_jsTrackedCount );

	for ( i = 0; i < s_jsTrackedCount; i++ ) {
		Com_Printf( "  %2d: %s\n", i + 1, s_jsTrackedScripts[i] );
	}
}

void Cmd_JsDump_f( void ) {
	int maxEntries = 128;
	int printed = 0;

	if ( Cmd_Argc() > 1 ) {
		const int requested = atoi( Cmd_Argv( 1 ) );
		if ( requested > 0 ) {
			maxEntries = requested;
		}
	}

	if ( !s_jsContext ) {
		Com_Printf( "JavaScript: runtime not initialized. Run js_reload first.\n" );
		return;
	}

	duk_push_global_object( s_jsContext );
	duk_enum( s_jsContext, -1, DUK_ENUM_OWN_PROPERTIES_ONLY );

	Com_Printf( "JavaScript: globals (limit %d)\n", maxEntries );
	while ( duk_next( s_jsContext, -1, 1 ) ) {
		const char *keyName = duk_safe_to_string( s_jsContext, -2 );
		const char *valueType = JsDebug_TypeName( duk_get_type( s_jsContext, -1 ) );

		Com_Printf( "  %s : %s\n", keyName ? keyName : "(unknown)", valueType );

		duk_pop_2( s_jsContext );
		printed++;
		if ( printed >= maxEntries ) {
			Com_Printf( "  ... output truncated ...\n" );
			break;
		}
	}

	duk_pop_2( s_jsContext );
	Com_Printf( "JavaScript: dumped %d global entr%s\n", printed, printed == 1 ? "y" : "ies" );
}

void Cmd_JsExec_f( void ) {
	const int argc = Cmd_Argc();
	const char *source;

	if ( argc <= 1 ) {
		Com_Printf( "Usage: js_exec <javascript-expression-or-code>\n" );
		return;
	}

	if ( !JsDebug_OpenState() ) {
		return;
	}

	source = Cmd_ArgsFrom( 1 );
	if ( !source || !source[0] ) {
		Com_Printf( "JavaScript: empty source\n" );
		return;
	}

	JsDebug_EvalText( "js_exec", source, strlen( source ), qtrue );
}

#else

void Cmd_JsReload_f( void ) {
	Com_Printf( "JavaScript support is disabled in this build. Configure with -DUSE_DUKTAPE=ON.\n" );
}

void Cmd_JsList_f( void ) {
	Com_Printf( "JavaScript support is disabled in this build. Configure with -DUSE_DUKTAPE=ON.\n" );
}

void Cmd_JsDump_f( void ) {
	Com_Printf( "JavaScript support is disabled in this build. Configure with -DUSE_DUKTAPE=ON.\n" );
}

void Cmd_JsExec_f( void ) {
	Com_Printf( "JavaScript support is disabled in this build. Configure with -DUSE_DUKTAPE=ON.\n" );
}

void JsDebug_Frame( int msec, int realMsec ) {
	(void)msec;
	(void)realMsec;
}

void JsDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 ) {
	(void)eventName;
	(void)s0;
	(void)s1;
	(void)i0;
	(void)i1;
}

#endif
