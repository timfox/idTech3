/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Enhanced server features implementation.

Backward reconciliation (Unlagged):
  Records entity bounding boxes each server frame into a circular
  history buffer. When a client fires a hitscan weapon, the server
  rewinds entity positions to the client's perceived time (accounting
  for latency), performs the trace, then restores positions. This makes
  hitscan weapons feel responsive at any ping.

Server pause:
  Freezes the game clock while keeping network connections alive.
  Essential for competitive/LAN events.
===========================================================================
*/

#include "server.h"
#include "sv_enhanced.h"

static unlaggedState_t  unlagState;
static qboolean         unlagInitialized = qfalse;
static qboolean         serverPaused = qfalse;

/* saved entity state for restore after rewind */
static vec3_t savedOrigin[MAX_GENTITIES];
static vec3_t savedMins[MAX_GENTITIES];
static vec3_t savedMaxs[MAX_GENTITIES];
static qboolean savedValid[MAX_GENTITIES];

/* ---- Cvars ---- */

static cvar_t *sv_unlagged;
static cvar_t *sv_unlaggedMaxRewind;
static cvar_t *sv_unlaggedProjectiles;

static cvar_t *sv_pauseEnabled;

static cvar_t *sv_latencyEqualizer;
static cvar_t *sv_latencyEqualizerTarget;

/* Movement physics */
static cvar_t *g_airControl;
static cvar_t *g_airControlAmount;
static cvar_t *g_rampJump;
static cvar_t *g_additiveJump;
static cvar_t *g_crouchSlide;
static cvar_t *g_wallJump;
static cvar_t *g_doubleJump;
static cvar_t *g_doubleJumpWindow;

/* Voting */
static cvar_t *g_allowVote;
static cvar_t *g_voteDelay;
static cvar_t *g_voteLimit;
static cvar_t *g_voteMinPlayers;

/* Anti-abuse */
static cvar_t *sv_antiWallhack;
static cvar_t *sv_floodProtectExt;
static cvar_t *sv_floodProtectWindow;

/* Team balance */
static cvar_t *g_teamBalance;
static cvar_t *g_teamBalanceThreshold;
static cvar_t *g_autoBalance;

/* Gameplay */
static cvar_t *g_missilesThruTeleporters;
static cvar_t *g_selfDamage;
static cvar_t *g_weaponRespawn;
static cvar_t *g_armorProtection;
static cvar_t *g_startHealth;
static cvar_t *g_startArmor;
static cvar_t *g_maxHealth;
static cvar_t *g_maxArmor;
static cvar_t *g_knockback;
static cvar_t *g_weaponDamageScale;

/* Warmup */
static cvar_t *g_warmupReady;
static cvar_t *g_warmupReadyPercentage;
static cvar_t *g_warmupDelay;

