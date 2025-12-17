/*
===========================================================================
Lua Events System Test

Tests the Lua event bus functionality
===========================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/lua_events.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Test global variables
static lua_State *g_L = NULL;
static int g_event_received = 0;
static const char *g_last_event_name = NULL;
static int g_event_arg_count = 0;

// Mock Lua state for testing
lua_State *Test_GetLuaState(void) {
    if (!g_L) {
        g_L = luaL_newstate();
        luaL_openlibs(g_L);
        Lua_Events_RegisterBindings(g_L);
    }
    return g_L;
}

void Test_CleanupLuaState(void) {
    if (g_L) {
        lua_close(g_L);
        g_L = NULL;
    }
    Lua_Events_Shutdown();
}

// Test callback functions
static int Test_EventHandler1(lua_State *L) {
    g_event_received = 1;
    g_last_event_name = "test_event_1";
    g_event_arg_count = lua_gettop(L);
    return 0;
}

static int Test_EventHandler2(lua_State *L) {
    g_event_received = 2;
    g_last_event_name = "test_event_2";
    g_event_arg_count = lua_gettop(L);
    return 0;
}

// Test functions
TEST(lua_events_init_shutdown) {
    Lua_Events_Init();
    ASSERT_TRUE(Lua_Events_Init() == qfalse); // Should not init twice

    Lua_Events_Shutdown();
    ASSERT_TRUE(Lua_Events_Shutdown() == qfalse); // Should not shutdown twice

    PASS();
}

TEST(lua_events_basic_emit) {
    lua_State *L = Test_GetLuaState();

    // Subscribe to an event
    lua_getglobal(L, "Events");
    lua_getfield(L, -1, "on");
    lua_pushstring(L, "test_event");
    lua_pushcfunction(L, Test_EventHandler1);
    lua_call(L, 2, 1);
    ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 2); // Remove result and Events table

    // Emit the event
    Lua_Events_Emit("test_event", 0);

    // Process events
    Lua_Events_Update();

    // Check that handler was called
    ASSERT_EQ(g_event_received, 1);
    ASSERT_STR_EQ(g_last_event_name, "test_event");
    ASSERT_EQ(g_event_arg_count, 0);

    PASS();
}

TEST(lua_events_with_args) {
    lua_State *L = Test_GetLuaState();

    // Subscribe to an event
    lua_getglobal(L, "Events");
    lua_getfield(L, -1, "on");
    lua_pushstring(L, "test_event_args");
    lua_pushcfunction(L, Test_EventHandler2);
    lua_call(L, 2, 1);
    lua_pop(L, 2); // Remove result and Events table

    // Emit the event with arguments
    Lua_Events_Emit("test_event_args", 2, "hello", 42);

    // Process events
    Lua_Events_Update();

    // Check that handler was called with args
    ASSERT_EQ(g_event_received, 2);
    ASSERT_STR_EQ(g_last_event_name, "test_event_args");

    PASS();
}

TEST(lua_events_once_subscription) {
    lua_State *L = Test_GetLuaState();

    // Subscribe once to an event
    lua_getglobal(L, "Events");
    lua_getfield(L, -1, "once");
    lua_pushstring(L, "test_once");
    lua_pushcfunction(L, Test_EventHandler1);
    lua_call(L, 2, 1);
    lua_pop(L, 2); // Remove result and Events table

    // Emit the event twice
    Lua_Events_Emit("test_once", 0);
    Lua_Events_Update();
    ASSERT_EQ(g_event_received, 1);

    g_event_received = 0; // Reset

    Lua_Events_Emit("test_once", 0);
    Lua_Events_Update();
    ASSERT_EQ(g_event_received, 0); // Should not be called again

    PASS();
}

TEST(lua_events_unsubscribe) {
    lua_State *L = Test_GetLuaState();

    // Subscribe to an event
    lua_getglobal(L, "Events");
    lua_getfield(L, -1, "on");
    lua_pushstring(L, "test_unsub");
    lua_pushcfunction(L, Test_EventHandler1);
    lua_call(L, 2, 1);
    lua_pop(L, 2); // Remove result and Events table

    // Emit event - should be received
    Lua_Events_Emit("test_unsub", 0);
    Lua_Events_Update();
    ASSERT_EQ(g_event_received, 1);

    // Unsubscribe
    lua_getglobal(L, "Events");
    lua_getfield(L, -1, "off");
    lua_pushstring(L, "test_unsub");
    lua_pushcfunction(L, Test_EventHandler1);
    lua_call(L, 2, 1);
    lua_pop(L, 2); // Remove result and Events table

    // Reset and emit again - should not be received
    g_event_received = 0;
    Lua_Events_Emit("test_unsub", 0);
    Lua_Events_Update();
    ASSERT_EQ(g_event_received, 0);

    PASS();
}

TEST(lua_events_lua_emit) {
    lua_State *L = Test_GetLuaState();

    // Subscribe to an event from Lua
    lua_getglobal(L, "Events");
    lua_getfield(L, -1, "on");
    lua_pushstring(L, "lua_emit_test");
    lua_pushcfunction(L, Test_EventHandler1);
    lua_call(L, 2, 1);
    lua_pop(L, 2); // Remove result and Events table

    // Emit from Lua
    lua_getglobal(L, "Events");
    lua_getfield(L, -1, "emit");
    lua_pushstring(L, "lua_emit_test");
    lua_pushstring(L, "arg1");
    lua_pushnumber(L, 123);
    lua_call(L, 3, 0);
    lua_pop(L, 1); // Remove Events table

    // Process events
    Lua_Events_Update();

    // Check that handler was called
    ASSERT_EQ(g_event_received, 1);
    ASSERT_STR_EQ(g_last_event_name, "lua_emit_test");

    PASS();
}

// Test main function
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // Initialize the event system
    Lua_Events_Init();

    // Run tests
    RUN_TEST(lua_events_init_shutdown);
    RUN_TEST(lua_events_basic_emit);
    RUN_TEST(lua_events_with_args);
    RUN_TEST(lua_events_once_subscription);
    RUN_TEST(lua_events_unsubscribe);
    RUN_TEST(lua_events_lua_emit);

    // Cleanup
    Test_CleanupLuaState();

    return 0;
}
