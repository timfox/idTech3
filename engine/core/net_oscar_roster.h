#ifndef NET_OSCAR_ROSTER_H
#define NET_OSCAR_ROSTER_H

#include "net_oscar.h"

/*
 * In-memory buddy roster (session-scoped; no SSI/feedbag persistence).
 * Kept separate so unit tests can exercise presence → roster without sockets.
 */

typedef struct oscarRoster_s {
	oscarBuddy_t buddies[OSCAR_MAX_BUDDIES];
	int count;
	unsigned int generation;
} oscarRoster_t;

void OSCAR_Roster_Init( oscarRoster_t *roster );
void OSCAR_Roster_Clear( oscarRoster_t *roster );
int OSCAR_Roster_Count( const oscarRoster_t *roster );
qboolean OSCAR_Roster_Get( const oscarRoster_t *roster, int index, oscarBuddy_t *out );
unsigned int OSCAR_Roster_Generation( const oscarRoster_t *roster );
int OSCAR_Roster_Ensure( oscarRoster_t *roster, const char *screenName );
void OSCAR_Roster_ApplyPresence( oscarRoster_t *roster, const oscarEvent_t *ev );
void OSCAR_Roster_Remove( oscarRoster_t *roster, const char *screenName );
/* Formats "name:status;..." into buf. Returns bytes written (excluding NUL). */
int OSCAR_Roster_FormatSnapshot( const oscarRoster_t *roster, char *buf, int bufSize );

#endif /* NET_OSCAR_ROSTER_H */
