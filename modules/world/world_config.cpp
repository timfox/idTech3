/* C++20 migration: extern "C" API boundary preserved. */
extern "C" {
/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "world_config.h"

#define WORLD_CONFIG_REPORT_CHUNK 256

static worldConfigEntry_t configs[WORLD_CONFIG_MAX];
static int configCount;
static char activeName[WORLD_CONFIG_NAME_MAX];
static char activeSpawnLayout[WORLD_CONFIG_NAME_MAX];
static int generation;
static char manifestPath[MAX_QPATH];
static worldConfigOnApply_f onApplyFn;

static worldConfigSpawn_t spawns[WORLD_CONFIG_SPAWN_MAX];
static int spawnCount;

static cvar_t *r_worldConfigEnable;
static cvar_t *r_worldConfig;
static cvar_t *r_worldConfigEpoch;
static cvar_t *sv_worldConfigEnable;
static cvar_t *sv_worldConfig;

static worldConfigEntry_t *WorldConfig_FindMutable( const char *name ) {
	int i;

	if ( !name || !name[0] ) {
		return NULL;
	}
	for ( i = 0; i < configCount; i++ ) {
		if ( !Q_stricmp( configs[i].name, name ) ) {
			return &configs[i];
		}
	}
	return NULL;
}

static worldConfigEntry_t *WorldConfig_Ensure( const char *name ) {
	worldConfigEntry_t *e;

	e = WorldConfig_FindMutable( name );
	if ( e ) {
		return e;
	}
	if ( configCount >= WORLD_CONFIG_MAX ) {
		Com_Printf( S_COLOR_YELLOW "[world_config] config table full, ignoring '%s'\n", name );
		return NULL;
	}
	e = &configs[configCount++];
	Com_Memset( e, 0, sizeof( *e ) );
	Q_strncpyz( e->name, name, sizeof( e->name ) );
	Q_strncpyz( e->spawnLayout, name, sizeof( e->spawnLayout ) );
	e->defined = qtrue;
	return e;
}

static void WorldConfig_SkipLine( char **text ) {
	char *p = *text;

	while ( *p && *p != '\n' ) {
		p++;
	}
	if ( *p == '\n' ) {
		p++;
	}
	*text = p;
}

static qboolean WorldConfig_ParseLine( char *line ) {
	const char *p;
	char token[MAX_TOKEN_CHARS];
	char name[WORLD_CONFIG_NAME_MAX];
	char key[64];
	char *dot;
	worldConfigEntry_t *e;

	while ( *line == ' ' || *line == '\t' ) {
		line++;
	}
	if ( !*line || *line == '#' || *line == '/' ) {
		return qtrue;
	}

	p = line;
	Q_strncpyz( token, COM_Parse( &p ), sizeof( token ) );
	if ( !token[0] ) {
		return qtrue;
	}

	if ( !Q_stricmp( token, "config" ) ) {
		Q_strncpyz( name, COM_Parse( &p ), sizeof( name ) );
		if ( !name[0] ) {
			return qfalse;
		}
		return WorldConfig_Ensure( name ) != NULL ? qtrue : qfalse;
	}

	/* name.key value... */
	Q_strncpyz( name, token, sizeof( name ) );
	dot = strchr( name, '.' );
	if ( !dot ) {
		Com_Printf( S_COLOR_YELLOW "[world_config] bad line (expected name.key): %s\n", line );
		return qfalse;
	}
	*dot = '\0';
	Q_strncpyz( key, dot + 1, sizeof( key ) );
	e = WorldConfig_Ensure( name );
	if ( !e ) {
		return qfalse;
	}

	if ( !Q_stricmp( key, "geometrySuffix" ) ) {
		Q_strncpyz( e->geometrySuffix, COM_Parse( &p ), sizeof( e->geometrySuffix ) );
	} else if ( !Q_stricmp( key, "navSuffix" ) ) {
		Q_strncpyz( e->navSuffix, COM_Parse( &p ), sizeof( e->navSuffix ) );
	} else if ( !Q_stricmp( key, "spawnLayout" ) ) {
		Q_strncpyz( e->spawnLayout, COM_Parse( &p ), sizeof( e->spawnLayout ) );
	} else if ( !Q_stricmp( key, "ndgiTime" ) ) {
		e->ndgiTime = (float)atof( COM_Parse( &p ) );
		e->hasNdgiTime = qtrue;
	} else if ( !Q_stricmp( key, "nivScale" ) ) {
		e->nivScale = (float)atof( COM_Parse( &p ) );
		e->hasNivScale = qtrue;
	} else if ( !Q_stricmp( key, "bounds" ) ) {
		const char *t = COM_Parse( &p );
		if ( Q_stricmp( t, "mins" ) ) {
			Com_Printf( S_COLOR_YELLOW "[world_config] bounds expects 'mins'\n" );
			return qfalse;
		}
		e->boundsMins[0] = (float)atof( COM_Parse( &p ) );
		e->boundsMins[1] = (float)atof( COM_Parse( &p ) );
		e->boundsMins[2] = (float)atof( COM_Parse( &p ) );
		t = COM_Parse( &p );
		if ( Q_stricmp( t, "maxs" ) ) {
			Com_Printf( S_COLOR_YELLOW "[world_config] bounds expects 'maxs'\n" );
			return qfalse;
		}
		e->boundsMaxs[0] = (float)atof( COM_Parse( &p ) );
		e->boundsMaxs[1] = (float)atof( COM_Parse( &p ) );
		e->boundsMaxs[2] = (float)atof( COM_Parse( &p ) );
		e->hasBounds = qtrue;
	} else if ( !Q_stricmp( key, "sightline" ) ) {
		worldConfigSightline_t *sl;
		const char *expectTok;
		const char *mid;

		if ( e->sightlineCount >= WORLD_CONFIG_SIGHTLINE_MAX ) {
			Com_Printf( S_COLOR_YELLOW "[world_config] sightline table full for '%s'\n", e->name );
			return qfalse;
		}
		sl = &e->sightlines[e->sightlineCount++];
		Com_Memset( sl, 0, sizeof( *sl ) );
		Q_strncpyz( sl->label, COM_Parse( &p ), sizeof( sl->label ) );
		sl->start[0] = (float)atof( COM_Parse( &p ) );
		sl->start[1] = (float)atof( COM_Parse( &p ) );
		sl->start[2] = (float)atof( COM_Parse( &p ) );
		mid = COM_Parse( &p );
		/* mid may be a label (B) or the first end coordinate */
		if ( mid[0] && ( mid[0] < '0' || mid[0] > '9' ) && mid[0] != '-' && mid[0] != '.' ) {
			sl->end[0] = (float)atof( COM_Parse( &p ) );
		} else {
			sl->end[0] = (float)atof( mid );
		}
		sl->end[1] = (float)atof( COM_Parse( &p ) );
		sl->end[2] = (float)atof( COM_Parse( &p ) );
		expectTok = COM_Parse( &p );
		sl->expect = WC_SIGHT_CLEAR;
		if ( expectTok[0] && !Q_stricmp( expectTok, "blocked" ) ) {
			sl->expect = WC_SIGHT_BLOCKED;
		} else if ( expectTok[0] && !Q_stricmp( expectTok, "clear" ) ) {
			sl->expect = WC_SIGHT_CLEAR;
		}
	} else {
		Com_Printf( S_COLOR_YELLOW "[world_config] unknown key '%s.%s'\n", name, key );
		return qfalse;
	}
	return qtrue;
}

static void WorldConfig_BumpEpoch( void ) {
	if ( r_worldConfigEpoch ) {
		Cvar_SetValue( "r_worldConfigEpoch", (float)( r_worldConfigEpoch->integer + 1 ) );
	}
}

static void WorldConfig_PublishSuffixCvars( const worldConfigEntry_t *e ) {
	Cvar_Get( "r_worldConfigGeoSuffix", "", CVAR_ROM );
	Cvar_Get( "r_worldConfigNavSuffix", "", CVAR_ROM );
	if ( e ) {
		Cvar_Set( "r_worldConfigGeoSuffix", e->geometrySuffix );
		Cvar_Set( "r_worldConfigNavSuffix", e->navSuffix );
	} else {
		Cvar_Set( "r_worldConfigGeoSuffix", "" );
		Cvar_Set( "r_worldConfigNavSuffix", "" );
	}
}

static void WorldConfig_ApplyLighting( const worldConfigEntry_t *e ) {
	if ( !e ) {
		return;
	}
	if ( e->hasNdgiTime ) {
		Cvar_SetValue( "r_ndgi_time", e->ndgiTime );
		Com_Printf( "[world_config] ndgiTime -> %.3f\n", e->ndgiTime );
	}
	if ( e->hasNivScale ) {
		Cvar_SetValue( "r_niv_scale", e->nivScale );
		Com_Printf( "[world_config] nivScale -> %.3f\n", e->nivScale );
	}
}

static qboolean worldConfigInited;

void WorldConfig_Init( void ) {
	if ( worldConfigInited ) {
		return;
	}
	r_worldConfigEnable = Cvar_Get( "r_worldConfigEnable", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_worldConfigEnable,
		"Enable named world configurations (alternate geometry/nav/spawns/lighting)." );
	r_worldConfig = Cvar_Get( "r_worldConfig", "default", CVAR_ARCHIVE );
	Cvar_SetDescription( r_worldConfig,
		"Active world config name (client preference; server CS overrides when synced)." );
	r_worldConfigEpoch = Cvar_Get( "r_worldConfigEpoch", "0", CVAR_ROM );
	Cvar_SetDescription( r_worldConfigEpoch,
		"Bumped on world-config geometry transitions (temporal sticky reset)." );
	sv_worldConfigEnable = Cvar_Get( "sv_worldConfigEnable", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	Cvar_SetDescription( sv_worldConfigEnable,
		"Server: publish CS_ENGINE_WORLD_CONFIG and authorize config transitions." );
	sv_worldConfig = Cvar_Get( "sv_worldConfig", "default", CVAR_ARCHIVE | CVAR_SERVERINFO );
	Cvar_SetDescription( sv_worldConfig,
		"Server-authoritative world config name." );

	WorldConfig_Clear();
	WorldConfig_PublishSuffixCvars( NULL );
	worldConfigInited = qtrue;
	Com_Printf( "[world_config] named world configurations initialized (r_worldConfigEnable 0)\n" );
}

void WorldConfig_Shutdown( void ) {
	WorldConfig_Clear();
	onApplyFn = NULL;
	worldConfigInited = qfalse;
}

void WorldConfig_Clear( void ) {
	Com_Memset( configs, 0, sizeof( configs ) );
	configCount = 0;
	activeName[0] = '\0';
	Q_strncpyz( activeSpawnLayout, "default", sizeof( activeSpawnLayout ) );
	generation = 0;
	manifestPath[0] = '\0';
	WorldConfig_ClearSpawns();
}

qboolean WorldConfig_LoadManifestPath( const char *path ) {
	char *buf;
	char *text;
	char *lineStart;
	int len;
	int lineNo = 0;

	if ( !path || !path[0] ) {
		return qfalse;
	}
	len = FS_ReadFile( path, (void **)&buf );
	if ( len <= 0 || !buf ) {
		Com_DPrintf( "[world_config] no manifest %s\n", path );
		return qfalse;
	}

	WorldConfig_Clear();
	Q_strncpyz( manifestPath, path, sizeof( manifestPath ) );

	text = buf;
	while ( *text ) {
		char line[1024];
		int i = 0;

		lineNo++;
		lineStart = text;
		WorldConfig_SkipLine( &text );
		while ( lineStart < text && i < (int)sizeof( line ) - 1 ) {
			if ( *lineStart == '\r' || *lineStart == '\n' ) {
				break;
			}
			line[i++] = *lineStart++;
		}
		line[i] = '\0';
		if ( !WorldConfig_ParseLine( line ) ) {
			Com_Printf( S_COLOR_YELLOW "[world_config] parse error %s:%d\n", path, lineNo );
		}
	}
	FS_FreeFile( buf );

	if ( configCount <= 0 ) {
		WorldConfig_Ensure( "default" );
	}
	if ( !activeName[0] ) {
		Q_strncpyz( activeName, configs[0].name, sizeof( activeName ) );
		Q_strncpyz( activeSpawnLayout, configs[0].spawnLayout[0] ? configs[0].spawnLayout : configs[0].name,
			sizeof( activeSpawnLayout ) );
	}

	Com_Printf( "[world_config] loaded %d config(s) from %s\n", configCount, path );
	return qtrue;
}

qboolean WorldConfig_LoadManifest( const char *mapBaseName ) {
	char path[MAX_QPATH];
	const char *base;

	if ( !mapBaseName || !mapBaseName[0] ) {
		return qfalse;
	}
	base = mapBaseName;
	if ( !Q_stricmpn( base, "maps/", 5 ) ) {
		base += 5;
	}
	Com_sprintf( path, sizeof( path ), "world/%s.wcfg", base );
	{
		char *dot = strrchr( path, '.' );
		/* strip .bsp if present in mapBaseName before .wcfg */
		(void)dot;
	}
	/* If mapBaseName includes .bsp, strip it */
	{
		char clean[MAX_QPATH];
		char *ext;

		Q_strncpyz( clean, base, sizeof( clean ) );
		ext = strrchr( clean, '.' );
		if ( ext && !Q_stricmp( ext, ".bsp" ) ) {
			*ext = '\0';
		}
		Com_sprintf( path, sizeof( path ), "world/%s.wcfg", clean );
	}
	return WorldConfig_LoadManifestPath( path );
}

qboolean WorldConfig_IsEnabled( void ) {
	if ( sv_worldConfigEnable && sv_worldConfigEnable->integer ) {
		return qtrue;
	}
	if ( r_worldConfigEnable && r_worldConfigEnable->integer ) {
		return qtrue;
	}
	return qfalse;
}

qboolean WorldConfig_SetActive( const char *name ) {
	const worldConfigEntry_t *e;
	char oldName[WORLD_CONFIG_NAME_MAX];
	qboolean geometryChange;

	if ( !name || !name[0] ) {
		return qfalse;
	}
	e = WorldConfig_GetEntry( name );
	if ( !e ) {
		/* Allow setting unknown names when no manifest — creates implicit entry. */
		if ( !WorldConfig_Ensure( name ) ) {
			return qfalse;
		}
		e = WorldConfig_GetEntry( name );
	}
	if ( !e ) {
		return qfalse;
	}

	Q_strncpyz( oldName, activeName, sizeof( oldName ) );
	geometryChange = ( Q_stricmp( oldName, e->name ) != 0 ) ? qtrue : qfalse;

	Q_strncpyz( activeName, e->name, sizeof( activeName ) );
	if ( e->spawnLayout[0] ) {
		Q_strncpyz( activeSpawnLayout, e->spawnLayout, sizeof( activeSpawnLayout ) );
	} else {
		Q_strncpyz( activeSpawnLayout, e->name, sizeof( activeSpawnLayout ) );
	}
	generation++;
	if ( r_worldConfig ) {
		Cvar_Set( "r_worldConfig", activeName );
	}
	if ( sv_worldConfig && sv_worldConfigEnable && sv_worldConfigEnable->integer ) {
		Cvar_Set( "sv_worldConfig", activeName );
	}

	WorldConfig_PublishSuffixCvars( e );
	WorldConfig_ApplyLighting( e );
	if ( geometryChange ) {
		WorldConfig_BumpEpoch();
	}

	Com_Printf( "[world_config] active -> %s (gen %d, layout %s)\n",
		activeName, generation, activeSpawnLayout );

	if ( onApplyFn ) {
		onApplyFn( oldName[0] ? oldName : "", activeName, generation );
	}
	return qtrue;
}

qboolean WorldConfig_SetSpawnLayout( const char *layoutName ) {
	if ( !layoutName || !layoutName[0] ) {
		return qfalse;
	}
	Q_strncpyz( activeSpawnLayout, layoutName, sizeof( activeSpawnLayout ) );
	Com_Printf( "[world_config] spawn layout -> %s (geometry unchanged)\n", activeSpawnLayout );
	return qtrue;
}

const char *WorldConfig_GetActive( void ) {
	return activeName[0] ? activeName : "default";
}

const char *WorldConfig_GetSpawnLayout( void ) {
	return activeSpawnLayout[0] ? activeSpawnLayout : "default";
}

int WorldConfig_GetGeneration( void ) {
	return generation;
}

const worldConfigEntry_t *WorldConfig_GetEntry( const char *name ) {
	return WorldConfig_FindMutable( name );
}

int WorldConfig_GetCount( void ) {
	return configCount;
}

const worldConfigEntry_t *WorldConfig_GetByIndex( int index ) {
	if ( index < 0 || index >= configCount ) {
		return NULL;
	}
	return &configs[index];
}

float WorldConfig_GetNdgiTime( qboolean *hasValue ) {
	const worldConfigEntry_t *e = WorldConfig_GetEntry( WorldConfig_GetActive() );

	if ( hasValue ) {
		*hasValue = ( e && e->hasNdgiTime ) ? qtrue : qfalse;
	}
	return ( e && e->hasNdgiTime ) ? e->ndgiTime : 0.0f;
}

float WorldConfig_GetNivScale( qboolean *hasValue ) {
	const worldConfigEntry_t *e = WorldConfig_GetEntry( WorldConfig_GetActive() );

	if ( hasValue ) {
		*hasValue = ( e && e->hasNivScale ) ? qtrue : qfalse;
	}
	return ( e && e->hasNivScale ) ? e->nivScale : 1.0f;
}

static void WorldConfig_SuffixFor( const char *kind, char *out, int outSize ) {
	const worldConfigEntry_t *e;

	out[0] = '\0';
	if ( !WorldConfig_IsEnabled() ) {
		return;
	}
	e = WorldConfig_GetEntry( WorldConfig_GetActive() );
	if ( !e ) {
		return;
	}
	if ( !Q_stricmp( kind, "geometry" ) && e->geometrySuffix[0] ) {
		Q_strncpyz( out, e->geometrySuffix, outSize );
	} else if ( !Q_stricmp( kind, "nav" ) && e->navSuffix[0] ) {
		Q_strncpyz( out, e->navSuffix, outSize );
	}
}

void WorldConfig_FormatSectorBsp( int cellX, int cellY, char *out, int outSize ) {
	char suffix[32];

	WorldConfig_SuffixFor( "geometry", suffix, sizeof( suffix ) );
	if ( suffix[0] ) {
		Com_sprintf( out, outSize, "maps/sector_%d_%d%s.bsp", cellX, cellY, suffix );
	} else {
		Com_sprintf( out, outSize, "maps/sector_%d_%d.bsp", cellX, cellY );
	}
}

void WorldConfig_FormatSectorNav( int cellX, int cellY, char *out, int outSize ) {
	char suffix[32];

	WorldConfig_SuffixFor( "nav", suffix, sizeof( suffix ) );
	if ( suffix[0] ) {
		Com_sprintf( out, outSize, "nav/sector_%d_%d%s.nav", cellX, cellY, suffix );
	} else {
		Com_sprintf( out, outSize, "nav/sector_%d_%d.nav", cellX, cellY );
	}
}

void WorldConfig_FormatScatter( int cellX, int cellY, char *out, int outSize ) {
	const char *layout;

	if ( WorldConfig_IsEnabled() ) {
		layout = WorldConfig_GetSpawnLayout();
		if ( layout && layout[0] && Q_stricmp( layout, "default" ) ) {
			Com_sprintf( out, outSize, "sprites/layout_%s.ents", layout );
			return;
		}
	}
	Com_sprintf( out, outSize, "sprites/sector_%d_%d.ents", cellX, cellY );
}

qboolean WorldConfig_ResolveReadable( const char *preferred, const char *fallback,
	char *out, int outSize ) {
	if ( preferred && preferred[0] && FS_ReadFile( preferred, NULL ) > 0 ) {
		Q_strncpyz( out, preferred, outSize );
		return qtrue;
	}
	if ( fallback && fallback[0] && FS_ReadFile( fallback, NULL ) > 0 ) {
		Q_strncpyz( out, fallback, outSize );
		return qtrue;
	}
	if ( preferred && preferred[0] ) {
		Q_strncpyz( out, preferred, outSize );
	} else if ( fallback && fallback[0] ) {
		Q_strncpyz( out, fallback, outSize );
	} else {
		out[0] = '\0';
	}
	return qfalse;
}

void WorldConfig_SetOnApply( worldConfigOnApply_f fn ) {
	onApplyFn = fn;
}

void WorldConfig_ClearSpawns( void ) {
	Com_Memset( spawns, 0, sizeof( spawns ) );
	spawnCount = 0;
}

int WorldConfig_AddSpawn( const vec3_t origin, const vec3_t angles,
	const char *layout, const char *spawnType, float minIntensity, float maxIntensity ) {
	worldConfigSpawn_t *s;

	if ( spawnCount >= WORLD_CONFIG_SPAWN_MAX ) {
		return -1;
	}
	s = &spawns[spawnCount++];
	Com_Memset( s, 0, sizeof( *s ) );
	VectorCopy( origin, s->origin );
	VectorCopy( angles, s->angles );
	Q_strncpyz( s->layout, layout && layout[0] ? layout : "default", sizeof( s->layout ) );
	Q_strncpyz( s->spawnType, spawnType && spawnType[0] ? spawnType : "", sizeof( s->spawnType ) );
	s->minIntensity = minIntensity;
	s->maxIntensity = maxIntensity;
	s->active = qtrue;
	return spawnCount - 1;
}

int WorldConfig_GetSpawnCount( void ) {
	return spawnCount;
}

const worldConfigSpawn_t *WorldConfig_GetSpawn( int index ) {
	if ( index < 0 || index >= spawnCount ) {
		return NULL;
	}
	return &spawns[index];
}

int WorldConfig_CollectSpawnsForLayout( const char *layout,
	const worldConfigSpawn_t **out, int maxOut ) {
	int i;
	int n = 0;
	const char *want = layout && layout[0] ? layout : WorldConfig_GetSpawnLayout();

	for ( i = 0; i < spawnCount && n < maxOut; i++ ) {
		if ( !spawns[i].active ) {
			continue;
		}
		if ( Q_stricmp( spawns[i].layout, want ) && Q_stricmp( spawns[i].layout, "default" ) ) {
			/* Include layout-specific and also "any" if we used empty — default matches all layouts only when named default */
			continue;
		}
		if ( out ) {
			out[n] = &spawns[i];
		}
		n++;
	}
	return n;
}

static void WorldConfig_AppendReport( char *report, int reportSize, int *used, const char *fmt, ... ) {
	va_list argptr;
	char chunk[WORLD_CONFIG_REPORT_CHUNK];
	int n;

	if ( !report || reportSize <= 0 || !used ) {
		return;
	}
	va_start( argptr, fmt );
	Q_vsnprintf( chunk, sizeof( chunk ), fmt, argptr );
	va_end( argptr );
	n = (int)strlen( chunk );
	if ( *used + n >= reportSize ) {
		n = reportSize - *used - 1;
	}
	if ( n > 0 ) {
		memcpy( report + *used, chunk, (size_t)n );
		*used += n;
		report[*used] = '\0';
	}
}

static int WorldConfig_ValidateOne( const worldConfigEntry_t *e, char *report, int reportSize, int *used ) {
	int fails = 0;
	int i;

	if ( !e ) {
		return 1;
	}

	WorldConfig_AppendReport( report, reportSize, used, "config %s:\n", e->name );

	if ( e->hasBounds ) {
		vec3_t samples[5];
		int s;
		vec3_t c;

		VectorCopy( e->boundsMins, samples[0] );
		VectorCopy( e->boundsMaxs, samples[1] );
		samples[2][0] = e->boundsMins[0]; samples[2][1] = e->boundsMaxs[1]; samples[2][2] = e->boundsMins[2];
		samples[3][0] = e->boundsMaxs[0]; samples[3][1] = e->boundsMins[1]; samples[3][2] = e->boundsMaxs[2];
		c[0] = 0.5f * ( e->boundsMins[0] + e->boundsMaxs[0] );
		c[1] = 0.5f * ( e->boundsMins[1] + e->boundsMaxs[1] );
		c[2] = 0.5f * ( e->boundsMins[2] + e->boundsMaxs[2] );
		VectorCopy( c, samples[4] );

		for ( s = 0; s < 5; s++ ) {
			trace_t tr;
			vec3_t start, end;

			VectorCopy( samples[s], start );
			start[2] = e->boundsMaxs[2] + 64.0f;
			VectorCopy( samples[s], end );
			end[2] = e->boundsMins[2] - 64.0f;
			CM_BoxTrace( &tr, start, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
			if ( tr.fraction >= 1.0f && !tr.startsolid ) {
				WorldConfig_AppendReport( report, reportSize, used,
					"  FAIL bounds sample %d: no floor hit\n", s );
				fails++;
			} else {
				WorldConfig_AppendReport( report, reportSize, used,
					"  OK bounds sample %d hit z=%.1f\n", s, tr.endpos[2] );
			}
		}
	} else {
		WorldConfig_AppendReport( report, reportSize, used, "  (no bounds)\n" );
	}

	for ( i = 0; i < e->sightlineCount; i++ ) {
		const worldConfigSightline_t *sl = &e->sightlines[i];
		trace_t tr;
		qboolean blocked;

		CM_BoxTrace( &tr, sl->start, sl->end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
		blocked = ( tr.fraction < 1.0f || tr.startsolid || tr.allsolid ) ? qtrue : qfalse;
		if ( sl->expect == WC_SIGHT_CLEAR && blocked ) {
			WorldConfig_AppendReport( report, reportSize, used,
				"  FAIL sightline %s: expected clear, blocked (frac=%.3f)\n",
				sl->label[0] ? sl->label : "?", tr.fraction );
			fails++;
		} else if ( sl->expect == WC_SIGHT_BLOCKED && !blocked ) {
			WorldConfig_AppendReport( report, reportSize, used,
				"  FAIL sightline %s: expected blocked, clear\n",
				sl->label[0] ? sl->label : "?" );
			fails++;
		} else {
			WorldConfig_AppendReport( report, reportSize, used,
				"  OK sightline %s (%s)\n",
				sl->label[0] ? sl->label : "?",
				sl->expect == WC_SIGHT_BLOCKED ? "blocked" : "clear" );
		}
	}

	/* Spawn walkability: downward trace at each matching layout spawn. */
	{
		const char *layout = e->spawnLayout[0] ? e->spawnLayout : e->name;
		int si;

		for ( si = 0; si < spawnCount; si++ ) {
			trace_t tr;
			vec3_t start, end;

			if ( !spawns[si].active ) {
				continue;
			}
			if ( Q_stricmp( spawns[si].layout, layout ) && Q_stricmp( spawns[si].layout, "default" ) ) {
				continue;
			}
			VectorCopy( spawns[si].origin, start );
			start[2] += 32.0f;
			VectorCopy( spawns[si].origin, end );
			end[2] -= 128.0f;
			CM_BoxTrace( &tr, start, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
			if ( tr.fraction >= 1.0f || tr.startsolid ) {
				WorldConfig_AppendReport( report, reportSize, used,
					"  FAIL spawn (%.0f %.0f %.0f): no walkable floor\n",
					spawns[si].origin[0], spawns[si].origin[1], spawns[si].origin[2] );
				fails++;
			} else {
				WorldConfig_AppendReport( report, reportSize, used,
					"  OK spawn (%.0f %.0f %.0f)\n",
					spawns[si].origin[0], spawns[si].origin[1], spawns[si].origin[2] );
			}
		}
	}

	return fails;
}

int WorldConfig_Validate( const char *nameOrNull, char *report, int reportSize ) {
	int used = 0;
	int fails = 0;
	int i;

	if ( report && reportSize > 0 ) {
		report[0] = '\0';
	}

	if ( nameOrNull && nameOrNull[0] && Q_stricmp( nameOrNull, "all" ) ) {
		const worldConfigEntry_t *e = WorldConfig_GetEntry( nameOrNull );
		if ( !e ) {
			WorldConfig_AppendReport( report, reportSize, &used, "unknown config '%s'\n", nameOrNull );
			return 1;
		}
		fails += WorldConfig_ValidateOne( e, report, reportSize, &used );
	} else {
		if ( configCount <= 0 ) {
			WorldConfig_AppendReport( report, reportSize, &used, "no configs loaded\n" );
			return 1;
		}
		for ( i = 0; i < configCount; i++ ) {
			fails += WorldConfig_ValidateOne( &configs[i], report, reportSize, &used );
		}
	}

	WorldConfig_AppendReport( report, reportSize, &used, "total failures: %d\n", fails );
	return fails;
}

void WorldConfig_List( void ) {
	int i;

	Com_Printf( "[world_config] %d config(s), active='%s' layout='%s' gen=%d manifest='%s'\n",
		configCount, WorldConfig_GetActive(), WorldConfig_GetSpawnLayout(), generation,
		manifestPath[0] ? manifestPath : "(none)" );
	for ( i = 0; i < configCount; i++ ) {
		const worldConfigEntry_t *e = &configs[i];
		Com_Printf( "  %s geo='%s' nav='%s' layout='%s' ndgi=%s%.3f sight=%d bounds=%d\n",
			e->name,
			e->geometrySuffix[0] ? e->geometrySuffix : "-",
			e->navSuffix[0] ? e->navSuffix : "-",
			e->spawnLayout[0] ? e->spawnLayout : e->name,
			e->hasNdgiTime ? "" : "(",
			e->hasNdgiTime ? e->ndgiTime : 0.0f,
			e->sightlineCount,
			e->hasBounds ? 1 : 0 );
		if ( !e->hasNdgiTime ) {
			/* close paren already implied in format — keep simple */
		}
	}
}

void WorldConfig_Status( void ) {
	WorldConfig_List();
	Com_Printf( "[world_config] enable r=%d sv=%d spawns=%d\n",
		r_worldConfigEnable ? r_worldConfigEnable->integer : 0,
		sv_worldConfigEnable ? sv_worldConfigEnable->integer : 0,
		spawnCount );
}

} /* extern "C" */
