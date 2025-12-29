/*
===========================================================================
id Tech 3 - LuaSQL Test

Tests for LuaSQL database integration
===========================================================================
*/

#include "test_framework.h"
#include "sqlite_wrapper.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifdef USE_LUASQL

/*
===============
Test SQLite wrapper functions
===============
*/
TEST(sqlite_wrapper_basic)
{
	sqlite3 *db;
	int result;

	// Test opening database
	db = SQLite_Open(":memory:");
	ASSERT_TRUE(db != NULL);

	if (db) {
		// Test executing SQL
		result = SQLite_Exec(db, "CREATE TABLE test (id INTEGER, name TEXT)", NULL, NULL);
		ASSERT_EQ(result, SQLITE_OK);

		result = SQLite_Exec(db, "INSERT INTO test VALUES (1, 'test')", NULL, NULL);
		ASSERT_EQ(result, SQLITE_OK);

		// Test closing database
		SQLite_Close(db);
	}

	PASS();
}

/*
===============
Test LuaSQL integration
===============
*/
TEST(luasql_integration)
{
	lua_State *L;

	// Create Lua state
	L = luaL_newstate();
	ASSERT_TRUE(L != NULL);

	if (L) {
		// Open libraries
		luaL_openlibs(L);

		// Test loading LuaSQL
		int result = luaopen_luasql_sqlite3(L);
		ASSERT_EQ(result, 1); // Should return 1 (the table)

		// Check that luasql table was created
		lua_getglobal(L, "luasql");
		ASSERT_TRUE(lua_istable(L, -1));

		if (lua_istable(L, -1)) {
			// Check for sqlite3 environment function
			lua_getfield(L, -1, "sqlite3");
			ASSERT_TRUE(lua_isfunction(L, -1));
			lua_pop(L, 1);
		}

		lua_pop(L, 1);

		// Close Lua state
		lua_close(L);
	}

	PASS();
}

#endif // USE_LUASQL

/*
===============
Main test runner
===============
*/
int main(int argc, char *argv[])
{
#ifdef USE_LUASQL
	RUN_TEST(sqlite_wrapper_basic);
	RUN_TEST(luasql_integration);
#else
	printf("LuaSQL support not enabled - skipping tests\n");
#endif

	return 0;
}