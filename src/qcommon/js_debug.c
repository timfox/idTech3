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
#define JS_STASH_MODULES "\xff""id3.modules"

static duk_context *s_jsContext;
static int s_jsTrackedCount;
static char s_jsTrackedScripts[MAX_JS_TRACKED_SCRIPTS][MAX_OSPATH];

static cvar_t *js_allowEvents;
static cvar_t *js_allowExec;
static cvar_t *js_cvarSetMode;
static cvar_t *js_allowFileWrite;
static cvar_t *js_maxEventCallbacks;
static cvar_t *js_frameCallbackBudgetMs;
static cvar_t *js_disableFaultyCallbacks;
static cvar_t *js_requireCache;
static cvar_t *js_compatTarget;
static cvar_t *js_autoInit;

static qboolean JsDebug_OpenState( void );
static qboolean JsDebug_LoadScript( const char *scriptPath );

static void JsDebug_InitPolicyCvars( void ) {
	if ( !js_allowEvents ) {
		js_allowEvents = Cvar_Get( "js_allowEvents", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_allowEvents, "Allow JavaScript event registration via idtech3.on/off (0=disabled, 1=enabled)." );
	}
	if ( !js_allowExec ) {
		js_allowExec = Cvar_Get( "js_allowExec", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_allowExec, "JavaScript command execution level: 0=off, 1=append, 2=append+insert, 3=append+insert+now." );
	}
	if ( !js_cvarSetMode ) {
		js_cvarSetMode = Cvar_Get( "js_cvarSetMode", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_cvarSetMode, "JavaScript cvar write level: 0=off, 1=existing writable only, 2=allow creating user cvars, 3=unrestricted (still subject to Cvar_Set rules)." );
	}
	if ( !js_allowFileWrite ) {
		js_allowFileWrite = Cvar_Get( "js_allowFileWrite", "0", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_allowFileWrite, "Allow JavaScript writeFile/appendFile operations (0=disabled, 1=enabled)." );
	}
	if ( !js_maxEventCallbacks ) {
		js_maxEventCallbacks = Cvar_Get( "js_maxEventCallbacks", "64", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_maxEventCallbacks, "Max callbacks allowed per JavaScript event name (1..1024)." );
	}
	if ( !js_frameCallbackBudgetMs ) {
		js_frameCallbackBudgetMs = Cvar_Get( "js_frameCallbackBudgetMs", "2", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_frameCallbackBudgetMs, "Soft per-frame JavaScript callback budget in milliseconds (0=unlimited)." );
	}
	if ( !js_disableFaultyCallbacks ) {
		js_disableFaultyCallbacks = Cvar_Get( "js_disableFaultyCallbacks", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_disableFaultyCallbacks, "Auto-disable callback entries that throw runtime errors (0=off, 1=on)." );
	}
	if ( !js_requireCache ) {
		js_requireCache = Cvar_Get( "js_requireCache", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_requireCache, "Enable idtech3.require module cache (0=reload every call, 1=cache)." );
	}
	if ( !js_compatTarget ) {
		js_compatTarget = Cvar_Get( "js_compatTarget", "es5.1-duktape", CVAR_ROM | CVAR_PROTECTED );
		Cvar_SetDescription( js_compatTarget, "Read-only JavaScript compatibility target for scripts." );
	}
	if ( !js_autoInit ) {
		js_autoInit = Cvar_Get( "js_autoInit", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( js_autoInit, "Initialize JavaScript runtime automatically at startup (0=manual via js_reload, 1=auto)." );
	}
}

void JsDebug_InitCvars( void ) {
	JsDebug_InitPolicyCvars();
	if ( js_autoInit && js_autoInit->integer ) {
		JsDebug_OpenState();
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
	duk_uarridx_t i;
	int count = 0;

	if ( !s_jsContext ) {
		return 0;
	}

	if ( !JsDebug_GetEventCallbacksArray( s_jsContext, eventName, qfalse ) ) {
		return 0;
	}

	len = duk_get_length( s_jsContext, -1 );
	for ( i = 0; i < len; i++ ) {
		duk_get_prop_index( s_jsContext, -1, i );
		if ( duk_is_function( s_jsContext, -1 ) ) {
			count++;
		}
		duk_pop( s_jsContext );
	}
	duk_pop( s_jsContext );
	return count;
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

static qboolean JsDebug_FileExists( const char *path ) {
	void *buffer = NULL;
	const int len = FS_ReadFile( path, &buffer );
	if ( len < 0 ) {
		return qfalse;
	}
	if ( buffer ) {
		FS_FreeFile( buffer );
	}
	return qtrue;
}

static qboolean JsDebug_ResolveRequirePath( const char *requestedPath, char *resolvedPath, size_t resolvedSize ) {
	char candidate[MAX_OSPATH];

	if ( !requestedPath || !requestedPath[0] ) {
		return qfalse;
	}
	if ( !JsDebug_IsSafePath( requestedPath ) ) {
		return qfalse;
	}

	if ( JsDebug_IsAllowedScriptPath( requestedPath ) ) {
		Q_strncpyz( candidate, requestedPath, sizeof( candidate ) );
		if ( JsDebug_FileExists( candidate ) ) {
			Q_strncpyz( resolvedPath, candidate, resolvedSize );
			return qtrue;
		}

		if ( !strstr( requestedPath, ".js" ) ) {
			Com_sprintf( candidate, sizeof( candidate ), "%s.js", requestedPath );
			if ( JsDebug_FileExists( candidate ) ) {
				Q_strncpyz( resolvedPath, candidate, resolvedSize );
				return qtrue;
			}
		}
	}

	Com_sprintf( candidate, sizeof( candidate ), "scripts/js/%s", requestedPath );
	if ( JsDebug_FileExists( candidate ) ) {
		Q_strncpyz( resolvedPath, candidate, resolvedSize );
		return qtrue;
	}

	if ( !strstr( requestedPath, ".js" ) ) {
		Com_sprintf( candidate, sizeof( candidate ), "scripts/js/%s.js", requestedPath );
		if ( JsDebug_FileExists( candidate ) ) {
			Q_strncpyz( resolvedPath, candidate, resolvedSize );
			return qtrue;
		}
	}

	return qfalse;
}

static duk_ret_t Js_Binding_Require( duk_context *ctx ) {
	const char *requestedPath = duk_require_string( ctx, 0 );
	char resolvedPath[MAX_OSPATH];
	void *buffer = NULL;
	int len;
	const char *prefix = "(function(module, exports, idtech3){\n";
	const char *suffix = "\n; return module.exports;\n})";
	const size_t prefixLen = strlen( prefix );
	const size_t suffixLen = strlen( suffix );
	char *wrappedSource;
	duk_idx_t modulesIndex;

	if ( !JsDebug_ResolveRequirePath( requestedPath, resolvedPath, sizeof( resolvedPath ) ) ) {
		return duk_error( ctx, DUK_ERR_ERROR, "require failed: unresolved/denied path '%s' (allowed roots: ui/, client/, frontend/, scripts/js/)", requestedPath );
	}

	duk_push_global_stash( ctx );
	if ( !duk_get_prop_string( ctx, -1, JS_STASH_MODULES ) || !duk_is_object( ctx, -1 ) ) {
		duk_pop( ctx );
		duk_push_object( ctx );
		duk_dup( ctx, -1 );
		duk_put_prop_string( ctx, -3, JS_STASH_MODULES );
	}
	duk_remove( ctx, -2 );
	modulesIndex = duk_normalize_index( ctx, -1 );

	if ( js_requireCache && js_requireCache->integer ) {
		if ( duk_get_prop_string( ctx, modulesIndex, resolvedPath ) ) {
			duk_remove( ctx, modulesIndex );
			return 1;
		}
		duk_pop( ctx );
	}

	len = FS_ReadFile( resolvedPath, &buffer );
	if ( len < 0 || !buffer ) {
		return duk_error( ctx, DUK_ERR_ERROR, "require failed: could not read '%s'", resolvedPath );
	}

	wrappedSource = Z_Malloc( prefixLen + (size_t)len + suffixLen + 1 );
	memcpy( wrappedSource, prefix, prefixLen );
	memcpy( wrappedSource + prefixLen, buffer, (size_t)len );
	memcpy( wrappedSource + prefixLen + (size_t)len, suffix, suffixLen );
	wrappedSource[prefixLen + (size_t)len + suffixLen] = '\0';

	FS_FreeFile( buffer );
	buffer = NULL;

	if ( duk_peval_lstring( ctx, wrappedSource, prefixLen + (size_t)len + suffixLen ) != 0 ) {
		Z_Free( wrappedSource );
		return duk_error( ctx, DUK_ERR_ERROR, "require compile error in '%s': %s", resolvedPath, duk_safe_to_string( ctx, -1 ) );
	}

	Z_Free( wrappedSource );

	duk_push_object( ctx );
	duk_push_object( ctx );
	duk_dup( ctx, -1 );
	duk_put_prop_string( ctx, -3, "exports" );
	duk_push_string( ctx, resolvedPath );
	duk_put_prop_string( ctx, -3, "id" );
	duk_get_global_string( ctx, "idtech3" );

	if ( duk_pcall( ctx, 3 ) != DUK_EXEC_SUCCESS ) {
		return duk_error( ctx, DUK_ERR_ERROR, "require runtime error in '%s': %s", resolvedPath, duk_safe_to_string( ctx, -1 ) );
	}

	if ( js_requireCache && js_requireCache->integer ) {
		duk_dup( ctx, -1 );
		duk_put_prop_string( ctx, modulesIndex, resolvedPath );
	}

	duk_remove( ctx, modulesIndex );
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
	{
		const int maxCallbacks = JsDebug_ClampInt( js_maxEventCallbacks ? js_maxEventCallbacks->integer : MAX_JS_EVENT_CALLBACKS, 1, 1024 );
		int activeCallbacks = 0;
		for ( i = 0; i < len; i++ ) {
			duk_get_prop_index( ctx, -1, i );
			if ( duk_is_function( ctx, -1 ) ) {
				activeCallbacks++;
			}
			duk_pop( ctx );
		}
		if ( activeCallbacks >= maxCallbacks ) {
			duk_pop( ctx );
			return duk_error( ctx, DUK_ERR_ERROR, "too many '%s' callbacks (max %d)", eventName, maxCallbacks );
		}
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
	duk_pop_n( ctx, 4 );

	duk_push_true( ctx );
	return 1;
}

/* ========== Timer system (setTimeout/setInterval) ========== */

#define MAX_JS_TIMERS 32
typedef struct {
	int     id;
	int     fireTime;
	int     interval;
	qboolean active;
	qboolean repeating;
} jsTimer_t;
static jsTimer_t jsTimers[MAX_JS_TIMERS];
static int jsTimerNextId = 1;

static duk_ret_t Js_Binding_SetTimeout( duk_context *ctx ) {
	int i, delay;
	if ( !duk_is_function( ctx, 0 ) ) return duk_error( ctx, DUK_ERR_TYPE_ERROR, "callback must be a function" );
	delay = duk_require_int( ctx, 1 );
	for ( i = 0; i < MAX_JS_TIMERS; i++ ) {
		if ( !jsTimers[i].active ) {
			jsTimers[i].id = jsTimerNextId++;
			jsTimers[i].fireTime = Sys_Milliseconds() + delay;
			jsTimers[i].interval = 0;
			jsTimers[i].active = qtrue;
			jsTimers[i].repeating = qfalse;
			duk_push_global_stash( ctx );
			duk_dup( ctx, 0 );
			duk_put_prop_string( ctx, -2, va( "\xff""timer_%d", jsTimers[i].id ) );
			duk_pop( ctx );
			duk_push_int( ctx, jsTimers[i].id );
			return 1;
		}
	}
	duk_push_int( ctx, -1 );
	return 1;
}

static duk_ret_t Js_Binding_SetInterval( duk_context *ctx ) {
	int i, delay;
	if ( !duk_is_function( ctx, 0 ) ) return duk_error( ctx, DUK_ERR_TYPE_ERROR, "callback must be a function" );
	delay = duk_require_int( ctx, 1 );
	if ( delay < 16 ) delay = 16;
	for ( i = 0; i < MAX_JS_TIMERS; i++ ) {
		if ( !jsTimers[i].active ) {
			jsTimers[i].id = jsTimerNextId++;
			jsTimers[i].fireTime = Sys_Milliseconds() + delay;
			jsTimers[i].interval = delay;
			jsTimers[i].active = qtrue;
			jsTimers[i].repeating = qtrue;
			duk_push_global_stash( ctx );
			duk_dup( ctx, 0 );
			duk_put_prop_string( ctx, -2, va( "\xff""timer_%d", jsTimers[i].id ) );
			duk_pop( ctx );
			duk_push_int( ctx, jsTimers[i].id );
			return 1;
		}
	}
	duk_push_int( ctx, -1 );
	return 1;
}

static duk_ret_t Js_Binding_ClearTimer( duk_context *ctx ) {
	int id = duk_require_int( ctx, 0 );
	int i;
	for ( i = 0; i < MAX_JS_TIMERS; i++ ) {
		if ( jsTimers[i].active && jsTimers[i].id == id ) {
			jsTimers[i].active = qfalse;
			duk_push_global_stash( ctx );
			duk_del_prop_string( ctx, -1, va( "\xff""timer_%d", id ) );
			duk_pop( ctx );
			break;
		}
	}
	return 0;
}

/* ========== Engine info queries ========== */

static duk_ret_t Js_Binding_GetEngineInfo( duk_context *ctx ) {
	duk_push_object( ctx );
	duk_push_string( ctx, Q3_VERSION );
	duk_put_prop_string( ctx, -2, "version" );
	duk_push_string( ctx, OS_STRING );
	duk_put_prop_string( ctx, -2, "platform" );
	duk_push_string( ctx, ARCH_STRING );
	duk_put_prop_string( ctx, -2, "arch" );
	duk_push_int( ctx, Sys_Milliseconds() );
	duk_put_prop_string( ctx, -2, "uptime" );
	return 1;
}

static duk_ret_t Js_Binding_GetMilliseconds( duk_context *ctx ) {
	(void)ctx;
	duk_push_int( ctx, Sys_Milliseconds() );
	return 1;
}

/* ========== HUD drawing extensions ========== */

#ifndef DEDICATED
static duk_ret_t Js_Binding_HudSetColor( duk_context *ctx ) {
	vec4_t color;
	color[0] = (float)duk_require_number( ctx, 0 );
	color[1] = (float)duk_require_number( ctx, 1 );
	color[2] = (float)duk_require_number( ctx, 2 );
	color[3] = ( duk_get_top( ctx ) > 3 ) ? (float)duk_to_number( ctx, 3 ) : 1.0f;
	re.SetColor( color );
	return 0;
}

static duk_ret_t Js_Binding_HudDrawRect( duk_context *ctx ) {
	float x = (float)duk_require_number( ctx, 0 );
	float y = (float)duk_require_number( ctx, 1 );
	float w = (float)duk_require_number( ctx, 2 );
	float h = (float)duk_require_number( ctx, 3 );
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, cls.whiteShader );
	return 0;
}

static duk_ret_t Js_Binding_HudResetColor( duk_context *ctx ) {
	(void)ctx;
	re.SetColor( NULL );
	return 0;
}

static duk_ret_t Js_Binding_GetScreenSize( duk_context *ctx ) {
	duk_push_object( ctx );
	duk_push_int( ctx, cls.glconfig.vidWidth );
	duk_put_prop_string( ctx, -2, "width" );
	duk_push_int( ctx, cls.glconfig.vidHeight );
	duk_put_prop_string( ctx, -2, "height" );
	return 1;
}
#endif

static void JsDebug_RegisterBindings( void ) {
	duk_push_global_object( s_jsContext );

	/* Standard JS globals */
	duk_push_c_function( s_jsContext, Js_Binding_SetTimeout, 2 );
	duk_put_prop_string( s_jsContext, -2, "setTimeout" );
	duk_push_c_function( s_jsContext, Js_Binding_SetInterval, 2 );
	duk_put_prop_string( s_jsContext, -2, "setInterval" );
	duk_push_c_function( s_jsContext, Js_Binding_ClearTimer, 1 );
	duk_put_prop_string( s_jsContext, -2, "clearTimeout" );
	duk_push_c_function( s_jsContext, Js_Binding_ClearTimer, 1 );
	duk_put_prop_string( s_jsContext, -2, "clearInterval" );

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

	duk_push_c_function( s_jsContext, Js_Binding_Require, 1 );
	duk_put_prop_string( s_jsContext, -2, "require" );

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

	duk_push_c_function( s_jsContext, Js_Binding_HudSetColor, DUK_VARARGS );
	duk_put_prop_string( s_jsContext, -2, "hudSetColor" );

	duk_push_c_function( s_jsContext, Js_Binding_HudDrawRect, 4 );
	duk_put_prop_string( s_jsContext, -2, "hudDrawRect" );

	duk_push_c_function( s_jsContext, Js_Binding_HudResetColor, 0 );
	duk_put_prop_string( s_jsContext, -2, "hudResetColor" );

	duk_push_c_function( s_jsContext, Js_Binding_GetScreenSize, 0 );
	duk_put_prop_string( s_jsContext, -2, "getScreenSize" );
#endif

	duk_push_c_function( s_jsContext, Js_Binding_GetEngineInfo, 0 );
	duk_put_prop_string( s_jsContext, -2, "getEngineInfo" );

	duk_push_c_function( s_jsContext, Js_Binding_GetMilliseconds, 0 );
	duk_put_prop_string( s_jsContext, -2, "getMilliseconds" );

	duk_push_string( s_jsContext, "Duktape" );
	duk_put_prop_string( s_jsContext, -2, "engine" );
	duk_push_string( s_jsContext, js_compatTarget ? js_compatTarget->string : "es5.1-duktape" );
	duk_put_prop_string( s_jsContext, -2, "compatTarget" );

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
	const int startTime = Sys_Milliseconds();
	const int budgetMs = JsDebug_ClampInt( js_frameCallbackBudgetMs ? js_frameCallbackBudgetMs->integer : 0, 0, 1000 );
	static int s_lastBudgetWarnMs = 0;

	if ( !s_jsContext ) {
		return;
	}

	/* Process timers */
	{
		int now = Sys_Milliseconds();
		int ti;
		for ( ti = 0; ti < MAX_JS_TIMERS; ti++ ) {
			if ( !jsTimers[ti].active ) continue;
			if ( now >= jsTimers[ti].fireTime ) {
				duk_push_global_stash( s_jsContext );
				if ( duk_get_prop_string( s_jsContext, -1, va( "\xff""timer_%d", jsTimers[ti].id ) ) && duk_is_function( s_jsContext, -1 ) ) {
					if ( duk_pcall( s_jsContext, 0 ) != 0 ) {
						Com_Printf( S_COLOR_YELLOW "JS timer %d error: %s\n", jsTimers[ti].id, duk_safe_to_string( s_jsContext, -1 ) );
					}
				}
				duk_pop_2( s_jsContext );

				if ( jsTimers[ti].repeating ) {
					jsTimers[ti].fireTime = now + jsTimers[ti].interval;
				} else {
					jsTimers[ti].active = qfalse;
					duk_push_global_stash( s_jsContext );
					duk_del_prop_string( s_jsContext, -1, va( "\xff""timer_%d", jsTimers[ti].id ) );
					duk_pop( s_jsContext );
				}
			}
		}
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
		if ( budgetMs > 0 && ( Sys_Milliseconds() - startTime ) >= budgetMs ) {
			const int now = Sys_Milliseconds();
			if ( now - s_lastBudgetWarnMs > 3000 ) {
				s_lastBudgetWarnMs = now;
				Com_Printf( S_COLOR_YELLOW "JavaScript: frame callback budget reached (%d ms), skipping remaining callbacks this frame\n", budgetMs );
			}
			break;
		}

		duk_get_prop_index( s_jsContext, -1, i );
		if ( !duk_is_function( s_jsContext, -1 ) ) {
			duk_pop( s_jsContext );
			continue;
		}
		duk_push_int( s_jsContext, msec );
		duk_push_int( s_jsContext, realMsec );
		if ( duk_pcall( s_jsContext, 2 ) != DUK_EXEC_SUCCESS ) {
			const char *msg = duk_safe_to_string( s_jsContext, -1 );
			Com_Printf( S_COLOR_RED "JavaScript: frame callback error: %s\n", msg ? msg : "(unknown error)" );
			if ( js_disableFaultyCallbacks && js_disableFaultyCallbacks->integer ) {
				duk_pop( s_jsContext );
				duk_push_undefined( s_jsContext );
				duk_put_prop_index( s_jsContext, -2, i );
				continue;
			}
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
		if ( !duk_is_function( s_jsContext, -1 ) ) {
			duk_pop( s_jsContext );
			continue;
		}
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
			if ( js_disableFaultyCallbacks && js_disableFaultyCallbacks->integer ) {
				duk_pop( s_jsContext );
				duk_push_undefined( s_jsContext );
				duk_put_prop_index( s_jsContext, -2, i );
				continue;
			}
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
	Com_Printf( "JavaScript: compatibility target %s\n", js_compatTarget ? js_compatTarget->string : "es5.1-duktape" );
	Com_Printf( "JavaScript: policy js_allowEvents=%d js_allowExec=%d js_cvarSetMode=%d js_allowFileWrite=%d js_maxEventCallbacks=%d js_frameCallbackBudgetMs=%d js_disableFaultyCallbacks=%d js_requireCache=%d js_autoInit=%d\n",
		js_allowEvents ? js_allowEvents->integer : 0,
		js_allowExec ? js_allowExec->integer : 0,
		js_cvarSetMode ? js_cvarSetMode->integer : 0,
		js_allowFileWrite ? js_allowFileWrite->integer : 0,
		js_maxEventCallbacks ? js_maxEventCallbacks->integer : MAX_JS_EVENT_CALLBACKS,
		js_frameCallbackBudgetMs ? js_frameCallbackBudgetMs->integer : 0,
		js_disableFaultyCallbacks ? js_disableFaultyCallbacks->integer : 0,
		js_requireCache ? js_requireCache->integer : 1,
		js_autoInit ? js_autoInit->integer : 1 );

	if ( !s_jsContext ) {
		Com_Printf( "JavaScript: runtime not initialized. Run js_reload first.\n" );
		return;
	}

	Com_Printf( "JavaScript: runtime initialized\n" );
	Com_Printf( "JavaScript: API namespace idtech3 (print, cvarGet, cvarSet, exec, readFile, writeFile, appendFile, include, require, on, off, textureLoad, textureReload, materialRegister, hudDrawPic, hudDrawText)\n" );
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

void JsDebug_InitCvars( void ) {
}

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
