#include "q_shared.h"
#include "qcommon.h"
#include "qcommon/engine_db.h"
#include "csharp_debug.h"

#ifdef USE_CSHARP

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/appdomain.h>

#define MAX_CS_TRACKED_SCRIPTS 64
#define MAX_CS_DLL_PATH MAX_OSPATH

static MonoDomain *s_csDomain;
static MonoAssembly *s_csAssembly;
static MonoImage *s_csImage;
static MonoClass *s_csGameScriptClass;
static MonoMethod *s_csInitMethod;
static MonoMethod *s_csFrameMethod;
static MonoClass *s_csEngineClass;
static MonoMethod *s_csDispatchEventMethod;
static qboolean s_csMonoReady;

static int s_csTrackedCount;
static char s_csTrackedScripts[MAX_CS_TRACKED_SCRIPTS][MAX_OSPATH];

static cvar_t *cs_autoInit;
static cvar_t *cs_allowEvents;
static cvar_t *cs_allowExec;
static cvar_t *cs_frameCallbackBudgetMs;
static cvar_t *cs_compiler;
static cvar_t *cs_compatTarget;

static qboolean CsDebug_IsAllowedPath( const char *scriptPath ) {
	if ( !scriptPath || !scriptPath[0] ) {
		return qfalse;
	}
	return ( !Q_strncmp( scriptPath, "scripts/csharp/", 15 ) ||
		!Q_strncmp( scriptPath, "gameplay/", 9 ) ||
		!Q_strncmp( scriptPath, "client/", 7 ) ||
		!Q_strncmp( scriptPath, "ui/", 3 ) );
}

