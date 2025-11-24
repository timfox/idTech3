/*
===========================================================================
Copyright (C) 2024 id Tech 3

This file provides SQLite integration for database support.
It wraps SQLite functions with engine-style APIs.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_SQLITE
#include <sqlite3.h>

// CVar to control SQLite usage
static cvar_t *com_sqlite_enabled;

/*
=================
SQLite_Open
=================
Open a SQLite database connection
Returns NULL on failure
=================
*/
sqlite3 *SQLite_Open(const char *filename)
{
	sqlite3 *db;
	int rc;
	
	if (!filename || !*filename)
		return NULL;
	
	if (!com_sqlite_enabled || !com_sqlite_enabled->integer)
		return NULL;
	
	rc = sqlite3_open(filename, &db);
	if (rc != SQLITE_OK) {
		Com_Printf("SQLite_Open: Can't open database %s: %s\n", filename, sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}
	
	return db;
}

/*
=================
SQLite_Close
=================
Close a SQLite database connection
=================
*/
void SQLite_Close(sqlite3 *db)
{
	if (db) {
		sqlite3_close(db);
	}
}

/*
=================
SQLite_Exec
=================
Execute a SQL statement
Returns SQLITE_OK on success
=================
*/
int SQLite_Exec(sqlite3 *db, const char *sql, int (*callback)(void*, int, char**, char**), void *arg)
{
	char *errMsg = NULL;
	int rc;
	
	if (!db || !sql)
		return SQLITE_ERROR;
	
	rc = sqlite3_exec(db, sql, callback, arg, &errMsg);
	if (rc != SQLITE_OK) {
		Com_Printf("SQLite_Exec error: %s\n", errMsg ? errMsg : "Unknown error");
		if (errMsg) {
			sqlite3_free(errMsg);
		}
	}
	
	return rc;
}

/*
=================
SQLite_Prepare
=================
Prepare a SQL statement
Returns SQLITE_OK on success
=================
*/
int SQLite_Prepare(sqlite3 *db, const char *sql, sqlite3_stmt **stmt, const char **tail)
{
	if (!db || !sql || !stmt)
		return SQLITE_ERROR;
	
	return sqlite3_prepare_v2(db, sql, -1, stmt, tail);
}

/*
=================
SQLite_Step
=================
Execute a prepared statement
Returns SQLITE_ROW if row available, SQLITE_DONE if complete, error code otherwise
=================
*/
int SQLite_Step(sqlite3_stmt *stmt)
{
	if (!stmt)
		return SQLITE_ERROR;
	
	return sqlite3_step(stmt);
}

/*
=================
SQLite_Finalize
=================
Finalize a prepared statement
=================
*/
int SQLite_Finalize(sqlite3_stmt *stmt)
{
	if (!stmt)
		return SQLITE_ERROR;
	
	return sqlite3_finalize(stmt);
}

/*
=================
SQLite_GetColumnText
=================
Get text value from current row
Returns NULL if column doesn't exist
=================
*/
const char *SQLite_GetColumnText(sqlite3_stmt *stmt, int col)
{
	if (!stmt)
		return NULL;
	
	return (const char *)sqlite3_column_text(stmt, col);
}

/*
=================
SQLite_GetColumnInt
=================
Get integer value from current row
Returns 0 if column doesn't exist
=================
*/
int SQLite_GetColumnInt(sqlite3_stmt *stmt, int col)
{
	if (!stmt)
		return 0;
	
	return sqlite3_column_int(stmt, col);
}

/*
=================
SQLite_GetColumnDouble
=================
Get double value from current row
Returns 0.0 if column doesn't exist
=================
*/
double SQLite_GetColumnDouble(sqlite3_stmt *stmt, int col)
{
	if (!stmt)
		return 0.0;
	
	return sqlite3_column_double(stmt, col);
}

/*
=================
SQLite_BindText
=================
Bind a text parameter to a prepared statement
=================
*/
int SQLite_BindText(sqlite3_stmt *stmt, int index, const char *value, int len)
{
	if (!stmt || !value)
		return SQLITE_ERROR;
	
	if (len < 0)
		len = (int)strlen(value);
	
	return sqlite3_bind_text(stmt, index, value, len, SQLITE_TRANSIENT);
}

/*
=================
SQLite_BindInt
=================
Bind an integer parameter to a prepared statement
=================
*/
int SQLite_BindInt(sqlite3_stmt *stmt, int index, int value)
{
	if (!stmt)
		return SQLITE_ERROR;
	
	return sqlite3_bind_int(stmt, index, value);
}

/*
=================
SQLite_BindDouble
=================
Bind a double parameter to a prepared statement
=================
*/
int SQLite_BindDouble(sqlite3_stmt *stmt, int index, double value)
{
	if (!stmt)
		return SQLITE_ERROR;
	
	return sqlite3_bind_double(stmt, index, value);
}

/*
=================
SQLite_LastInsertRowid
=================
Get the rowid of the last inserted row
=================
*/
sqlite3_int64 SQLite_LastInsertRowid(sqlite3 *db)
{
	if (!db)
		return 0;
	
	return sqlite3_last_insert_rowid(db);
}

/*
=================
SQLite_Changes
=================
Get the number of rows changed by the last statement
=================
*/
int SQLite_Changes(sqlite3 *db)
{
	if (!db)
		return 0;
	
	return sqlite3_changes(db);
}

/*
=================
SQLite_Init
=================
Initialize SQLite subsystem
=================
*/
void SQLite_Init(void)
{
	com_sqlite_enabled = Cvar_Get("com_sqlite_enabled", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(com_sqlite_enabled, "Enable SQLite database support (1 = enabled, 0 = disabled)");
}

#endif // USE_SQLITE