void SV_Enhanced_Init( void ) {
	/* Unlagged — ARCHIVE only (SERVERINFO budget is tight with OA videoflags/voteflags). */
	sv_unlagged            = Cvar_Get( "sv_unlagged",            "1",    CVAR_ARCHIVE );
	sv_unlagged->flags &= ~(int)CVAR_SERVERINFO;
	sv_unlaggedMaxRewind   = Cvar_Get( "sv_unlaggedMaxRewind",   "800",  CVAR_ARCHIVE );
	sv_unlaggedProjectiles = Cvar_Get( "sv_unlaggedProjectiles", "0",    CVAR_ARCHIVE );
	Cvar_SetDescription( sv_unlagged, "Enable backward reconciliation for hitscan weapons (0 = off, 1 = on)." );
	Cvar_SetDescription( sv_unlaggedMaxRewind, "Maximum rewind time in ms for unlagged hit detection." );
	Cvar_SetDescription( sv_unlaggedProjectiles, "Enable delagged projectile spawning (0 = off, 1 = on)." );

	/* Pause */
	sv_pauseEnabled = Cvar_Get( "sv_pauseEnabled", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_pauseEnabled, "Allow server pause via /pause command (0 = disabled, 1 = enabled)." );

	/* Latency equalizer */
	sv_latencyEqualizer       = Cvar_Get( "sv_latencyEqualizer",       "0",   CVAR_ARCHIVE );
	sv_latencyEqualizerTarget = Cvar_Get( "sv_latencyEqualizerTarget", "50",  CVAR_ARCHIVE );
	Cvar_SetDescription( sv_latencyEqualizer, "Add artificial delay to low-ping players to equalize ping (0 = off)." );
	Cvar_SetDescription( sv_latencyEqualizerTarget, "Target ping in ms for latency equalizer." );

	/* Movement physics — not advertised in SERVERINFO (keeps OA videoflags/voteflags). */
	g_airControl       = Cvar_Get( "g_airControl",       "0",    CVAR_ARCHIVE );
	g_airControl->flags &= ~(int)CVAR_SERVERINFO;
	g_airControlAmount = Cvar_Get( "g_airControlAmount", "0.6",  CVAR_ARCHIVE );
	g_rampJump         = Cvar_Get( "g_rampJump",         "0",    CVAR_ARCHIVE );
	g_rampJump->flags &= ~(int)CVAR_SERVERINFO;
	g_additiveJump     = Cvar_Get( "g_additiveJump",     "0",    CVAR_ARCHIVE );
	g_additiveJump->flags &= ~(int)CVAR_SERVERINFO;
	g_crouchSlide      = Cvar_Get( "g_crouchSlide",      "0",    CVAR_ARCHIVE );
	g_crouchSlide->flags &= ~(int)CVAR_SERVERINFO;
	g_wallJump         = Cvar_Get( "g_wallJump",         "0",    CVAR_ARCHIVE );
	g_wallJump->flags &= ~(int)CVAR_SERVERINFO;
	g_doubleJump       = Cvar_Get( "g_doubleJump",       "0",    CVAR_ARCHIVE );
	g_doubleJump->flags &= ~(int)CVAR_SERVERINFO;
	g_doubleJumpWindow = Cvar_Get( "g_doubleJumpWindow", "400",  CVAR_ARCHIVE );
	Cvar_SetDescription( g_airControl, "Enable air control for strafing (0 = vanilla, 1 = CPM-style)." );
	Cvar_SetDescription( g_rampJump, "Enable ramp/slope jumping (0 = off, 1 = on)." );
	Cvar_SetDescription( g_additiveJump, "Additive jump velocity instead of fixed (0 = vanilla, 1 = additive)." );
	Cvar_SetDescription( g_crouchSlide, "Enable crouch sliding (0 = off, 1 = on)." );
	Cvar_SetDescription( g_wallJump, "Enable wall jumping (0 = off, 1 = on)." );
	Cvar_SetDescription( g_doubleJump, "Enable double jump (0 = off, 1 = on)." );

	/* Voting */
	g_allowVote      = Cvar_Get( "g_allowVote",      "1",  CVAR_ARCHIVE );
	g_allowVote->flags &= ~(int)CVAR_SERVERINFO;
	g_voteDelay      = Cvar_Get( "g_voteDelay",      "30", CVAR_ARCHIVE );
	g_voteLimit      = Cvar_Get( "g_voteLimit",      "3",  CVAR_ARCHIVE );
	g_voteMinPlayers = Cvar_Get( "g_voteMinPlayers", "2",  CVAR_ARCHIVE );
	Cvar_SetDescription( g_allowVote, "Allow voting (0 = disabled, 1 = enabled)." );
	Cvar_SetDescription( g_voteDelay, "Minimum seconds between votes from the same player." );
	Cvar_SetDescription( g_voteLimit, "Maximum votes per player per map." );

	/* Anti-abuse */
	sv_antiWallhack       = Cvar_Get( "sv_antiWallhack",       "0",    CVAR_ARCHIVE );
	sv_floodProtectExt    = Cvar_Get( "sv_floodProtectExt",    "1",    CVAR_ARCHIVE );
	sv_floodProtectWindow = Cvar_Get( "sv_floodProtectWindow", "1000", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_antiWallhack, "Server-side anti-wallhack: suppress entity data for players behind walls (0 = off, 1 = on)." );

	/* Team balance */
	g_teamBalance          = Cvar_Get( "g_teamBalance",          "1",  CVAR_ARCHIVE );
	g_teamBalanceThreshold = Cvar_Get( "g_teamBalanceThreshold", "2",  CVAR_ARCHIVE );
	g_autoBalance          = Cvar_Get( "g_autoBalance",          "0",  CVAR_ARCHIVE );
	Cvar_SetDescription( g_teamBalance, "Prevent joining teams that would cause imbalance (0 = off, 1 = on)." );
	Cvar_SetDescription( g_autoBalance, "Automatically move players to balance teams between rounds." );

	/* Gameplay */
	g_missilesThruTeleporters = Cvar_Get( "g_missilesThruTeleporters", "1",    CVAR_ARCHIVE );
	g_missilesThruTeleporters->flags &= ~(int)CVAR_SERVERINFO;
	g_selfDamage              = Cvar_Get( "g_selfDamage",              "1.0",  CVAR_ARCHIVE );
	g_weaponRespawn           = Cvar_Get( "g_weaponRespawn",           "5",    CVAR_ARCHIVE );
	g_armorProtection         = Cvar_Get( "g_armorProtection",         "0.66", CVAR_ARCHIVE );
	g_startHealth             = Cvar_Get( "g_startHealth",             "125",  CVAR_ARCHIVE );
	g_startArmor              = Cvar_Get( "g_startArmor",              "0",    CVAR_ARCHIVE );
	g_maxHealth               = Cvar_Get( "g_maxHealth",               "200",  CVAR_ARCHIVE );
	g_maxArmor                = Cvar_Get( "g_maxArmor",                "200",  CVAR_ARCHIVE );
	g_knockback               = Cvar_Get( "g_knockback",               "1.0",  CVAR_ARCHIVE );
	g_weaponDamageScale       = Cvar_Get( "g_weaponDamageScale",       "1.0",  CVAR_ARCHIVE );
	Cvar_SetDescription( g_missilesThruTeleporters, "Allow rockets/plasma to pass through teleporters." );
	Cvar_SetDescription( g_selfDamage, "Self-damage multiplier (0 = no self damage, 1 = full)." );
	Cvar_SetDescription( g_knockback, "Global knockback multiplier." );
	Cvar_SetDescription( g_weaponDamageScale, "Global weapon damage multiplier." );

	/* Warmup */
	g_warmupReady           = Cvar_Get( "g_warmupReady",           "1",    CVAR_ARCHIVE );
	g_warmupReadyPercentage = Cvar_Get( "g_warmupReadyPercentage", "0.5",  CVAR_ARCHIVE );
	g_warmupDelay           = Cvar_Get( "g_warmupDelay",           "5",    CVAR_ARCHIVE );
	Cvar_SetDescription( g_warmupReady, "Require /ready from players before match starts." );
	Cvar_SetDescription( g_warmupReadyPercentage, "Fraction of players that must be ready (0.0 - 1.0)." );

	/* Initialize unlagged state */
	Com_Memset( &unlagState, 0, sizeof( unlagState ) );
	unlagInitialized = qtrue;
	serverPaused = qfalse;

	Com_Printf( "Server enhancements: unlagged %s, %d server cvars registered\n",
		sv_unlagged->integer ? "enabled" : "disabled", 30 );
}

