/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

App CRDT — semver LWW register + versioned script event queue (Wyns et al.).
Opt-in via com_app_crdt; does not alter vanilla snapshot/usercmd wire format.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "app_crdt.h"

#ifdef USE_LUA
#include "lua_debug.h"
#endif

static appCrdtVersion_t s_localVersion;

#ifndef APP_CRDT_UNIT_TEST
static cvar_t *com_app_crdt;
static cvar_t *com_app_crdt_sign;
static cvar_t *com_app_crdt_version;
static cvar_t *com_app_crdt_queue_max;
static cvar_t *com_app_crdt_auto;
static cvar_t *com_app_crdt_backend;
static cvar_t *com_app_crdt_backend_root;
static char s_backendRoot[MAX_OSPATH];
#endif

#define APP_CRDT_BACKEND_MANIFEST_REL "app_crdt/manifest.json"

#ifndef APP_CRDT_UNIT_TEST
static qboolean AppCrdt_OsPathHasFile( const char *osPath )
{
	fileOffset_t size;
	fileTime_t mtime, ctime;

	if ( !osPath || !osPath[0] ) {
		return qfalse;
	}
	return Sys_GetFileStats( osPath, &size, &mtime, &ctime );
}

static qboolean AppCrdt_ReadOsFile( const char *osPath, void **bufOut, int *lenOut )
{
	FILE *f;
	long flen;
	byte *buf;

	if ( !osPath || !osPath[0] || !bufOut || !lenOut ) {
		return qfalse;
	}

	*bufOut = NULL;
	*lenOut = 0;

	f = Sys_FOpen( osPath, "rb" );
	if ( !f ) {
		return qfalse;
	}

	if ( fseek( f, 0, SEEK_END ) != 0 ) {
		fclose( f );
		return qfalse;
	}
	flen = ftell( f );
	if ( flen <= 0 || flen > 1024 * 1024 ) {
		fclose( f );
		return qfalse;
	}
	if ( fseek( f, 0, SEEK_SET ) != 0 ) {
		fclose( f );
		return qfalse;
	}

	buf = (byte *)Z_Malloc( (int)flen + 1 );
	if ( !buf ) {
		fclose( f );
		return qfalse;
	}
	if ( (long)fread( buf, 1, (size_t)flen, f ) != flen ) {
		fclose( f );
		Z_Free( buf );
		return qfalse;
	}
	fclose( f );
	buf[flen] = '\0';
	*bufOut = buf;
	*lenOut = (int)flen;
	return qtrue;
}

static qboolean AppCrdt_TrySetBackendRoot( const char *root )
{
	char test[MAX_OSPATH];

	if ( !root || !root[0] ) {
		return qfalse;
	}
	Com_sprintf( test, sizeof( test ), "%s/%s", root, APP_CRDT_BACKEND_MANIFEST_REL );
	if ( !AppCrdt_OsPathHasFile( test ) ) {
		return qfalse;
	}
	Q_strncpyz( s_backendRoot, root, sizeof( s_backendRoot ) );
#ifdef USE_LUA
	LuaDebug_SetScriptFallbackRoot( s_backendRoot );
#endif
	return qtrue;
}

void AppCrdt_SetBackendRoot( const char *root )
{
	if ( root && root[0] ) {
		AppCrdt_TrySetBackendRoot( root );
	}
}

const char *AppCrdt_GetBackendRoot( void )
{
	return s_backendRoot[0] ? s_backendRoot : NULL;
}

qboolean AppCrdt_BackendAvailable( void )
{
	return s_backendRoot[0] != '\0';
}

qboolean AppCrdt_GetDefaultBackendManifest( char *buf, int buflen )
{
	if ( !buf || buflen <= 0 ) {
		return qfalse;
	}
	Q_strncpyz( buf, APP_CRDT_BACKEND_MANIFEST_REL, buflen );
	return AppCrdt_BackendAvailable();
}

qboolean AppCrdt_ResolveBackendOsPath( const char *qpath, char *osOut, int osLen )
{
	if ( !qpath || !qpath[0] || !osOut || osLen <= 0 || !s_backendRoot[0] ) {
		return qfalse;
	}
	Com_sprintf( osOut, osLen, "%s/%s", s_backendRoot, qpath );
	return AppCrdt_OsPathHasFile( osOut );
}

