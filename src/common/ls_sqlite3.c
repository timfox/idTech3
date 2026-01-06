/*
===========================================================================
id Tech 3 - LuaSQL SQLite3 Driver

SQLite3 database driver for LuaSQL integration.
Based on LuaSQL implementation.
===========================================================================
*/

#ifdef USE_LUASQL

#include "luasql.h"
#include "sqlite_wrapper.h"
#include <sqlite3.h>
#include "q_shared.h"

#ifdef USE_LUA
#ifdef USE_SQLITE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define LUASQL_ENVIRONMENT_SQLITE "SQLite3 environment"
#define LUASQL_CONNECTION_SQLITE "SQLite3 connection"
#define LUASQL_CURSOR_SQLITE "SQLite3 cursor"

/*
===============
Environment structure
===============
*/
typedef struct {
	short closed;
} env_data;

/*
===============
Connection structure
===============
*/
typedef struct {
	short closed;
	int env;                       /* reference to environment */
	short auto_commit;             /* 0 for manual commit */
	unsigned int cur_counter;
	sqlite3 *sql_conn;
} conn_data;

/*
===============
Cursor structure
===============
*/
typedef struct {
	short closed;
	int conn;                     /* reference to connection */
	int numcols;                  /* number of columns */
	int colnames, coltypes;       /* reference to column information tables */
	conn_data *conn_data;         /* reference to connection for cursor */
	sqlite3_stmt *sql_vm;
} cur_data;

/*
===============
getenvironment

Check for valid environment
===============
*/
static env_data *getenvironment(lua_State *L)
{
	env_data *env = (env_data *)luaL_checkudata(L, 1, LUASQL_ENVIRONMENT_SQLITE);
	luaL_argcheck(L, env != NULL, 1, LUASQL_PREFIX "environment expected");
	luaL_argcheck(L, !env->closed, 1, LUASQL_PREFIX "environment is closed");
	return env;
}

/*
===============
getconnection

Check for valid connection
===============
*/
static conn_data *getconnection(lua_State *L)
{
	conn_data *conn = (conn_data *)luaL_checkudata(L, 1, LUASQL_CONNECTION_SQLITE);
	luaL_argcheck(L, conn != NULL, 1, LUASQL_PREFIX "connection expected");
	luaL_argcheck(L, !conn->closed, 1, LUASQL_PREFIX "connection is closed");
	return conn;
}

/*
===============
getcursor

Check for valid cursor
===============
*/
static cur_data *getcursor(lua_State *L)
{
	cur_data *cur = (cur_data *)luaL_checkudata(L, 1, LUASQL_CURSOR_SQLITE);
	luaL_argcheck(L, cur != NULL, 1, LUASQL_PREFIX "cursor expected");
	luaL_argcheck(L, !cur->closed, 1, LUASQL_PREFIX "cursor is closed");
	return cur;
}

/*
===============
cur_nullify

Close cursor and nullify fields
===============
*/
static void cur_nullify(lua_State *L, cur_data *cur)
{
	conn_data *conn;

	/* Nullify structure fields. */
	cur->closed = 1;
	cur->sql_vm = NULL;

	/* Decrement cursor counter on connection object */
	lua_rawgeti(L, LUA_REGISTRYINDEX, cur->conn);
	conn = lua_touserdata(L, -1);
	conn->cur_counter--;

	luaL_unref(L, LUA_REGISTRYINDEX, cur->colnames);
	luaL_unref(L, LUA_REGISTRYINDEX, cur->coltypes);
	lua_pop(L, 1);
}

/*
===============
cur_create

Create a new cursor
===============
*/
static int cur_create(lua_State *L, conn_data *conn, sqlite3_stmt *sql_vm)
{
	cur_data *cur = (cur_data *)lua_newuserdata(L, sizeof(cur_data));
	luasql_setmeta(L, LUASQL_CURSOR_SQLITE);

	/* fill in structure */
	cur->closed = 0;
	cur->conn = luaL_ref(L, LUA_REGISTRYINDEX);
	cur->numcols = sqlite3_column_count(sql_vm);
	cur->colnames = LUA_NOREF;
	cur->coltypes = LUA_NOREF;
	cur->conn_data = conn;
	cur->sql_vm = sql_vm;

	/* increment cursor counter on connection */
	conn->cur_counter++;

	return 1;
}