static void CsDebug_InitPolicyCvars( void ) {
	if ( !cs_allowEvents ) {
		cs_allowEvents = Cvar_Get( "cs_allowEvents", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( cs_allowEvents, "Allow C# event handlers via IdTech3.Engine.On (0=off, 1=on)." );
	}
	if ( !cs_allowExec ) {
		cs_allowExec = Cvar_Get( "cs_allowExec", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( cs_allowExec, "Allow IdTech3.Engine.Exec to append console commands (0=off, 1=on)." );
	}
	if ( !cs_frameCallbackBudgetMs ) {
		cs_frameCallbackBudgetMs = Cvar_Get( "cs_frameCallbackBudgetMs", "2", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( cs_frameCallbackBudgetMs, "Soft per-frame C# callback budget in ms (0=unlimited)." );
	}
	if ( !cs_compiler ) {
		cs_compiler = Cvar_Get( "cs_compiler", "mcs", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( cs_compiler, "C# compiler executable for cs_reload (.cs sources), typically mcs or csc." );
	}
	if ( !cs_compatTarget ) {
		cs_compatTarget = Cvar_Get( "cs_compatTarget", "mono-4.7-api", CVAR_ROM | CVAR_PROTECTED );
		Cvar_SetDescription( cs_compatTarget, "Read-only C# scripting API target (Mono / .NET 4.7 profile)." );
	}
	if ( !cs_autoInit ) {
		cs_autoInit = Cvar_Get( "cs_autoInit", "0", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( cs_autoInit, "Initialize Mono C# runtime at startup (0=manual cs_reload, 1=auto)." );
	}
}

static void CsDebug_ClearAssemblyRefs( void ) {
	s_csAssembly = NULL;
	s_csImage = NULL;
	s_csGameScriptClass = NULL;
	s_csInitMethod = NULL;
	s_csFrameMethod = NULL;
	s_csEngineClass = NULL;
	s_csDispatchEventMethod = NULL;
}

static void CsDebug_CloseState( void ) {
	CsDebug_ClearAssemblyRefs();
	if ( s_csDomain ) {
		mono_domain_set( s_csDomain, 0 );
	}
	s_csDomain = NULL;
	s_csMonoReady = qfalse;
}

static void id3_cs_print( MonoString *msg ) {
	char *utf8;

	if ( !msg ) {
		return;
	}
	utf8 = mono_string_to_utf8( msg );
	if ( utf8 ) {
		Com_Printf( "%s\n", utf8 );
		mono_free( utf8 );
	}
}

static MonoString *id3_cs_cvar_get( MonoString *name ) {
	char *utf8;
	const char *value;
	MonoString *result;

	if ( !name ) {
		return NULL;
	}
	utf8 = mono_string_to_utf8( name );
	if ( !utf8 ) {
		return NULL;
	}
	value = Cvar_VariableString( utf8 );
	result = mono_string_new( mono_domain_get(), value ? value : "" );
	mono_free( utf8 );
	return result;
}

static void id3_cs_cvar_set( MonoString *name, MonoString *value ) {
	char *nameUtf8;
	char *valueUtf8;

	if ( !name || !value ) {
		return;
	}
	nameUtf8 = mono_string_to_utf8( name );
	valueUtf8 = mono_string_to_utf8( value );
	if ( nameUtf8 && valueUtf8 ) {
		Cvar_Set2( nameUtf8, valueUtf8, qtrue );
	}
	if ( nameUtf8 ) {
		mono_free( nameUtf8 );
	}
	if ( valueUtf8 ) {
		mono_free( valueUtf8 );
	}
}

static int id3_cs_get_milliseconds( void ) {
	return Sys_Milliseconds();
}

static MonoString *id3_cs_get_engine_info( void ) {
	char info[256];

	Com_sprintf( info, sizeof( info ), "idtech3 mono=%s", mono_get_runtime_build_info() );
	return mono_string_new( mono_domain_get(), info );
}

static void id3_cs_exec( MonoString *command ) {
	char *utf8;

	if ( !cs_allowExec || !cs_allowExec->integer ) {
		return;
	}
	if ( !command ) {
		return;
	}
	utf8 = mono_string_to_utf8( command );
	if ( utf8 && utf8[0] ) {
		Cbuf_AddText( utf8 );
		Cbuf_AddText( "\n" );
	}
	if ( utf8 ) {
		mono_free( utf8 );
	}
}

static int id3_cs_db_available( void ) {
	return EngineDB_IsAvailable() ? 1 : 0;
}

static MonoString *id3_cs_db_path( void ) {
	return mono_string_new( mono_domain_get(), EngineDB_GetPath() ? EngineDB_GetPath() : "" );
}

static int id3_cs_db_exec( MonoString *sql ) {
	char *utf8;
	int result = 0;

	if ( !sql ) {
		return 0;
	}
	utf8 = mono_string_to_utf8( sql );
	if ( utf8 ) {
		result = EngineDB_Exec( utf8 ) ? 1 : 0;
		mono_free( utf8 );
	}
	return result;
}

static MonoString *id3_cs_db_query_one( MonoString *sql ) {
	char *utf8;
	char value[1024];
	MonoString *result = NULL;

	if ( !sql ) {
		return NULL;
	}
	utf8 = mono_string_to_utf8( sql );
	if ( utf8 && EngineDB_QueryOne( utf8, value, sizeof( value ) ) ) {
		result = mono_string_new( mono_domain_get(), value );
	}
	if ( utf8 ) {
		mono_free( utf8 );
	}
	return result;
}

static int id3_cs_profile_set( MonoString *key, MonoString *value ) {
	char *keyUtf8;
	char *valueUtf8;
	int result = 0;

	if ( !key || !value ) {
		return 0;
	}
	keyUtf8 = mono_string_to_utf8( key );
	valueUtf8 = mono_string_to_utf8( value );
	if ( keyUtf8 && valueUtf8 ) {
		result = EngineDB_ProfileSet( keyUtf8, valueUtf8 ) ? 1 : 0;
	}
	if ( keyUtf8 ) {
		mono_free( keyUtf8 );
	}
	if ( valueUtf8 ) {
		mono_free( valueUtf8 );
	}
	return result;
}

static MonoString *id3_cs_profile_get( MonoString *key ) {
	char *keyUtf8;
	char value[1024];
	MonoString *result = NULL;

	if ( !key ) {
		return NULL;
	}
	keyUtf8 = mono_string_to_utf8( key );
	if ( keyUtf8 && EngineDB_ProfileGet( keyUtf8, value, sizeof( value ) ) ) {
		result = mono_string_new( mono_domain_get(), value );
	}
	if ( keyUtf8 ) {
		mono_free( keyUtf8 );
	}
	return result;
}

static int id3_cs_profile_delete( MonoString *key ) {
	char *keyUtf8;
	int result = 0;

	if ( !key ) {
		return 0;
	}
	keyUtf8 = mono_string_to_utf8( key );
	if ( keyUtf8 ) {
		result = EngineDB_ProfileDelete( keyUtf8 ) ? 1 : 0;
		mono_free( keyUtf8 );
	}
	return result;
}

static void CsDebug_RegisterInternalCalls( void ) {
	mono_add_internal_call( "IdTech3.Engine::Print", id3_cs_print );
	mono_add_internal_call( "IdTech3.Engine::CvarGet", id3_cs_cvar_get );
	mono_add_internal_call( "IdTech3.Engine::CvarSet", id3_cs_cvar_set );
	mono_add_internal_call( "IdTech3.Engine::GetMilliseconds", id3_cs_get_milliseconds );
	mono_add_internal_call( "IdTech3.Engine::GetEngineInfo", id3_cs_get_engine_info );
	mono_add_internal_call( "IdTech3.Engine::Exec", id3_cs_exec );
	mono_add_internal_call( "IdTech3.Engine::DbAvailable", id3_cs_db_available );
	mono_add_internal_call( "IdTech3.Engine::DbPath", id3_cs_db_path );
	mono_add_internal_call( "IdTech3.Engine::DbExec", id3_cs_db_exec );
	mono_add_internal_call( "IdTech3.Engine::DbQueryOne", id3_cs_db_query_one );
	mono_add_internal_call( "IdTech3.Engine::ProfileSet", id3_cs_profile_set );
	mono_add_internal_call( "IdTech3.Engine::ProfileGet", id3_cs_profile_get );
	mono_add_internal_call( "IdTech3.Engine::ProfileDelete", id3_cs_profile_delete );
}

static qboolean CsDebug_StageVfsFileToHome( const char *vfsPath, char *diskPath, int diskPathSize );

static qboolean CsDebug_EnsureMono( void ) {
	if ( s_csMonoReady ) {
		return qtrue;
	}

	if ( !mono_get_root_domain() ) {
		mono_jit_init( "idtech3" );
	}
	CsDebug_RegisterInternalCalls();
	s_csDomain = mono_domain_get();
	s_csMonoReady = qtrue;
	Com_Printf( "C# scripting: Mono runtime ready (%s); cs_reload scripts/csharp/*.cs\n",
		mono_get_runtime_build_info() );
	return qtrue;
}

static qboolean CsDebug_ResolveApiDiskPath( char *out, int outSize ) {
	const char *fromDefine;

	if ( !out || outSize <= 0 ) {
		return qfalse;
	}

	if ( FS_FileExists( "scripts/csharp/IdTech3.Engine.cs" ) ) {
		return CsDebug_StageVfsFileToHome( "scripts/csharp/IdTech3.Engine.cs", out, outSize );
	}

#ifdef IDTECH3_CSHARP_API_PATH
	fromDefine = IDTECH3_CSHARP_API_PATH;
	if ( fromDefine && fromDefine[0] ) {
		Q_strncpyz( out, fromDefine, outSize );
		return qtrue;
	}
#endif

	return qfalse;
}

static qboolean CsDebug_StageVfsFileToHome( const char *vfsPath, char *diskPath, int diskPathSize ) {
	void *buf;
	int len;

	len = FS_ReadFile( vfsPath, &buf );
	if ( len <= 0 || !buf ) {
		return qfalse;
	}

	FS_WriteFile( vfsPath, buf, len );
	FS_FreeFile( buf );

	Com_sprintf( diskPath, diskPathSize, "%s/%s", FS_GetHomePath(), vfsPath );
	return qtrue;
}

static qboolean CsDebug_CompileScript( const char *scriptPath, char *dllPath, int dllPathSize ) {
	char apiDisk[MAX_OSPATH];
	char scriptDisk[MAX_OSPATH];
	char cmd[2048];
	const char *compiler;
	int result;

	if ( !CsDebug_ResolveApiDiskPath( apiDisk, sizeof( apiDisk ) ) ) {
		Com_Printf( S_COLOR_RED "C#: IdTech3.Engine.cs not found (scripts/csharp/ or build tree)\n" );
		return qfalse;
	}

	if ( !CsDebug_StageVfsFileToHome( scriptPath, scriptDisk, sizeof( scriptDisk ) ) ) {
		Com_Printf( S_COLOR_RED "C#: could not stage script '%s' for compile\n", scriptPath );
		return qfalse;
	}

	compiler = ( cs_compiler && cs_compiler->string[0] ) ? cs_compiler->string : "mcs";
	Com_sprintf( cmd, sizeof( cmd ),
		"%s -nologo -warn:0 -target:library -out:\"%s\" -reference:System \"%s\" \"%s\"",
		compiler, dllPath, apiDisk, scriptDisk );

	Com_Printf( "C#: compiling %s\n", scriptPath );
	result = system( cmd );
	if ( result != 0 ) {
		Com_Printf( S_COLOR_RED "C#: compiler failed (exit %d) for %s\n", result, scriptPath );
		return qfalse;
	}

	return qtrue;
}

static qboolean CsDebug_LoadAssembly( const char *dllPath ) {
	MonoImageOpenStatus status;
	MonoObject *exc;

	CsDebug_ClearAssemblyRefs();

	s_csAssembly = mono_assembly_open( dllPath, &status );
	if ( !s_csAssembly || status != MONO_IMAGE_OK ) {
		Com_Printf( S_COLOR_RED "C#: failed to open assembly '%s' (status %d)\n", dllPath, (int)status );
		return qfalse;
	}

	s_csImage = mono_assembly_get_image( s_csAssembly );
	s_csGameScriptClass = mono_class_from_name( s_csImage, "Game", "Script" );
	if ( !s_csGameScriptClass ) {
		Com_Printf( S_COLOR_RED "C#: missing Game.Script class in %s\n", dllPath );
		return qfalse;
	}

	s_csEngineClass = mono_class_from_name( s_csImage, "IdTech3", "Engine" );
	if ( !s_csEngineClass ) {
		Com_Printf( S_COLOR_RED "C#: missing IdTech3.Engine class in %s\n", dllPath );
		return qfalse;
	}

	s_csInitMethod = mono_class_get_method_from_name( s_csGameScriptClass, "Init", 0 );
	s_csFrameMethod = mono_class_get_method_from_name( s_csGameScriptClass, "Frame", 2 );
	s_csDispatchEventMethod = mono_class_get_method_from_name( s_csEngineClass, "DispatchEvent", 5 );

	if ( s_csInitMethod ) {
		mono_runtime_invoke( s_csInitMethod, NULL, NULL, &exc );
		if ( exc ) {
			Com_Printf( S_COLOR_RED "C#: Game.Script.Init threw an exception\n" );
			return qfalse;
		}
	}

	return qtrue;
}

static qboolean CsDebug_LoadScript( const char *scriptPath ) {
	char dllPath[MAX_OSPATH];
	char cacheDir[MAX_OSPATH];
	const char *base;
	int len;

	if ( !CsDebug_EnsureMono() ) {
		return qfalse;
	}
	if ( !CsDebug_IsAllowedPath( scriptPath ) ) {
		Com_Printf( S_COLOR_RED "C#: denied path '%s' (use scripts/csharp/, gameplay/, client/, ui/)\n", scriptPath );
		return qfalse;
	}

	len = (int)strlen( scriptPath );
	if ( len < 4 || Q_stricmp( scriptPath + len - 3, ".cs" ) ) {
		Com_Printf( S_COLOR_RED "C#: only .cs sources supported (got '%s')\n", scriptPath );
		return qfalse;
	}

	Com_sprintf( cacheDir, sizeof( cacheDir ), "%s/vm/csharp_cache", FS_GetHomePath() );
	Sys_Mkdir( cacheDir );

	{
		char baseName[MAX_QPATH];

		Q_strncpyz( baseName, scriptPath, sizeof( baseName ) );
		base = COM_SkipPath( baseName );
		COM_StripExtension( base, dllPath, sizeof( dllPath ) );
		Com_sprintf( dllPath, sizeof( dllPath ), "%s/%s.dll", cacheDir, dllPath );
	}

	if ( !CsDebug_CompileScript( scriptPath, dllPath, sizeof( dllPath ) ) ) {
		return qfalse;
	}

	return CsDebug_LoadAssembly( dllPath );
}

static void CsDebug_TrackScript( const char *scriptPath ) {
	int i;

	for ( i = 0; i < s_csTrackedCount; i++ ) {
		if ( !Q_stricmp( s_csTrackedScripts[i], scriptPath ) ) {
			return;
		}
	}
	if ( s_csTrackedCount >= MAX_CS_TRACKED_SCRIPTS ) {
		Com_Printf( S_COLOR_YELLOW "C#: tracked script limit reached (%d)\n", MAX_CS_TRACKED_SCRIPTS );
		return;
	}
	Q_strncpyz( s_csTrackedScripts[s_csTrackedCount], scriptPath, sizeof( s_csTrackedScripts[0] ) );
	s_csTrackedCount++;
}

void CsDebug_InitCvars( void ) {
	CsDebug_InitPolicyCvars();
	if ( cs_autoInit && cs_autoInit->integer ) {
		CsDebug_EnsureMono();
	}
}

void CsDebug_Frame( int msec, int realMsec ) {
	void *args[2];
	void *evArgs[5];
	int i0;
	int i1;
	MonoObject *exc;

	if ( !s_csMonoReady || !s_csAssembly ) {
		return;
	}

	if ( s_csFrameMethod ) {
		args[0] = &msec;
		args[1] = &realMsec;
		mono_runtime_invoke( s_csFrameMethod, NULL, args, &exc );
		if ( exc ) {
			Com_Printf( S_COLOR_RED "C#: Game.Script.Frame exception\n" );
		}
	}

	if ( cs_allowEvents && cs_allowEvents->integer && s_csDispatchEventMethod ) {
		i0 = msec;
		i1 = realMsec;
		evArgs[0] = mono_string_new( s_csDomain, "frame" );
		evArgs[1] = mono_string_new( s_csDomain, "" );
		evArgs[2] = mono_string_new( s_csDomain, "" );
		evArgs[3] = &i0;
		evArgs[4] = &i1;
		mono_runtime_invoke( s_csDispatchEventMethod, NULL, evArgs, &exc );
		if ( exc ) {
			Com_Printf( S_COLOR_RED "C#: frame event exception\n" );
		}
	}
}

void CsDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 ) {
	void *args[5];
	MonoObject *exc;

	if ( !eventName || !eventName[0] ) {
		return;
	}
	if ( !cs_allowEvents || !cs_allowEvents->integer ) {
		return;
	}
	if ( !s_csMonoReady || !s_csDispatchEventMethod ) {
		return;
	}

	args[0] = mono_string_new( s_csDomain, eventName );
	args[1] = mono_string_new( s_csDomain, s0 ? s0 : "" );
	args[2] = mono_string_new( s_csDomain, s1 ? s1 : "" );
	args[3] = &i0;
	args[4] = &i1;
	mono_runtime_invoke( s_csDispatchEventMethod, NULL, args, &exc );
	if ( exc ) {
		Com_Printf( S_COLOR_RED "C#: DispatchEvent(%s) exception\n", eventName );
	}
}

void Cmd_CsReload_f( void ) {
	int argc = Cmd_Argc();
	int i;
	int ok = 0;
	int fail = 0;

	CsDebug_InitPolicyCvars();

	if ( argc <= 1 ) {
		if ( s_csTrackedCount <= 0 ) {
			if ( CsDebug_EnsureMono() ) {
				Com_Printf( "C#: runtime initialized (no tracked scripts)\n" );
			}
			return;
		}
		CsDebug_CloseState();
		if ( !CsDebug_EnsureMono() ) {
			return;
		}
		for ( i = 0; i < s_csTrackedCount; i++ ) {
			if ( CsDebug_LoadScript( s_csTrackedScripts[i] ) ) {
				ok++;
			} else {
				fail++;
			}
		}
		Com_Printf( "C#: reloaded %d script(s), %d failure(s)\n", ok, fail );
		return;
	}

	CsDebug_CloseState();
	s_csTrackedCount = 0;
	if ( !CsDebug_EnsureMono() ) {
		return;
	}

	for ( i = 1; i < argc; i++ ) {
		const char *path = Cmd_Argv( i );
		if ( !path || !path[0] ) {
			continue;
		}
		if ( CsDebug_LoadScript( path ) ) {
			CsDebug_TrackScript( path );
			ok++;
		} else {
			fail++;
		}
	}
	Com_Printf( "C#: loaded %d script(s), %d failure(s)\n", ok, fail );
}

void Cmd_CsList_f( void ) {
	int i;

	CsDebug_InitPolicyCvars();
	Com_Printf( "C#: compat %s\n", cs_compatTarget ? cs_compatTarget->string : "mono-4.7-api" );
	Com_Printf( "C#: policy cs_allowEvents=%d cs_allowExec=%d cs_frameCallbackBudgetMs=%d cs_autoInit=%d compiler=%s\n",
		cs_allowEvents ? cs_allowEvents->integer : 0,
		cs_allowExec ? cs_allowExec->integer : 0,
		cs_frameCallbackBudgetMs ? cs_frameCallbackBudgetMs->integer : 0,
		cs_autoInit ? cs_autoInit->integer : 0,
		cs_compiler ? cs_compiler->string : "mcs" );
	if ( !s_csMonoReady ) {
		Com_Printf( "C#: runtime not initialized (cs_reload)\n" );
		return;
	}
	Com_Printf( "C#: runtime active, assembly %s\n", s_csAssembly ? "loaded" : "none" );
	Com_Printf( "C#: paths scripts/csharp/, gameplay/, client/, ui/\n" );
	Com_Printf( "C#: entry Game.Script.Init / Frame; events via IdTech3.Engine.On\n" );
	for ( i = 0; i < s_csTrackedCount; i++ ) {
		Com_Printf( "  %s\n", s_csTrackedScripts[i] );
	}
}

void Cmd_CsDump_f( void ) {
	Cmd_CsList_f();
}

#else /* !USE_CSHARP */

void CsDebug_InitCvars( void ) {
}

void Cmd_CsReload_f( void ) {
	Com_Printf( "C# scripting is disabled. Configure with -DUSE_CSHARP=ON and install Mono (libmono-2.0-dev, mono-devel).\n" );
}

void Cmd_CsList_f( void ) {
	Cmd_CsReload_f();
}

void Cmd_CsDump_f( void ) {
	Cmd_CsReload_f();
}

void CsDebug_Frame( int msec, int realMsec ) {
	(void)msec;
	(void)realMsec;
}

void CsDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 ) {
	(void)eventName;
	(void)s0;
	(void)s1;
	(void)i0;
	(void)i1;
}

#endif /* USE_CSHARP */
