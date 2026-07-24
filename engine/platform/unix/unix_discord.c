#include "../client/core/cl_discord.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

struct discordConn_s {
	int fd;
};

/* Discord's socket lives directly under a runtime dir, or under the
   snap/flatpak subpaths on Linux. Try each base, suffixes 0..9. */
static int Discord_TryConnect( const char *base ) {
	int i;
	if ( !base || !base[0] ) {
		return -1;
	}
	for ( i = 0; i < 10; i++ ) {
		struct sockaddr_un addr;
		int fd;

		memset( &addr, 0, sizeof( addr ) );
		addr.sun_family = AF_UNIX;
		snprintf( addr.sun_path, sizeof( addr.sun_path ), "%s/discord-ipc-%d", base, i );

		fd = socket( AF_UNIX, SOCK_STREAM, 0 );
		if ( fd < 0 ) {
			return -1;
		}
		if ( connect( fd, (struct sockaddr *)&addr, sizeof( addr ) ) == 0 ) {
			if ( fcntl( fd, F_SETFL, O_NONBLOCK ) < 0 ) {
				close( fd );
				continue;
			}
			return fd;
		}
		close( fd );
	}
	return -1;
}

discordConn_t *Sys_DiscordConnect( void ) {
	const char *runtime = getenv( "XDG_RUNTIME_DIR" );
	const char *tmpdir = getenv( "TMPDIR" );
	char sub[1024];
	int fd = -1;
	const char *base;

	base = runtime ? runtime : ( tmpdir ? tmpdir : "/tmp" );

	fd = Discord_TryConnect( base );
	if ( fd < 0 ) {
		snprintf( sub, sizeof( sub ), "%s/snap.discord", base );
		fd = Discord_TryConnect( sub );
	}
	if ( fd < 0 ) {
		snprintf( sub, sizeof( sub ), "%s/app/com.discordapp.Discord", base );
		fd = Discord_TryConnect( sub );
	}
	if ( fd < 0 ) {
		return NULL;
	}
	{
		discordConn_t *c = (discordConn_t *)calloc( 1, sizeof( *c ) );
		if ( !c ) {
			close( fd );
			return NULL;
		}
		c->fd = fd;
		return c;
	}
}

int Sys_DiscordRead( discordConn_t *c, void *buf, int len ) {
	ssize_t n;
	if ( !c ) {
		return -1;
	}
	n = read( c->fd, buf, (size_t)len );
	if ( n == 0 ) {
		return -1;	/* peer closed */
	}
	if ( n < 0 ) {
		return ( errno == EAGAIN || errno == EWOULDBLOCK ) ? 0 : -1;
	}
	return (int)n;
}

int Sys_DiscordWrite( discordConn_t *c, const void *buf, int len ) {
	const char *p = (const char *)buf;
	int sent = 0;
	if ( !c ) {
		return -1;
	}
	while ( sent < len ) {
		ssize_t n = write( c->fd, p + sent, (size_t)( len - sent ) );
		if ( n < 0 ) {
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
				break;	/* send buffer full; report a short write so the caller resets */
			}
			return -1;
		}
		sent += (int)n;
	}
	return sent;
}

void Sys_DiscordClose( discordConn_t *c ) {
	if ( c ) {
		close( c->fd );
		free( c );
	}
}