/*
===============
env_connect

Environment connect function
===============
*/
static int env_connect(lua_State *L)
{
	env_data *env = getenvironment(L);
	const char *sourcename = luaL_checkstring(L, 2);
	conn_data *conn = NULL;
	sqlite3 *sql_conn;

	(void)env; /* not used */

	/* Try to open database */
	sql_conn = SQLite_Open(sourcename);
	if (!sql_conn) {
		return luasql_faildirect(L, "could not connect to database");
	}

	conn = (conn_data *)lua_newuserdata(L, sizeof(conn_data));
	luasql_setmeta(L, LUASQL_CONNECTION_SQLITE);

	/* fill in structure */
	conn->closed = 0;
	conn->env = LUA_NOREF;
	conn->auto_commit = 1;
	conn->cur_counter = 0;
	conn->sql_conn = sql_conn;

	return 1;
}

/*
===============
env_close

Environment close function
===============
*/
static int env_close(lua_State *L)
{
	env_data *env = getenvironment(L);
	(void)env; /* not used */
	return 0;
}

/*
===============
conn_close

Connection close function
===============
*/
static int conn_close(lua_State *L)
{
	conn_data *conn = (conn_data *)luaL_checkudata(L, 1, LUASQL_CONNECTION_SQLITE);

	if (conn->closed) {
		return 0;
	}

	if (conn->cur_counter > 0) {
		return luasql_failmsg(L, "cannot close connection", "there are open cursors");
	}

	SQLite_Close(conn->sql_conn);
	conn->sql_conn = NULL;
	conn->closed = 1;

	luaL_unref(L, LUA_REGISTRYINDEX, conn->env);

	return 0;
}

/*
===============
conn_execute

Connection execute function
===============
*/
static int conn_execute(lua_State *L)
{
	conn_data *conn = getconnection(L);
	const char *statement = luaL_checkstring(L, 2);
	sqlite3_stmt *sql_vm;
	int res;

	/* compile SQL statement */
	res = SQLite_Prepare(conn->sql_conn, statement, &sql_vm, NULL);
	if (res != SQLITE_OK) {
		return luasql_failmsg(L, "error executing query", "prepare failed");
	}

	/* execute statement */
	res = SQLite_Step(sql_vm);
	if (res == SQLITE_ROW) {
		/* return cursor */
		return cur_create(L, conn, sql_vm);
	}

	if (res == SQLITE_DONE) {
		/* statement completed */
		SQLite_Finalize(sql_vm);

		/* return number of changes */
		lua_pushinteger(L, SQLite_Changes(conn->sql_conn));
		return 1;
	}

	/* error */
	SQLite_Finalize(sql_vm);
	return luasql_failmsg(L, "error executing query", "execution failed");
}

/*
===============
conn_commit

Connection commit function
===============
*/
static int conn_commit(lua_State *L)
{
	conn_data *conn = getconnection(L);

	if (SQLite_Exec(conn->sql_conn, "COMMIT", NULL, NULL) != SQLITE_OK) {
		return luasql_faildirect(L, "cannot commit transaction");
	}

	return 0;
}

/*
===============
conn_rollback

Connection rollback function
===============
*/
static int conn_rollback(lua_State *L)
{
	conn_data *conn = getconnection(L);

	if (SQLite_Exec(conn->sql_conn, "ROLLBACK", NULL, NULL) != SQLITE_OK) {
		return luasql_faildirect(L, "cannot rollback transaction");
	}

	return 0;
}

/*
===============
conn_setautocommit

Connection set autocommit function
===============
*/
static int conn_setautocommit(lua_State *L)
{
	conn_data *conn = getconnection(L);
	conn->auto_commit = lua_toboolean(L, 2);
	return 0;
}

/*
===============
cur_close

Cursor close function
===============
*/
static int cur_close(lua_State *L)
{
	cur_data *cur = (cur_data *)luaL_checkudata(L, 1, LUASQL_CURSOR_SQLITE);

	if (cur->closed) {
		return 0;
	}

	SQLite_Finalize(cur->sql_vm);
	cur_nullify(L, cur);

	return 0;
}

