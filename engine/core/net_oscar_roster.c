/*
===========================================================================
In-memory OSCAR buddy roster (hybrid AIM stack Phase 1).
===========================================================================
*/

#include "net_oscar_roster.h"

#include <string.h>

void OSCAR_Roster_Init( oscarRoster_t *roster )
{
	if ( !roster ) {
		return;
	}
	Com_Memset( roster, 0, sizeof( *roster ) );
}

void OSCAR_Roster_Clear( oscarRoster_t *roster )
{
	if ( !roster ) {
		return;
	}
	roster->count = 0;
	Com_Memset( roster->buddies, 0, sizeof( roster->buddies ) );
	roster->generation++;
}

int OSCAR_Roster_Count( const oscarRoster_t *roster )
{
	return roster ? roster->count : 0;
}

qboolean OSCAR_Roster_Get( const oscarRoster_t *roster, int index, oscarBuddy_t *out )
{
	if ( !roster || !out || index < 0 || index >= roster->count ) {
		return qfalse;
	}
	*out = roster->buddies[index];
	return qtrue;
}

unsigned int OSCAR_Roster_Generation( const oscarRoster_t *roster )
{
	return roster ? roster->generation : 0u;
}

static int OSCAR_Roster_Find( const oscarRoster_t *roster, const char *screenName )
{
	int i;

	if ( !roster || !screenName || !screenName[0] ) {
		return -1;
	}
	for ( i = 0; i < roster->count; i++ ) {
		if ( !Q_stricmp( roster->buddies[i].screenName, screenName ) ) {
			return i;
		}
	}
	return -1;
}

int OSCAR_Roster_Ensure( oscarRoster_t *roster, const char *screenName )
{
	int idx;

	if ( !roster || !screenName || !screenName[0] ) {
		return -1;
	}
	idx = OSCAR_Roster_Find( roster, screenName );
	if ( idx >= 0 ) {
		return idx;
	}
	if ( roster->count >= OSCAR_MAX_BUDDIES ) {
		return -1;
	}
	idx = roster->count++;
	Com_Memset( &roster->buddies[idx], 0, sizeof( roster->buddies[idx] ) );
	Q_strncpyz( roster->buddies[idx].screenName, screenName, sizeof( roster->buddies[idx].screenName ) );
	Q_strncpyz( roster->buddies[idx].status, "offline", sizeof( roster->buddies[idx].status ) );
	roster->buddies[idx].online = qfalse;
	roster->generation++;
	return idx;
}

void OSCAR_Roster_ApplyPresence( oscarRoster_t *roster, const oscarEvent_t *ev )
{
	int idx;
	qboolean online;

	if ( !roster || !ev || !ev->screenName[0] ) {
		return;
	}
	idx = OSCAR_Roster_Ensure( roster, ev->screenName );
	if ( idx < 0 ) {
		return;
	}
	online = (qboolean)( Q_stricmp( ev->status, "offline" ) != 0 );
	Q_strncpyz( roster->buddies[idx].status,
		ev->status[0] ? ev->status : ( online ? "available" : "offline" ),
		sizeof( roster->buddies[idx].status ) );
	Q_strncpyz( roster->buddies[idx].awayMessage, ev->text, sizeof( roster->buddies[idx].awayMessage ) );
	roster->buddies[idx].online = online;
	roster->generation++;
}

void OSCAR_Roster_Remove( oscarRoster_t *roster, const char *screenName )
{
	int idx;
	int i;

	if ( !roster ) {
		return;
	}
	idx = OSCAR_Roster_Find( roster, screenName );
	if ( idx < 0 ) {
		return;
	}
	for ( i = idx; i < roster->count - 1; i++ ) {
		roster->buddies[i] = roster->buddies[i + 1];
	}
	roster->count--;
	roster->generation++;
}

int OSCAR_Roster_FormatSnapshot( const oscarRoster_t *roster, char *buf, int bufSize )
{
	int used = 0;
	int i;

	if ( !roster || !buf || bufSize <= 0 ) {
		return 0;
	}
	buf[0] = '\0';
	for ( i = 0; i < roster->count; i++ ) {
		char piece[MAX_NAME_LENGTH + 40];
		int pieceLen;

		Com_sprintf( piece, sizeof( piece ), "%s:%s%s",
			roster->buddies[i].screenName,
			roster->buddies[i].status[0] ? roster->buddies[i].status :
				( roster->buddies[i].online ? "online" : "offline" ),
			( i + 1 < roster->count ) ? ";" : "" );
		pieceLen = (int)strlen( piece );
		if ( used + pieceLen >= bufSize - 1 ) {
			break;
		}
		Com_Memcpy( buf + used, piece, pieceLen + 1 );
		used += pieceLen;
	}
	return used;
}
