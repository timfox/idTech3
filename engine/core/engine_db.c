#include "q_shared.h"
#include "qcommon.h"
#include "engine_db.h"

#ifdef USE_SQLITE
#include <sqlite3.h>
#endif

static qboolean s_engineDbInit;
static qboolean s_engineDbAvailable;
static char s_engineDbPath[MAX_OSPATH];

#ifdef USE_SQLITE
static sqlite3 *s_engineDb;

static void EngineDB_EnsureDirectories( void )
{
	char gameDirPath[MAX_OSPATH];
	char saveDirPath[MAX_OSPATH];

	Q_strncpyz( gameDirPath,
		FS_BuildOSPath( FS_GetHomePath(), FS_GetCurrentGameDir(), NULL ),
		sizeof( gameDirPath ) );
	Sys_Mkdir( gameDirPath );

	Q_strncpyz( saveDirPath,
		FS_BuildOSPath( FS_GetHomePath(), FS_GetCurrentGameDir(), "save" ),
		sizeof( saveDirPath ) );
	Sys_Mkdir( saveDirPath );
}

static qboolean EngineDB_Open( void )
{
	char *err = NULL;
	const char *schema =
		"PRAGMA journal_mode=WAL;"
		"PRAGMA synchronous=NORMAL;"
		"CREATE TABLE IF NOT EXISTS profile_kv ("
		"  key TEXT PRIMARY KEY,"
		"  value TEXT NOT NULL,"
		"  updated_ms INTEGER NOT NULL DEFAULT 0"
		");"
		"CREATE TABLE IF NOT EXISTS save_slots ("
		"  slot INTEGER PRIMARY KEY,"
		"  label TEXT NOT NULL,"
		"  checksum INTEGER NOT NULL,"
		"  protocol_version INTEGER NOT NULL,"
		"  mod_version TEXT NOT NULL,"
		"  updated_ms INTEGER NOT NULL DEFAULT 0"
		");";

	if ( s_engineDb ) {
		return qtrue;
	}

	EngineDB_EnsureDirectories();
	Q_strncpyz( s_engineDbPath,
		FS_BuildOSPath( FS_GetHomePath(), FS_GetCurrentGameDir(), "save/engine_profile.db" ),
		sizeof( s_engineDbPath ) );

	if ( sqlite3_open( s_engineDbPath, &s_engineDb ) != SQLITE_OK ) {
		Com_Printf( S_COLOR_YELLOW "EngineDB: sqlite open failed for %s: %s\n",
			s_engineDbPath, s_engineDb ? sqlite3_errmsg( s_engineDb ) : "unknown" );
		if ( s_engineDb ) {
			sqlite3_close( s_engineDb );
			s_engineDb = NULL;
		}
		s_engineDbAvailable = qfalse;
		return qfalse;
	}

	if ( sqlite3_exec( s_engineDb, schema, NULL, NULL, &err ) != SQLITE_OK ) {
		Com_Printf( S_COLOR_YELLOW "EngineDB: schema init failed: %s\n", err ? err : "unknown" );
		if ( err ) {
			sqlite3_free( err );
		}
		sqlite3_close( s_engineDb );
		s_engineDb = NULL;
		s_engineDbAvailable = qfalse;
		return qfalse;
	}

	s_engineDbAvailable = qtrue;
	return qtrue;
}

static qboolean EngineDB_Prepare( const char *sql, sqlite3_stmt **stmtOut )
{
	if ( !sql || !sql[0] || !stmtOut ) {
		return qfalse;
	}
	if ( !EngineDB_Open() ) {
		return qfalse;
	}
	if ( sqlite3_prepare_v2( s_engineDb, sql, -1, stmtOut, NULL ) != SQLITE_OK ) {
		Com_Printf( S_COLOR_YELLOW "EngineDB: prepare failed: %s\n", sqlite3_errmsg( s_engineDb ) );
		return qfalse;
	}
	return qtrue;
}
#endif