void AppCrdt_RefreshBackendRoot( void )
{
	const char *candidates[4];
	int i;

	if ( !com_app_crdt_backend || !com_app_crdt_backend->integer ) {
		s_backendRoot[0] = '\0';
#ifdef USE_LUA
		LuaDebug_SetScriptFallbackRoot( NULL );
#endif
		return;
	}

	s_backendRoot[0] = '\0';
	candidates[0] = com_app_crdt_backend_root ? com_app_crdt_backend_root->string : NULL;
#ifdef IDTECH3_BACKEND_DIR
	candidates[1] = IDTECH3_BACKEND_DIR;
#else
	candidates[1] = NULL;
#endif
	{
		static char relFromBase[MAX_OSPATH];
		const char *base = Cvar_VariableString( "fs_basepath" );
		if ( base && base[0] ) {
			Com_sprintf( relFromBase, sizeof( relFromBase ), "%s/src/external/idtech3backend", base );
			candidates[2] = relFromBase;
		} else {
			candidates[2] = NULL;
		}
	}
	candidates[3] = NULL;

	for ( i = 0; candidates[i]; i++ ) {
		if ( AppCrdt_TrySetBackendRoot( candidates[i] ) ) {
			Com_Printf( "[AppCRDT] backend root: %s\n", s_backendRoot );
			return;
		}
	}
}
#endif /* APP_CRDT_UNIT_TEST */

qboolean AppCrdt_ParseVersion( const char *text, appCrdtVersion_t *out )
{
	int major = 0;
	int minor = 0;
	int patch = 0;

	if ( !text || !text[0] || !out ) {
		return qfalse;
	}

	if ( sscanf( text, "%d.%d.%d", &major, &minor, &patch ) < 1 ) {
		return qfalse;
	}

	out->major = major;
	out->minor = minor;
	out->patch = patch;
	return qtrue;
}

void AppCrdt_FormatVersion( const appCrdtVersion_t *ver, char *buf, int buflen )
{
	if ( !buf || buflen <= 0 ) {
		return;
	}
	if ( !ver ) {
		Q_strncpyz( buf, "0.0.0", buflen );
		return;
	}
	Com_sprintf( buf, buflen, "%d.%d.%d", ver->major, ver->minor, ver->patch );
}

int AppCrdt_CompareVersion( const appCrdtVersion_t *a, const appCrdtVersion_t *b )
{
	if ( !a && !b ) {
		return 0;
	}
	if ( !a ) {
		return -1;
	}
	if ( !b ) {
		return 1;
	}
	if ( a->major != b->major ) {
		return a->major - b->major;
	}
	if ( a->minor != b->minor ) {
		return a->minor - b->minor;
	}
	return a->patch - b->patch;
}

qboolean AppCrdt_MergeLWW( appCrdtVersion_t *local, const appCrdtVersion_t *remote )
{
	if ( !local || !remote ) {
		return qfalse;
	}
	if ( AppCrdt_CompareVersion( remote, local ) > 0 ) {
		*local = *remote;
		return qtrue;
	}
	return qfalse;
}

static qboolean AppCrdt_ParseStringArrayEntry( const char *json, const char *key, char paths[][MAX_OSPATH],
	int maxPaths, int *countOut )
{
	char search[64];
	const char *p;
	const char *arr;
	int count = 0;

	if ( countOut ) {
		*countOut = 0;
	}
	if ( !json || !key || !paths || maxPaths <= 0 ) {
		return qfalse;
	}

	Com_sprintf( search, sizeof( search ), "\"%s\":", key );
	p = strstr( json, search );
	if ( !p ) {
		return qfalse;
	}
	arr = strchr( p, '[' );
	if ( !arr ) {
		return qfalse;
	}
	arr++;

	while ( count < maxPaths ) {
		const char *q1 = strchr( arr, '"' );
		if ( !q1 ) {
			break;
		}
		const char *q2 = strchr( q1 + 1, '"' );
		if ( !q2 ) {
			break;
		}
		{
			int len = (int)( q2 - ( q1 + 1 ) );
			if ( len >= MAX_OSPATH ) {
				len = MAX_OSPATH - 1;
			}
			Q_strncpyz( paths[count], q1 + 1, len + 1 );
			count++;
		}
		arr = q2 + 1;
		if ( strchr( arr, ']' ) && !strchr( arr, '"' ) ) {
			break;
		}
	}

	if ( countOut ) {
		*countOut = count;
	}
	return count > 0;
}

