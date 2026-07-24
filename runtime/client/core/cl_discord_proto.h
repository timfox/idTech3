#ifndef CL_DISCORD_PROTO_H
#define CL_DISCORD_PROTO_H

#define DISCORD_FIELD_SIZE 128

typedef struct {
	char	state[ DISCORD_FIELD_SIZE ];
	char	details[ DISCORD_FIELD_SIZE ];
	int		startTimestamp;
} discordActivity_t;

typedef struct {
	char	largeImage[ DISCORD_FIELD_SIZE ];
	char	largeText[ DISCORD_FIELD_SIZE ];
	char	button1Label[ DISCORD_FIELD_SIZE ];
	char	button1Url[ DISCORD_FIELD_SIZE ];
	char	button2Label[ DISCORD_FIELD_SIZE ];
	char	button2Url[ DISCORD_FIELD_SIZE ];
} discordPresenceOpts_t;

int Discord_JsonEscape( char *dst, int dstSize, const char *src );
int Discord_BuildHandshake( char *out, int outSize, const char *clientId );
int Discord_BuildSetActivity( char *out, int outSize, const discordActivity_t *act, int pid, int nonce,
	const discordPresenceOpts_t *opts );
int Discord_BuildClearActivity( char *out, int outSize, int pid, int nonce );

typedef enum {
	DISCORD_MENU,
	DISCORD_CONNECTING,
	DISCORD_LOADING,
	DISCORD_PLAYING,
	DISCORD_CINEMATIC,
	DISCORD_WATCHING_DEMO
} discordPhase_t;

const char *Discord_GametypeLabel( int gametype );
void Discord_MapActivity( discordActivity_t *out, discordPhase_t phase,
	const char *serverInfo, const char *mapMessage, int gametype, int nowSecs,
	const discordActivity_t *prev );
int Discord_ActivityEqual( const discordActivity_t *a, const discordActivity_t *b );

/* Search the first n bytes of buf for needle, scanning across embedded NUL
   bytes. Discord response frames begin with an 8-byte binary header that
   contains NULs, so a C-string scan stops before reaching the JSON body. */
int Discord_BufContains( const char *buf, int n, const char *needle );

#endif