void SV_Enhanced_Shutdown( void ) {
	unlagInitialized = qfalse;
	serverPaused = qfalse;
}

/* ---- Unlagged: backward reconciliation ---- */

void SV_Unlagged_Record( int serverTime ) {
	int i, slot;

	if ( !unlagInitialized || !sv_unlagged || !sv_unlagged->integer ) {
		return;
	}

	slot = unlagState.historyHead % UNLAGGED_HISTORY_SIZE;

	for ( i = 0; i < sv.num_entities; i++ ) {
		sharedEntity_t *ent = SV_GentityNum( i );
		entityHistory_t *h = &unlagState.history[i][slot];

		if ( !ent || ent->r.svFlags & SVF_NOCLIENT ) {
			h->valid = qfalse;
			continue;
		}

		VectorCopy( ent->r.currentOrigin, h->origin );
		VectorCopy( ent->r.mins, h->mins );
		VectorCopy( ent->r.maxs, h->maxs );
		h->time = serverTime;
		h->valid = qtrue;
	}

	unlagState.historyHead++;
}

void SV_Unlagged_Rewind( int clientNum, int targetTime ) {
	int i, slot, bestSlot, bestDelta;

	if ( !unlagInitialized || !sv_unlagged || !sv_unlagged->integer ) {
		return;
	}

	(void)clientNum;

	if ( sv.time - targetTime > sv_unlaggedMaxRewind->integer ) {
		targetTime = sv.time - sv_unlaggedMaxRewind->integer;
	}

	for ( i = 0; i < sv.num_entities; i++ ) {
		sharedEntity_t *ent = SV_GentityNum( i );

		VectorCopy( ent->r.currentOrigin, savedOrigin[i] );
		VectorCopy( ent->r.mins, savedMins[i] );
		VectorCopy( ent->r.maxs, savedMaxs[i] );
		savedValid[i] = qtrue;

		bestSlot = -1;
		bestDelta = 0x7FFFFFFF;
		for ( slot = 0; slot < UNLAGGED_HISTORY_SIZE; slot++ ) {
			entityHistory_t *h = &unlagState.history[i][slot];
			int delta;
			if ( !h->valid ) continue;
			delta = abs( h->time - targetTime );
			if ( delta < bestDelta ) {
				bestDelta = delta;
				bestSlot = slot;
			}
		}

		if ( bestSlot >= 0 ) {
			entityHistory_t *h = &unlagState.history[i][bestSlot];
			VectorCopy( h->origin, ent->r.currentOrigin );
			VectorCopy( h->mins, ent->r.mins );
			VectorCopy( h->maxs, ent->r.maxs );
			SV_LinkEntity( ent );
		}
	}
}

