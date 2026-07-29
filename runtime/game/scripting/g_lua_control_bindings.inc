/* Generic script-control Lua bindings: cvars, console, and P2P state.
 * Included by g_lua_bindings.c so binding functions remain file-local. */

/* ========== Generic script control bindings ========== */

static int l_cvars_getString( lua_State *L )
{
	char value[MAX_CVAR_VALUE_STRING];

	Cvar_VariableStringBuffer( luaL_checkstring( L, 1 ), value, sizeof( value ) );
	lua_pushstring( L, value );
	return 1;
}

static int l_cvars_getNumber( lua_State *L )
{
	lua_pushnumber( L, Cvar_VariableValue( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_cvars_getInteger( lua_State *L )
{
	lua_pushinteger( L, Cvar_VariableIntegerValue( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_cvars_getBoolean( lua_State *L )
{
	lua_pushboolean( L, Cvar_VariableIntegerValue( luaL_checkstring( L, 1 ) ) ? qtrue : qfalse );
	return 1;
}

static int l_cvars_exists( lua_State *L )
{
	lua_pushboolean( L, Cvar_Flags( luaL_checkstring( L, 1 ) ) != CVAR_NONEXISTENT );
	return 1;
}

static int l_cvars_flags( lua_State *L )
{
	lua_pushinteger( L, (lua_Integer)Cvar_Flags( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_cvars_set( lua_State *L )
{
	Cvar_Set( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) );
	return 0;
}

static int l_cvars_setNumber( lua_State *L )
{
	char value[64];

	Com_sprintf( value, sizeof( value ), "%g", luaL_checknumber( L, 2 ) );
	Cvar_Set( luaL_checkstring( L, 1 ), value );
	return 0;
}

static int l_cvars_setInteger( lua_State *L )
{
	char value[32];

	Com_sprintf( value, sizeof( value ), "%d", (int)luaL_checkinteger( L, 2 ) );
	Cvar_Set( luaL_checkstring( L, 1 ), value );
	return 0;
}

static int l_cvars_setBoolean( lua_State *L )
{
	Cvar_Set( luaL_checkstring( L, 1 ), lua_toboolean( L, 2 ) ? "1" : "0" );
	return 0;
}

static int l_cvars_reset( lua_State *L )
{
	Cvar_Reset( luaL_checkstring( L, 1 ) );
	return 0;
}

static int l_console_exec( lua_State *L )
{
	const char *text = luaL_checkstring( L, 1 );
	const char *mode = luaL_optstring( L, 2, "append" );
	cbufExec_t when = EXEC_APPEND;

	if ( !Q_stricmp( mode, "insert" ) ) {
		when = EXEC_INSERT;
	} else if ( !Q_stricmp( mode, "now" ) ) {
		when = EXEC_NOW;
	}

	Cbuf_ExecuteText( when, text );
	return 0;
}

static int l_console_addText( lua_State *L )
{
	Cbuf_AddText( luaL_checkstring( L, 1 ) );
	return 0;
}

static int l_p2p_getSession( lua_State *L )
{
	lua_newtable( L );

	lua_pushstring( L, CL_P2P_SessionId() );
	lua_setfield( L, -2, "id" );
	lua_pushstring( L, CL_P2P_SessionAddress() );
	lua_setfield( L, -2, "address" );
	lua_pushstring( L, CL_P2P_SessionFailoverPolicy() );
	lua_setfield( L, -2, "failover" );
	lua_pushinteger( L, CL_P2P_SessionReconnectWindowSec() );
	lua_setfield( L, -2, "reconnectWindowSec" );
	lua_pushinteger( L, CL_P2P_SessionAttemptCount() );
	lua_setfield( L, -2, "attemptCount" );
	lua_pushboolean( L, CL_P2P_SessionPending() );
	lua_setfield( L, -2, "pending" );
	lua_pushboolean( L, CL_P2P_SessionMigratePending() );
	lua_setfield( L, -2, "migratePending" );
	lua_pushstring( L, CL_P2P_SessionMigrateAddress() );
	lua_setfield( L, -2, "migrateAddress" );
	lua_pushstring( L, CL_P2P_SessionCurrentTarget() );
	lua_setfield( L, -2, "currentTarget" );
	lua_pushstring( L, CL_P2P_SessionRecoveryStopReason() );
	lua_setfield( L, -2, "recoveryStopReason" );
	lua_pushboolean( L, CL_P2P_SessionIsBackupHostEligible() );
	lua_setfield( L, -2, "backupHostEligible" );

	return 1;
}

static int l_p2p_isBackupHostEligible( lua_State *L )
{
	(void)L;
	lua_pushboolean( L, CL_P2P_SessionIsBackupHostEligible() );
	return 1;
}
