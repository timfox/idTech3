/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client P2P session reconnect and listen-host migration orchestration.
===========================================================================
*/

#include "client.h"
#include "cl_p2p_session.h"
#include "../../qcommon/net_p2p.h"
#include "../../qcommon/script_emit.h"

#include <string.h>

extern cvar_t *cl_reconnectArgs;

typedef struct {
	qboolean active;
	char sessionId[64];
	char p2pAddr[MAX_STRING_CHARS];
	char failover[32];
	int reconnectWindowSec;
	int disconnectTime;
	int nextAttemptTime;
	int attemptCount;
	qboolean pending;
	qboolean migratePending;
	char migrateAddr[MAX_STRING_CHARS];
} cl_p2p_session_t;

static cvar_t *cl_p2pAutoReconnect;
static cvar_t *cl_p2pReconnectLog;
static cvar_t *cl_p2pBackupHost;

static cl_p2p_session_t cl_p2pSession;
static qboolean cl_p2pDisconnectServerInitiated;

static void CL_P2P_SessionRegisterCvars( void )
{
	if ( cl_p2pAutoReconnect && cl_p2pReconnectLog && cl_p2pBackupHost ) {
		return;
	}

	cl_p2pAutoReconnect = Cvar_Get( "cl_p2pAutoReconnect", "1", CVAR_ARCHIVE_ND );
	cl_p2pReconnectLog = Cvar_Get( "cl_p2pReconnectLog", "1", CVAR_ARCHIVE_ND );
	cl_p2pBackupHost = Cvar_Get( "cl_p2pBackupHost", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_p2pAutoReconnect, "Auto-reconnect within advertised P2P reconnect window (0=off, 1=on)." );
	Cvar_SetDescription( cl_p2pReconnectLog, "Log P2P reconnect attempts (0=off, 1=on)." );
	Cvar_SetDescription( cl_p2pBackupHost, "Eligible to promote as backup listen host on migration (0=off, 1=on)." );
}

static qboolean CL_P2P_SessionFailoverAllowsRecovery( void )
{
	if ( !cl_p2pSession.failover[0] ) {
		return qtrue;
	}
	return (qboolean)( Q_stricmp( cl_p2pSession.failover, "none" ) != 0 );
}

static int CL_P2P_SessionBackoffMs( int attempt )
{
	int ms = 1000;

	if ( attempt > 0 ) {
		ms <<= attempt;
		if ( ms > 8000 ) {
			ms = 8000;
		}
	}
	return ms;
}

void CL_P2P_SessionInit( void )
{
	CL_P2P_SessionRegisterCvars();
	Com_Printf( "P2P session: auto-reconnect %s\n",
		( cl_p2pAutoReconnect && cl_p2pAutoReconnect->integer ) ? "enabled" : "disabled" );
}

void CL_P2P_SessionShutdown( void )
{
	Com_Memset( &cl_p2pSession, 0, sizeof( cl_p2pSession ) );
}

void CL_P2P_SessionOnConnect( const char *sessionId, const char *p2pAddr, const char *failover, int reconnectWindowSec )
{
	CL_P2P_SessionRegisterCvars();
	Com_Memset( &cl_p2pSession, 0, sizeof( cl_p2pSession ) );
	cl_p2pSession.active = qtrue;
	if ( sessionId && sessionId[0] ) {
		Q_strncpyz( cl_p2pSession.sessionId, sessionId, sizeof( cl_p2pSession.sessionId ) );
	}
	if ( p2pAddr && p2pAddr[0] ) {
		Q_strncpyz( cl_p2pSession.p2pAddr, p2pAddr, sizeof( cl_p2pSession.p2pAddr ) );
	}
	if ( failover && failover[0] ) {
		Q_strncpyz( cl_p2pSession.failover, failover, sizeof( cl_p2pSession.failover ) );
	}
	cl_p2pSession.reconnectWindowSec = reconnectWindowSec > 0 ? reconnectWindowSec : 0;
	Com_ScriptEmitEvent( "p2p_session_connect",
		cl_p2pSession.p2pAddr, cl_p2pSession.sessionId,
		cl_p2pSession.reconnectWindowSec, 0 );
}

void CL_P2P_SessionOnConnectFromServerInfo( const char *serverInfo )
{
	const char *p2pFlag;
	const char *sessionId;
	const char *p2pAddr;
	const char *failover;
	int reconnectWindow;

	if ( !serverInfo || !NET_P2P_IsEnabled() ) {
		return;
	}

	p2pFlag = Info_ValueForKey( serverInfo, "p2p" );
	if ( !p2pFlag || !p2pFlag[0] || !atoi( p2pFlag ) ) {
		return;
	}

	sessionId = Info_ValueForKey( serverInfo, "p2psession" );
	p2pAddr = Info_ValueForKey( serverInfo, "p2paddr" );
	failover = Info_ValueForKey( serverInfo, "p2pfail" );
	reconnectWindow = atoi( Info_ValueForKey( serverInfo, "p2preconn" ) );

	if ( !p2pAddr || !p2pAddr[0] ) {
		p2pAddr = cls.servername;
	}

	CL_P2P_SessionOnConnect( sessionId, p2pAddr, failover, reconnectWindow );
}