qboolean AppCrdt_ParseManifestJson( const char *jsonText, appCrdtSpec_t *spec )
{
	char versionBuf[32];
	const char *p;

	if ( !jsonText || !spec ) {
		return qfalse;
	}

	Com_Memset( spec, 0, sizeof( *spec ) );

	p = strstr( jsonText, "\"version\"" );
	if ( p ) {
		const char *colon = strchr( p, ':' );
		const char *q1;
		const char *q2;
		if ( colon ) {
			q1 = strchr( colon, '"' );
			if ( q1 ) {
				q2 = strchr( q1 + 1, '"' );
				if ( q2 ) {
					int len = (int)( q2 - ( q1 + 1 ) );
					if ( len >= (int)sizeof( versionBuf ) ) {
						len = (int)sizeof( versionBuf ) - 1;
					}
					Q_strncpyz( versionBuf, q1 + 1, len + 1 );
					if ( !AppCrdt_ParseVersion( versionBuf, &spec->version ) ) {
						return qfalse;
					}
				}
			}
		}
	}

	if ( !AppCrdt_ParseStringArrayEntry( jsonText, "scripts", spec->scriptPaths,
		APP_CRDT_MAX_SCRIPTS, &spec->scriptCount ) ) {
		return qfalse;
	}

	return qtrue;
}

qboolean AppCrdt_LoadManifest( const char *manifestPath, appCrdtSpec_t *spec )
{
#ifndef APP_CRDT_UNIT_TEST
	void *buf = NULL;
	int len;
	qboolean ok;
	qboolean fromOs = qfalse;

	if ( !manifestPath || !manifestPath[0] || !spec ) {
	 return qfalse;
	}

	len = FS_ReadFile( manifestPath, &buf );
	if ( len <= 0 || !buf ) {
		char osPath[MAX_OSPATH];
		void *osBuf = NULL;
		int osLen = 0;

		if ( AppCrdt_ResolveBackendOsPath( manifestPath, osPath, sizeof( osPath ) ) &&
			AppCrdt_ReadOsFile( osPath, &osBuf, &osLen ) ) {
			buf = osBuf;
			len = osLen;
			fromOs = qtrue;
		} else {
			Com_sprintf( osPath, sizeof( osPath ), "%s/%s",
				AppCrdt_GetBackendRoot() ? AppCrdt_GetBackendRoot() : "",
				manifestPath );
			if ( AppCrdt_GetBackendRoot() && AppCrdt_ReadOsFile( osPath, &osBuf, &osLen ) ) {
				buf = osBuf;
				len = osLen;
				fromOs = qtrue;
			} else {
				Com_Printf( S_COLOR_YELLOW "[AppCRDT] manifest not found: %s\n", manifestPath );
				return qfalse;
			}
		}
	}

	ok = AppCrdt_ParseManifestJson( (const char *)buf, spec );
	if ( ok ) {
		Q_strncpyz( spec->manifestPath, manifestPath, sizeof( spec->manifestPath ) );
	}
	if ( fromOs ) {
		Z_Free( buf );
	} else {
		FS_FreeFile( buf );
	}
	return ok;
#else
	(void)manifestPath;
	(void)spec;
	return qfalse;
#endif
}

#if defined( APP_CRDT_UNIT_TEST )
void AppCrdt_SetBackendRoot( const char *root ) { (void)root; }
const char *AppCrdt_GetBackendRoot( void ) { return NULL; }
qboolean AppCrdt_BackendAvailable( void ) { return qfalse; }
qboolean AppCrdt_GetDefaultBackendManifest( char *buf, int buflen )
{
	if ( buf && buflen > 0 ) {
		buf[0] = '\0';
	}
	return qfalse;
}
qboolean AppCrdt_ResolveBackendOsPath( const char *qpath, char *osOut, int osLen )
{
	(void)qpath; (void)osOut; (void)osLen;
	return qfalse;
}
void AppCrdt_RefreshBackendRoot( void ) {}
#endif