void SV_Unlagged_Restore( void ) {
	int i;

	if ( !unlagInitialized || !sv_unlagged || !sv_unlagged->integer ) {
		return;
	}

	for ( i = 0; i < sv.num_entities; i++ ) {
		if ( !savedValid[i] ) continue;

		sharedEntity_t *ent = SV_GentityNum( i );
		VectorCopy( savedOrigin[i], ent->r.currentOrigin );
		VectorCopy( savedMins[i], ent->r.mins );
		VectorCopy( savedMaxs[i], ent->r.maxs );
		SV_LinkEntity( ent );

		savedValid[i] = qfalse;
	}
}

/* ---- Server Pause ---- */

void SV_Pause( void ) {
	if ( !sv_pauseEnabled || !sv_pauseEnabled->integer ) {
		Com_Printf( "Server pause is disabled (sv_pauseEnabled 0)\n" );
		return;
	}
	if ( serverPaused ) {
		Com_Printf( "Server is already paused\n" );
		return;
	}
	serverPaused = qtrue;
	Com_Printf( "Server PAUSED\n" );
}

void SV_Unpause( void ) {
	if ( !serverPaused ) {
		Com_Printf( "Server is not paused\n" );
		return;
	}
	serverPaused = qfalse;
	Com_Printf( "Server UNPAUSED\n" );
}

qboolean SV_IsPaused( void ) {
	return serverPaused;
}