void CL_P2P_SessionPrepareDisconnect( qboolean serverInitiated )
{
	cl_p2pDisconnectServerInitiated = serverInitiated;
}

void CL_P2P_SessionNotifyDisconnect( void )
{
	CL_P2P_SessionOnDisconnect( cl_p2pDisconnectServerInitiated );
	cl_p2pDisconnectServerInitiated = qfalse;
}

static void CL_P2P_SessionBroadcastMigrate( const char *newAddr )
{
	char address[MAX_STRING_CHARS];
	netadr_t to;

	if ( !newAddr || !newAddr[0] || !cl_p2pSession.sessionId[0] ) {
		return;
	}

	if ( !NET_P2P_GetLocalAddressString( address, sizeof( address ) ) ) {
		Q_strncpyz( address, newAddr, sizeof( address ) );
	}

	to.type = NA_BROADCAST;
	to.port = BigShort( (unsigned short)Cvar_VariableIntegerValue( "net_port" ) );
	NET_OutOfBandPrint( NS_CLIENT, &to, "p2pMigrate %s %s %d",
		cl_p2pSession.sessionId, address, rand() );
	NET_OutOfBandPrint( NS_SERVER, &to, "p2pMigrate %s %s %d",
		cl_p2pSession.sessionId, address, rand() );

	if ( cl_p2pReconnectLog && cl_p2pReconnectLog->integer ) {
		Com_Printf( "P2P migrate: broadcast new host %s\n", address );
	}
}

static void CL_P2P_SessionTryPromoteBackupHost( void )
{
	char address[MAX_STRING_CHARS];

	if ( !CL_P2P_SessionIsBackupHostEligible() ) {
		return;
	}
	if ( Q_stricmp( cl_p2pSession.failover, "migrate" ) != 0 ) {
		return;
	}
	if ( com_sv_running && com_sv_running->integer ) {
		return;
	}

	if ( cl_reconnectArgs && cl_reconnectArgs->string[0] ) {
		Cbuf_AddText( "listen\n" );
	}

	if ( NET_P2P_GetLocalAddressString( address, sizeof( address ) ) ) {
		CL_P2P_SessionBroadcastMigrate( address );
	}
}

void CL_P2P_SessionOnDisconnect( qboolean serverInitiated )
{
	int now;
	qboolean hadActive = cl_p2pSession.active;

	CL_P2P_SessionRegisterCvars();

	if ( !serverInitiated ) {
		if ( hadActive ) {
			Com_ScriptEmitEvent( "p2p_session_disconnect",
				cl_p2pSession.p2pAddr, cl_p2pSession.sessionId, 0, 0 );
		}
		Com_Memset( &cl_p2pSession, 0, sizeof( cl_p2pSession ) );
		return;
	}

	if ( !cl_p2pSession.active || !NET_P2P_IsEnabled() ) {
		Com_Memset( &cl_p2pSession, 0, sizeof( cl_p2pSession ) );
		return;
	}

	if ( !cl_p2pAutoReconnect || !cl_p2pAutoReconnect->integer ) {
		Com_Memset( &cl_p2pSession, 0, sizeof( cl_p2pSession ) );
		return;
	}

	if ( !CL_P2P_SessionFailoverAllowsRecovery() || cl_p2pSession.reconnectWindowSec <= 0 ) {
		Com_Memset( &cl_p2pSession, 0, sizeof( cl_p2pSession ) );
		return;
	}

	now = Sys_Milliseconds();
	cl_p2pSession.disconnectTime = now;
	cl_p2pSession.pending = qtrue;
	cl_p2pSession.nextAttemptTime = now + CL_P2P_SessionBackoffMs( 0 );
	cl_p2pSession.attemptCount = 0;
	Com_ScriptEmitEvent( "p2p_session_disconnect",
		cl_p2pSession.p2pAddr, cl_p2pSession.sessionId, 1, 0 );
	Com_ScriptEmitEvent( "p2p_reconnect_scheduled",
		cl_p2pSession.p2pAddr, cl_p2pSession.failover,
		cl_p2pSession.reconnectWindowSec, 0 );

	if ( cl_p2pReconnectLog && cl_p2pReconnectLog->integer ) {
		Com_Printf( "P2P reconnect: scheduled within %ds window (policy %s)\n",
			cl_p2pSession.reconnectWindowSec,
			cl_p2pSession.failover[0] ? cl_p2pSession.failover : "reconnect" );
	}

	if ( serverInitiated && Q_stricmp( cl_p2pSession.failover, "migrate" ) == 0 ) {
		CL_P2P_SessionTryPromoteBackupHost();
	}
}