void AppCrdt_QueueInit( appCrdtQueue_t *queue, int capacity,
	appCrdtDeliverFn deliver, appCrdtAdaptFn adapt, void *userData )
{
	if ( !queue ) {
		return;
	}
	Com_Memset( queue, 0, sizeof( *queue ) );
	queue->capacity = capacity > 0 ? capacity : APP_CRDT_DEFAULT_QUEUE;
	if ( queue->capacity > APP_CRDT_DEFAULT_QUEUE ) {
		queue->capacity = APP_CRDT_DEFAULT_QUEUE;
	}
	queue->deliver = deliver;
	queue->adapt = adapt;
	queue->userData = userData;
}

static void AppCrdt_QueuePush( appCrdtQueue_t *queue, int msgMajor, const char *payload )
{
	appCrdtEvent_t *slot;

	if ( !queue || !payload ) {
		return;
	}

	if ( queue->count >= queue->capacity ) {
#ifndef APP_CRDT_UNIT_TEST
		Com_Printf( S_COLOR_YELLOW "[AppCRDT] event queue full, dropping oldest\n" );
#endif
		queue->head = ( queue->head + 1 ) % queue->capacity;
		queue->count--;
	}

	slot = &queue->events[( queue->head + queue->count ) % queue->capacity];
	slot->msgMajor = msgMajor;
	Q_strncpyz( slot->payload, payload, sizeof( slot->payload ) );
	queue->count++;
}

appCrdtDispatchResult_t AppCrdt_QueueDispatch( appCrdtQueue_t *queue, int localMajor,
	int msgMajor, const char *payload )
{
	if ( !queue || !payload ) {
		return APP_CRDT_DISPATCH_BUFFER;
	}

	if ( msgMajor == localMajor ) {
		if ( queue->deliver ) {
			queue->deliver( msgMajor, payload, queue->userData );
		}
		return APP_CRDT_DISPATCH_DELIVER;
	}
	if ( msgMajor > localMajor ) {
		AppCrdt_QueuePush( queue, msgMajor, payload );
		return APP_CRDT_DISPATCH_BUFFER;
	}
	if ( queue->adapt ) {
		queue->adapt( msgMajor, payload, queue->userData );
	}
	return APP_CRDT_DISPATCH_ADAPT;
}

int AppCrdt_QueueFlushUpToMajor( appCrdtQueue_t *queue, int localMajor )
{
	int delivered = 0;

	if ( !queue ) {
		return 0;
	}

	while ( queue->count > 0 ) {
		appCrdtEvent_t *slot = &queue->events[queue->head];
		if ( slot->msgMajor > localMajor ) {
			break;
		}
		if ( queue->deliver ) {
			queue->deliver( slot->msgMajor, slot->payload, queue->userData );
		}
		queue->head = ( queue->head + 1 ) % queue->capacity;
		queue->count--;
		delivered++;
	}
	return delivered;
}

void AppCrdt_QueueClear( appCrdtQueue_t *queue )
{
	if ( !queue ) {
		return;
	}
	queue->head = 0;
	queue->count = 0;
}

qboolean AppCrdt_IsEnabled( void )
{
#ifndef APP_CRDT_UNIT_TEST
	return com_app_crdt && com_app_crdt->integer != 0;
#else
	return qfalse;
#endif
}

int AppCrdt_GetLocalMajor( void )
{
	return s_localVersion.major;
}

void AppCrdt_SetLocalVersion( const appCrdtVersion_t *ver )
{
	if ( ver ) {
		s_localVersion = *ver;
	}
}

const appCrdtVersion_t *AppCrdt_GetLocalVersion( void )
{
	return &s_localVersion;
}