void EngineDB_Init( void )
{
	if ( s_engineDbInit ) {
		return;
	}

	s_engineDbInit = qtrue;
	s_engineDbAvailable = qfalse;
	s_engineDbPath[0] = '\0';

#ifdef USE_SQLITE
	s_engineDbAvailable = EngineDB_Open();
	if ( s_engineDbAvailable ) {
		Com_Printf( "EngineDB: SQLite ready (%s)\n", s_engineDbPath );
	} else {
		Com_Printf( S_COLOR_YELLOW "EngineDB: SQLite unavailable, continuing without DB service\n" );
	}
#else
	Com_Printf( "EngineDB: SQLite support disabled at build time\n" );
#endif
}

void EngineDB_Shutdown( void )
{
#ifdef USE_SQLITE
	if ( s_engineDb ) {
		sqlite3_close( s_engineDb );
		s_engineDb = NULL;
	}
#endif
	s_engineDbAvailable = qfalse;
	s_engineDbInit = qfalse;
}

qboolean EngineDB_IsAvailable( void )
{
	return s_engineDbAvailable;
}

const char *EngineDB_GetPath( void )
{
	return s_engineDbPath;
}

qboolean EngineDB_Exec( const char *sql )
{
#ifdef USE_SQLITE
	char *err = NULL;

	if ( !sql || !sql[0] || !EngineDB_Open() ) {
		return qfalse;
	}
	if ( sqlite3_exec( s_engineDb, sql, NULL, NULL, &err ) != SQLITE_OK ) {
		Com_Printf( S_COLOR_YELLOW "EngineDB: exec failed: %s\n", err ? err : "unknown" );
		if ( err ) {
			sqlite3_free( err );
		}
		return qfalse;
	}
	return qtrue;
#else
	(void)sql;
	return qfalse;
#endif
}

qboolean EngineDB_QueryOne( const char *sql, char *out, int outSize )
{
#ifdef USE_SQLITE
	sqlite3_stmt *stmt = NULL;
	const unsigned char *text;
	int step;

	if ( out && outSize > 0 ) {
		out[0] = '\0';
	}
	if ( !out || outSize < 1 || !EngineDB_Prepare( sql, &stmt ) ) {
		return qfalse;
	}

	step = sqlite3_step( stmt );
	if ( step != SQLITE_ROW ) {
		sqlite3_finalize( stmt );
		return qfalse;
	}

	text = sqlite3_column_text( stmt, 0 );
	Q_strncpyz( out, text ? (const char *)text : "", outSize );
	sqlite3_finalize( stmt );
	return qtrue;
#else
	(void)sql;
	if ( out && outSize > 0 ) {
		out[0] = '\0';
	}
	return qfalse;
#endif
}

qboolean EngineDB_ProfileSet( const char *key, const char *value )
{
#ifdef USE_SQLITE
	sqlite3_stmt *stmt = NULL;
	int step;

	if ( !key || !key[0] || !value || !EngineDB_Prepare(
		"INSERT INTO profile_kv(key, value, updated_ms) VALUES(?1, ?2, ?3) "
		"ON CONFLICT(key) DO UPDATE SET value=excluded.value, updated_ms=excluded.updated_ms;",
		&stmt ) ) {
		return qfalse;
	}

	sqlite3_bind_text( stmt, 1, key, -1, SQLITE_TRANSIENT );
	sqlite3_bind_text( stmt, 2, value, -1, SQLITE_TRANSIENT );
	sqlite3_bind_int( stmt, 3, Sys_Milliseconds() );
	step = sqlite3_step( stmt );
	sqlite3_finalize( stmt );
	return ( step == SQLITE_DONE ) ? qtrue : qfalse;
#else
	(void)key;
	(void)value;
	return qfalse;
#endif
}

qboolean EngineDB_ProfileGet( const char *key, char *out, int outSize )
{
#ifdef USE_SQLITE
	sqlite3_stmt *stmt = NULL;
	const unsigned char *text;
	int step;

	if ( out && outSize > 0 ) {
		out[0] = '\0';
	}
	if ( !key || !key[0] || !out || outSize < 1 || !EngineDB_Prepare(
		"SELECT value FROM profile_kv WHERE key = ?1;",
		&stmt ) ) {
		return qfalse;
	}

	sqlite3_bind_text( stmt, 1, key, -1, SQLITE_TRANSIENT );
	step = sqlite3_step( stmt );
	if ( step != SQLITE_ROW ) {
		sqlite3_finalize( stmt );
		return qfalse;
	}

	text = sqlite3_column_text( stmt, 0 );
	Q_strncpyz( out, text ? (const char *)text : "", outSize );
	sqlite3_finalize( stmt );
	return qtrue;
#else
	(void)key;
	if ( out && outSize > 0 ) {
		out[0] = '\0';
	}
	return qfalse;
#endif
}

