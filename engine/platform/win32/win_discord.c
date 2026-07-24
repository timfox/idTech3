#include "../client/core/cl_discord.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

struct discordConn_s {
	HANDLE pipe;
};

discordConn_t *Sys_DiscordConnect( void ) {
	int i;
	for ( i = 0; i < 10; i++ ) {
		char name[64];
		HANDLE h;
		DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;

		snprintf( name, sizeof( name ), "\\\\.\\pipe\\discord-ipc-%d", i );
		h = CreateFileA( name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, 0, NULL );
		if ( h == INVALID_HANDLE_VALUE ) {
			continue;
		}
		if ( !SetNamedPipeHandleState( h, &mode, NULL, NULL ) ) {
			CloseHandle( h );
			continue;
		}
		{
			discordConn_t *c = (discordConn_t *)calloc( 1, sizeof( *c ) );
			if ( !c ) {
				CloseHandle( h );
				return NULL;
			}
			c->pipe = h;
			return c;
		}
	}
	return NULL;
}

int Sys_DiscordRead( discordConn_t *c, void *buf, int len ) {
	DWORD avail = 0, got = 0;
	if ( !c ) {
		return -1;
	}
	if ( !PeekNamedPipe( c->pipe, NULL, 0, NULL, &avail, NULL ) ) {
		return -1;	// pipe broken
	}
	if ( avail == 0 ) {
		return 0;
	}
	if ( (DWORD)len < avail ) {
		avail = (DWORD)len;
	}
	if ( !ReadFile( c->pipe, buf, avail, &got, NULL ) ) {
		return -1;
	}
	return (int)got;
}

int Sys_DiscordWrite( discordConn_t *c, const void *buf, int len ) {
	DWORD wrote = 0;
	if ( !c ) {
		return -1;
	}
	/* Single WriteFile; a short write (wrote < len) is returned as-is so the
	   caller resets the connection — frames are tiny, so a short write means
	   the pipe is unhealthy. */
	if ( !WriteFile( c->pipe, buf, (DWORD)len, &wrote, NULL ) ) {
		return -1;
	}
	return (int)wrote;
}

void Sys_DiscordClose( discordConn_t *c ) {
	if ( c ) {
		CloseHandle( c->pipe );
		free( c );
	}
}