/*
===============
cur_getcoltypes

Get column type information
===============
*/
static int cur_getcoltypes(lua_State *L)
{
	cur_data *cur = getcursor(L);

	if (cur->coltypes == LUA_NOREF) {
		lua_newtable(L);
		// Note: SQLite doesn't provide explicit column type info
		// We could implement type detection here if needed
		cur->coltypes = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, cur->coltypes);
	return 1;
}

/*
===============
cur_getcolnames

Get column name information
===============
*/
static int cur_getcolnames(lua_State *L)
{
	cur_data *cur = getcursor(L);

	if (cur->colnames == LUA_NOREF) {
		int i;
		lua_newtable(L);

		for (i = 0; i < cur->numcols; i++) {
			// Note: SQLite wrapper doesn't provide column names
			// We could implement this if needed
			char colName[32];
			Q_secure_snprintf(colName, sizeof(colName), "col%d", i + 1);
			lua_pushstring(L, colName);
			lua_rawseti(L, -2, i + 1);
		}

		cur->colnames = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, cur->colnames);
	return 1;
}

/*
===============
cur_fetch

Cursor fetch function
===============
*/
static int cur_fetch(lua_State *L)
{
	cur_data *cur = getcursor(L);
	int res;

	res = SQLite_Step(cur->sql_vm);
	if (res == SQLITE_ROW) {
		int i;
		lua_newtable(L);

		for (i = 0; i < cur->numcols; i++) {
			// For now, try to get as text, then try numeric conversion
			const char *text = SQLite_GetColumnText(cur->sql_vm, i);
			if (text) {
				// Try to convert to number first
				char *endptr;
				long int_val = strtol(text, &endptr, 10);
				if (*endptr == '\0') {
					// It's an integer
					lua_pushinteger(L, int_val);
				} else {
					// Try float
					double float_val = strtod(text, &endptr);
					if (*endptr == '\0') {
						lua_pushnumber(L, float_val);
					} else {
						// It's a string
						lua_pushstring(L, text);
					}
				}
			} else {
				lua_pushnil(L);
			}

			lua_rawseti(L, -2, i + 1);
		}

		return 1;
	}

	if (res == SQLITE_DONE) {
		return 0; // No more rows
	}

	// Error
	return luasql_faildirect(L, "error fetching row");
}

/*
===============
create_environment

Create SQLite environment
===============
*/
static int create_environment(lua_State *L)
{
	env_data *env = (env_data *)lua_newuserdata(L, sizeof(env_data));
	luasql_setmeta(L, LUASQL_ENVIRONMENT_SQLITE);

	/* fill in structure */
	env->closed = 0;

	return 1;
}

/*
===============
Environment methods
===============
*/
static const luaL_Reg env_methods[] = {
	{"__gc", env_close},
	{"close", env_close},
	{"connect", env_connect},
	{NULL, NULL}
};

/*
===============
Connection methods
===============
*/
static const luaL_Reg conn_methods[] = {
	{"__gc", conn_close},
	{"close", conn_close},
	{"execute", conn_execute},
	{"commit", conn_commit},
	{"rollback", conn_rollback},
	{"setautocommit", conn_setautocommit},
	{NULL, NULL}
};

/*
===============
Cursor methods
===============
*/
static const luaL_Reg cur_methods[] = {
	{"__gc", cur_close},
	{"close", cur_close},
	{"getcoltypes", cur_getcoltypes},
	{"getcolnames", cur_getcolnames},
	{"fetch", cur_fetch},
	{NULL, NULL}
};

/*
===============
luaopen_luasql_sqlite3

Initialize SQLite3 LuaSQL driver
===============
*/
int luaopen_luasql_sqlite3(lua_State *L)
{
	// Create environment metatable
	luasql_createmeta(L, LUASQL_ENVIRONMENT_SQLITE, env_methods);

	// Create connection metatable
	luasql_createmeta(L, LUASQL_CONNECTION_SQLITE, conn_methods);

	// Create cursor metatable
	luasql_createmeta(L, LUASQL_CURSOR_SQLITE, cur_methods);

	// Create main table
	lua_pushstring(L, LUASQL_TABLENAME);
	lua_newtable(L);

	// Set info
	luasql_set_info(L);

	// Set create environment function
	lua_pushstring(L, "sqlite3");
	lua_pushcfunction(L, create_environment);
	lua_settable(L, -3);

	return 1;
}

#endif // USE_SQLITE
#endif // USE_LUA

#endif // USE_LUASQL