qboolean EngineDB_ProfileDelete( const char *key )
{
#ifdef USE_SQLITE
	sqlite3_stmt *stmt = NULL;
	int step;

	if ( !key || !key[0] || !EngineDB_Prepare( "DELETE FROM profile_kv WHERE key = ?1;", &stmt ) ) {
		return qfalse;
	}
	sqlite3_bind_text( stmt, 1, key, -1, SQLITE_TRANSIENT );
	step = sqlite3_step( stmt );
	sqlite3_finalize( stmt );
	return ( step == SQLITE_DONE ) ? qtrue : qfalse;
#else
	(void)key;
	return qfalse;
#endif
}

qboolean EngineDB_SaveWriteSlot( int slot, const char *label, int protocolVersion, const char *modVersion, unsigned checksum )
{
#ifdef USE_SQLITE
	sqlite3_stmt *stmt = NULL;
	int step;

	if ( slot < 0 || !label || !label[0] || !modVersion || !EngineDB_Prepare(
		"INSERT INTO save_slots(slot, label, checksum, protocol_version, mod_version, updated_ms) "
		"VALUES(?1, ?2, ?3, ?4, ?5, ?6) "
		"ON CONFLICT(slot) DO UPDATE SET label=excluded.label, checksum=excluded.checksum, "
		"protocol_version=excluded.protocol_version, mod_version=excluded.mod_version, updated_ms=excluded.updated_ms;",
		&stmt ) ) {
		return qfalse;
	}

	sqlite3_bind_int( stmt, 1, slot );
	sqlite3_bind_text( stmt, 2, label, -1, SQLITE_TRANSIENT );
	sqlite3_bind_int64( stmt, 3, (sqlite3_int64)checksum );
	sqlite3_bind_int( stmt, 4, protocolVersion );
	sqlite3_bind_text( stmt, 5, modVersion, -1, SQLITE_TRANSIENT );
	sqlite3_bind_int( stmt, 6, Sys_Milliseconds() );
	step = sqlite3_step( stmt );
	sqlite3_finalize( stmt );
	return ( step == SQLITE_DONE ) ? qtrue : qfalse;
#else
	(void)slot;
	(void)label;
	(void)protocolVersion;
	(void)modVersion;
	(void)checksum;
	return qfalse;
#endif
}

qboolean EngineDB_SaveReadSlot( int slot, char *labelOut, int labelLen, int *protoOut )
{
#ifdef USE_SQLITE
	sqlite3_stmt *stmt = NULL;
	const unsigned char *text;
	int step;

	if ( labelOut && labelLen > 0 ) {
		labelOut[0] = '\0';
	}
	if ( protoOut ) {
		*protoOut = 0;
	}
	if ( slot < 0 || !labelOut || labelLen < 1 || !EngineDB_Prepare(
		"SELECT label, protocol_version FROM save_slots WHERE slot = ?1;",
		&stmt ) ) {
		return qfalse;
	}

	sqlite3_bind_int( stmt, 1, slot );
	step = sqlite3_step( stmt );
	if ( step != SQLITE_ROW ) {
		sqlite3_finalize( stmt );
		return qfalse;
	}

	text = sqlite3_column_text( stmt, 0 );
	Q_strncpyz( labelOut, text ? (const char *)text : "", labelLen );
	if ( protoOut ) {
		*protoOut = sqlite3_column_int( stmt, 1 );
	}
	sqlite3_finalize( stmt );
	return qtrue;
#else
	(void)slot;
	if ( labelOut && labelLen > 0 ) {
		labelOut[0] = '\0';
	}
	if ( protoOut ) {
		*protoOut = 0;
	}
	return qfalse;
#endif
}
