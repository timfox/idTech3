/*
===========================================================================
id Tech 3 - SQLite Wrapper Header

SQLite database wrapper functions.
===========================================================================
*/

#ifdef USE_SQLITE

#ifndef INCLUDE_SQLITE_WRAPPER_H
#define INCLUDE_SQLITE_WRAPPER_H

#include <sqlite3.h>

// SQLite wrapper functions
sqlite3 *SQLite_Open(const char *filename);
void SQLite_Close(sqlite3 *db);
int SQLite_Exec(sqlite3 *db, const char *sql, int (*callback)(void*, int, char**, char**), void *arg);
int SQLite_Prepare(sqlite3 *db, const char *sql, sqlite3_stmt **stmt, const char **tail);
int SQLite_Step(sqlite3_stmt *stmt);
int SQLite_Finalize(sqlite3_stmt *stmt);
const char *SQLite_GetColumnText(sqlite3_stmt *stmt, int col);
int SQLite_GetColumnInt(sqlite3_stmt *stmt, int col);
double SQLite_GetColumnDouble(sqlite3_stmt *stmt, int col);
int SQLite_BindText(sqlite3_stmt *stmt, int index, const char *value, int len);
int SQLite_BindInt(sqlite3_stmt *stmt, int index, int value);
int SQLite_BindDouble(sqlite3_stmt *stmt, int index, double value);
sqlite3_int64 SQLite_LastInsertRowid(sqlite3 *db);
int SQLite_Changes(sqlite3 *db);
void SQLite_Init(void);

#endif // INCLUDE_SQLITE_WRAPPER_H

#endif // USE_SQLITE