void Cmd_AppCrdtStatus_f( void )
{
#ifndef APP_CRDT_UNIT_TEST
	char verBuf[32];

	AppCrdt_FormatVersion( &s_localVersion, verBuf, sizeof( verBuf ) );
	Com_Printf( "[AppCRDT] enabled=%d sign=%d local=%s authoritative=%s\n",
		AppCrdt_IsEnabled() ? 1 : 0,
		com_app_crdt_sign ? com_app_crdt_sign->integer : 0,
		verBuf,
		com_app_crdt_version ? com_app_crdt_version->string : "(unset)" );
#endif
}

#ifdef USE_LUA
#ifndef APP_CRDT_UNIT_TEST
qboolean AppCrdt_ApplyPublish( const appCrdtSpec_t *spec )
{
	int i;

	if ( !spec || spec->scriptCount <= 0 ) {
		return qfalse;
	}

	if ( !LuaDebug_BeginHotloadReload() ) {
		return qfalse;
	}

	AppCrdt_SetLocalVersion( &spec->version );

	for ( i = 0; i < spec->scriptCount; i++ ) {
		if ( !spec->scriptPaths[i][0] ) {
			continue;
		}
		if ( !LuaDebug_ReloadScriptPath( spec->scriptPaths[i] ) ) {
			Com_Printf( S_COLOR_YELLOW "[AppCRDT] failed to load %s\n", spec->scriptPaths[i] );
			return qfalse;
		}
	}

	LuaDebug_FinishHotloadReload();
	Com_Printf( "[AppCRDT] applied publish %d.%d.%d (%d scripts)\n",
		spec->version.major, spec->version.minor, spec->version.patch, spec->scriptCount );
	return qtrue;
}
#endif
#else
qboolean AppCrdt_ApplyPublish( const appCrdtSpec_t *spec )
{
	(void)spec;
	return qfalse;
}
#endif

#ifndef APP_CRDT_UNIT_TEST
int AppCrdt_GetQueueMax( void )
{
	if ( com_app_crdt_queue_max ) {
		return com_app_crdt_queue_max->integer;
	}
	return APP_CRDT_DEFAULT_QUEUE;
}

void AppCrdt_Init( void )
{
	char verBuf[32];

	com_app_crdt = Cvar_Get( "com_app_crdt", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_app_crdt,
		"Enable App CRDT distributed Lua updates (server push + versioned script events)." );
	com_app_crdt_sign = Cvar_Get( "com_app_crdt_sign", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_app_crdt_sign,
		"Require signed app bundles (stub; 0=accept unsigned)." );
	com_app_crdt_version = Cvar_Get( "com_app_crdt_version", "0.0.0", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_SetDescription( com_app_crdt_version,
		"Authoritative App CRDT semver replicated via systeminfo." );
	com_app_crdt_queue_max = Cvar_Get( "com_app_crdt_queue_max", "64", CVAR_ARCHIVE );
	Cvar_SetDescription( com_app_crdt_queue_max, "Max buffered versioned script events per node." );
	com_app_crdt_auto = Cvar_Get( "com_app_crdt_auto", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_app_crdt_auto,
		"Auto-publish idtech3backend App CRDT manifest on map load when backend is present." );
	com_app_crdt_backend = Cvar_Get( "com_app_crdt_backend", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_app_crdt_backend,
		"Enable idtech3backend submodule integration for App CRDT manifests and server/lua/ scripts." );
	com_app_crdt_backend_root = Cvar_Get( "com_app_crdt_backend_root", "", CVAR_ARCHIVE );
	Cvar_SetDescription( com_app_crdt_backend_root,
		"Override idtech3backend root (default: submodule or fs_basepath/src/external/idtech3backend)." );

	AppCrdt_ParseVersion( com_app_crdt_version->string, &s_localVersion );
	AppCrdt_RefreshBackendRoot();

	Cmd_AddCommand( "app_crdt_status", Cmd_AppCrdtStatus_f );

	if ( AppCrdt_IsEnabled() ) {
		AppCrdt_FormatVersion( &s_localVersion, verBuf, sizeof( verBuf ) );
		Com_Printf( "[AppCRDT] com_app_crdt=1 version=%s (server-authoritative Lua updates)\n", verBuf );
		if ( AppCrdt_BackendAvailable() ) {
			Com_Printf( "[AppCRDT] idtech3backend linked at %s\n", s_backendRoot );
		}
	}
}
#endif /* APP_CRDT_UNIT_TEST */