void CL_P2P_SessionOnMigrate( const char *sessionId, const char *newP2pAddr )
{
	if ( !sessionId || !newP2pAddr || !newP2pAddr[0] ) {
		return;
	}

	if ( cl_p2pSession.sessionId[0] && Q_stricmp( cl_p2pSession.sessionId, sessionId ) != 0 ) {
		return;
	}

	Q_strncpyz( cl_p2pSession.migrateAddr, newP2pAddr, sizeof( cl_p2pSession.migrateAddr ) );
	cl_p2pSession.migratePending = qtrue;
	cl_p2pSession.pending = qtrue;
	cl_p2pSession.disconnectTime = Sys_Milliseconds();
	cl_p2pSession.nextAttemptTime = cl_p2pSession.disconnectTime + 500;
	cl_p2pSession.attemptCount = 0;
	Com_ScriptEmitEvent( "p2p_session_migrate",
		cl_p2pSession.migrateAddr, sessionId, 1, 0 );

	if ( cl_p2pReconnectLog && cl_p2pReconnectLog->integer ) {
		Com_Printf( "P2P migrate: new host %s for session %s\n", newP2pAddr, sessionId );
	}
}

void CL_P2P_SessionFrame( void )
{
	int now;
	int elapsedSec;
	int remainsSec;
	char connectTarget[MAX_STRING_CHARS];

	CL_P2P_SessionRegisterCvars();

	if ( !cl_p2pSession.pending || !NET_P2P_IsEnabled() ) {
		return;
	}

	now = Sys_Milliseconds();
	if ( now < cl_p2pSession.nextAttemptTime ) {
		return;
	}

	elapsedSec = ( now - cl_p2pSession.disconnectTime ) / 1000;
	remainsSec = cl_p2pSession.reconnectWindowSec - elapsedSec;
	if ( remainsSec <= 0 ) {
		if ( cl_p2pReconnectLog && cl_p2pReconnectLog->integer ) {
			Com_Printf( "P2P reconnect: window expired\n" );
		}
		Com_ScriptEmitEvent( "p2p_reconnect_expired",
			cl_p2pSession.p2pAddr, cl_p2pSession.sessionId,
			cl_p2pSession.attemptCount, 0 );
		CL_P2P_SessionShutdown();
		return;
	}

	if ( cls.state != CA_DISCONNECTED && cls.state != CA_CONNECTING ) {
		cl_p2pSession.pending = qfalse;
		return;
	}

	cl_p2pSession.attemptCount++;

	if ( cl_p2pSession.migratePending && cl_p2pSession.migrateAddr[0] ) {
		Q_strncpyz( connectTarget, cl_p2pSession.migrateAddr, sizeof( connectTarget ) );
	} else if ( cl_p2pSession.p2pAddr[0] ) {
		Q_strncpyz( connectTarget, cl_p2pSession.p2pAddr, sizeof( connectTarget ) );
	} else {
		CL_P2P_SessionShutdown();
		return;
	}

	if ( cl_p2pReconnectLog && cl_p2pReconnectLog->integer ) {
		Com_Printf( "P2P reconnect: attempt %d, window %ds remaining -> %s\n",
			cl_p2pSession.attemptCount, remainsSec, connectTarget );
	}
	Com_ScriptEmitEvent( "p2p_reconnect_attempt",
		connectTarget, cl_p2pSession.sessionId,
		cl_p2pSession.attemptCount, remainsSec );

	NET_P2P_BeginConnectPath( connectTarget );
	Cbuf_AddText( va( "connect %s\n", connectTarget ) );
	cl_p2pSession.nextAttemptTime = now + CL_P2P_SessionBackoffMs( cl_p2pSession.attemptCount );
}

qboolean CL_P2P_SessionHandleOobPacket( const netadr_t *from, const char *cmd )
{
	const char *sessionId;
	const char *newAddr;

	(void)from;

	if ( !cmd || Q_stricmp( cmd, "p2pMigrate" ) != 0 ) {
		return qfalse;
	}

	sessionId = Cmd_Argv( 1 );
	newAddr = Cmd_Argv( 2 );
	CL_P2P_SessionOnMigrate( sessionId, newAddr );
	return qtrue;
}

qboolean CL_P2P_SessionIsBackupHostEligible( void )
{
	CL_P2P_SessionRegisterCvars();
	return (qboolean)( cl_p2pBackupHost && cl_p2pBackupHost->integer );
}

const char *CL_P2P_SessionId( void )
{
	return cl_p2pSession.sessionId;
}

const char *CL_P2P_SessionAddress( void )
{
	return cl_p2pSession.p2pAddr;
}

const char *CL_P2P_SessionFailoverPolicy( void )
{
	return cl_p2pSession.failover;
}

int CL_P2P_SessionReconnectWindowSec( void )
{
	return cl_p2pSession.reconnectWindowSec;
}

int CL_P2P_SessionAttemptCount( void )
{
	return cl_p2pSession.attemptCount;
}

qboolean CL_P2P_SessionPending( void )
{
	return cl_p2pSession.pending;
}

qboolean CL_P2P_SessionMigratePending( void )
{
	return cl_p2pSession.migratePending;
}

const char *CL_P2P_SessionMigrateAddress( void )
{
	return cl_p2pSession.migrateAddr;
}
