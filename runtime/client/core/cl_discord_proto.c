#include "cl_discord_proto.h"
#include <string.h>
#include <stdio.h>

int Discord_JsonEscape( char *dst, int dstSize, const char *src ) {
	int o = 0;
	if ( dstSize <= 0 ) {
		return 0;
	}
	while ( *src && o < dstSize - 1 ) {
		char c = *src++;
		if ( c == '"' || c == '\\' ) {
			if ( o >= dstSize - 2 ) {
				break;
			}
			dst[o++] = '\\';
		}
		dst[o++] = c;
	}
	dst[o] = '\0';
	return o;
}

/* Prepend the 8-byte little-endian frame header (opcode + body length)
   to a JSON body already sitting at out+8. Returns total frame size. */
static int Discord_Frame( char *out, int opcode, int bodyLen ) {
	out[0] = (char)( opcode         & 0xff );
	out[1] = (char)( ( opcode >> 8 ) & 0xff );
	out[2] = (char)( ( opcode >> 16 ) & 0xff );
	out[3] = (char)( ( opcode >> 24 ) & 0xff );
	out[4] = (char)( bodyLen         & 0xff );
	out[5] = (char)( ( bodyLen >> 8 ) & 0xff );
	out[6] = (char)( ( bodyLen >> 16 ) & 0xff );
	out[7] = (char)( ( bodyLen >> 24 ) & 0xff );
	return 8 + bodyLen;
}

int Discord_BuildHandshake( char *out, int outSize, const char *clientId ) {
	char esc[ DISCORD_FIELD_SIZE ];
	int body;

	Discord_JsonEscape( esc, sizeof( esc ), clientId );
	body = snprintf( out + 8, outSize - 8, "{\"v\":1,\"client_id\":\"%s\"}", esc );
	if ( body < 0 || body >= outSize - 8 ) {
		return -1;
	}
	return Discord_Frame( out, 0, body );
}

static int Discord_AppendButton( char *dst, int dstSize, int *fp, int *count,
	const char *label, const char *url ) {
	char escLabel[ DISCORD_FIELD_SIZE * 2 + 1 ];
	char escUrl[ DISCORD_FIELD_SIZE * 2 + 1 ];
	int w;

	if ( !label || !label[0] || !url || !url[0] ) {
		return 0;
	}
	Discord_JsonEscape( escLabel, sizeof( escLabel ), label );
	Discord_JsonEscape( escUrl, sizeof( escUrl ), url );
	w = snprintf( dst + *fp, dstSize - *fp, "%s{\"label\":\"%s\",\"url\":\"%s\"}",
		( *count > 0 ) ? "," : "", escLabel, escUrl );
	if ( w < 0 || w >= dstSize - *fp ) {
		return -1;
	}
	*fp += w;
	( *count )++;
	return 0;
}

int Discord_BuildSetActivity( char *out, int outSize, const discordActivity_t *act, int pid, int nonce,
	const discordPresenceOpts_t *opts ) {
	/* worst case: every source char escapes to two, plus NUL */
	char details[ DISCORD_FIELD_SIZE * 2 + 1 ];
	char state[ DISCORD_FIELD_SIZE * 2 + 1 ];
	char largeImage[ DISCORD_FIELD_SIZE * 2 + 1 ];
	char largeText[ DISCORD_FIELD_SIZE * 2 + 1 ];
	char fields[ DISCORD_FIELD_SIZE * 8 ];
	char buttons[ DISCORD_FIELD_SIZE * 8 ];
	int fp = 0, bp = 0, btnCount = 0, w, body;
	const char *img = "logo";
	const char *imgText = "Surf";

	Discord_JsonEscape( details, sizeof( details ), act->details );
	Discord_JsonEscape( state, sizeof( state ), act->state );

	if ( opts ) {
		if ( opts->largeImage[0] ) {
			img = opts->largeImage;
		}
		if ( opts->largeText[0] ) {
			imgText = opts->largeText;
		}
	}
	Discord_JsonEscape( largeImage, sizeof( largeImage ), img );
	Discord_JsonEscape( largeText, sizeof( largeText ), imgText );

	/* Emit only non-empty fields: Discord rejects the whole activity if its
	   state or details is an empty string ("not allowed to be empty"). Each
	   field keeps a trailing comma; "timestamps" always follows it. */
	fields[0] = '\0';
	if ( details[0] ) {
		w = snprintf( fields + fp, sizeof( fields ) - fp, "\"details\":\"%s\",", details );
		if ( w < 0 || w >= (int)( sizeof( fields ) - fp ) ) {
			return -1;
		}
		fp += w;
	}
	if ( state[0] ) {
		w = snprintf( fields + fp, sizeof( fields ) - fp, "\"state\":\"%s\",", state );
		if ( w < 0 || w >= (int)( sizeof( fields ) - fp ) ) {
			return -1;
		}
		fp += w;
	}

	buttons[0] = '\0';
	if ( opts ) {
		if ( Discord_AppendButton( buttons, sizeof( buttons ), &bp, &btnCount,
				opts->button1Label, opts->button1Url ) < 0 ) {
			return -1;
		}
		if ( Discord_AppendButton( buttons, sizeof( buttons ), &bp, &btnCount,
				opts->button2Label, opts->button2Url ) < 0 ) {
			return -1;
		}
	}

	if ( btnCount > 0 ) {
		body = snprintf( out + 8, outSize - 8,
			"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d,\"activity\":"
			"{%s\"timestamps\":{\"start\":%d},"
			"\"assets\":{\"large_image\":\"%s\",\"large_text\":\"%s\"},"
			"\"buttons\":[%s]}},"
			"\"nonce\":\"%d\"}",
			pid, fields, act->startTimestamp, largeImage, largeText, buttons, nonce );
	} else {
		body = snprintf( out + 8, outSize - 8,
			"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d,\"activity\":"
			"{%s\"timestamps\":{\"start\":%d},"
			"\"assets\":{\"large_image\":\"%s\",\"large_text\":\"%s\"}}},"
			"\"nonce\":\"%d\"}",
			pid, fields, act->startTimestamp, largeImage, largeText, nonce );
	}

	if ( body < 0 || body >= outSize - 8 ) {
		return -1;
	}
	return Discord_Frame( out, 1, body );
}

int Discord_BuildClearActivity( char *out, int outSize, int pid, int nonce ) {
	int body = snprintf( out + 8, outSize - 8,
		"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d,\"activity\":null},"
		"\"nonce\":\"%d\"}", pid, nonce );
	if ( body < 0 || body >= outSize - 8 ) {
		return -1;
	}
	return Discord_Frame( out, 1, body );
}

const char *Discord_GametypeLabel( int gametype ) {
	switch ( gametype ) {
		case 0: return "Free For All";
		case 1: return "Tournament";
		case 2: return "Single Player";
		case 3: return "Team Deathmatch";
		case 4: return "Capture the Flag";
		case 5: return "One Flag CTF";
		case 6: return "Obelisk";
		case 7: return "Harvester";
		default: return "Multiplayer";
	}
}

/* Read a value for `key` out of a Quake3 \\key\\value infostring into dst.
   Self-contained so this unit needs no engine headers. */
static void Discord_InfoValue( char *dst, int dstSize, const char *info, const char *key ) {
	int keyLen = (int)strlen( key );
	if ( dstSize <= 0 ) {
		return;
	}
	dst[0] = '\0';
	while ( *info == '\\' ) {
		const char *k = ++info;
		const char *v;
		int kl;
		while ( *info && *info != '\\' ) info++;
		kl = (int)( info - k );
		if ( *info != '\\' ) break;
		v = ++info;
		while ( *info && *info != '\\' ) info++;
		if ( kl == keyLen && strncmp( k, key, keyLen ) == 0 ) {
			int vl = (int)( info - v );
			if ( vl > dstSize - 1 ) vl = dstSize - 1;
			memcpy( dst, v, vl );
			dst[vl] = '\0';
			return;
		}
	}
}

/* Copy src into dst stripping Quake color codes (^ followed by a non-^ char)
   and any non-printable bytes, matching the engine's Q_CleanStr behaviour. */
static void Discord_CopyClean( char *dst, int dstSize, const char *src ) {
	int o = 0;
	if ( dstSize <= 0 ) {
		return;
	}
	while ( *src && o < dstSize - 1 ) {
		if ( src[0] == '^' && src[1] && src[1] != '^' ) {
			src += 2;
			continue;
		}
		if ( (unsigned char)*src >= 0x20 && (unsigned char)*src <= 0x7e ) {
			dst[o++] = *src;
		}
		src++;
	}
	dst[o] = '\0';
}

void Discord_MapActivity( discordActivity_t *out, discordPhase_t phase,
	const char *serverInfo, const char *mapMessage, int gametype, int nowSecs,
	const discordActivity_t *prev ) {
	int wantsMap = 0;

	memset( out, 0, sizeof( *out ) );

	switch ( phase ) {
		case DISCORD_MENU:
			strcpy( out->details, "In Menus" );
			break;
		case DISCORD_CONNECTING:
			strcpy( out->details, "Connecting" );
			break;
		case DISCORD_LOADING:
			strcpy( out->details, "Loading" );
			break;
		case DISCORD_CINEMATIC:
			strcpy( out->details, "Watching a cinematic" );
			break;
		case DISCORD_PLAYING:
			snprintf( out->details, sizeof( out->details ), "%s",
				Discord_GametypeLabel( gametype ) );
			wantsMap = 1;
			break;
		case DISCORD_WATCHING_DEMO:
			strcpy( out->details, "Watching a demo" );
			wantsMap = 1;
			break;
	}

	if ( wantsMap ) {
		/* prefer the worldspawn message (the map's display title), stripped of
		   color codes; fall back to the bsp name when unset */
		if ( mapMessage && mapMessage[0] ) {
			Discord_CopyClean( out->state, sizeof( out->state ), mapMessage );
		}
		if ( out->state[0] == '\0' ) {
			Discord_InfoValue( out->state, sizeof( out->state ), serverInfo, "mapname" );
		}
	}

	if ( prev && Discord_ActivityEqual( out, prev ) ) {
		out->startTimestamp = prev->startTimestamp;
	} else {
		out->startTimestamp = nowSecs;
	}
}

int Discord_ActivityEqual( const discordActivity_t *a, const discordActivity_t *b ) {
	return ( strcmp( a->details, b->details ) == 0 &&
	         strcmp( a->state, b->state ) == 0 ) ? 1 : 0;
}

int Discord_BufContains( const char *buf, int n, const char *needle ) {
	int nl = (int)strlen( needle );
	int i;
	if ( nl <= 0 || n < nl ) {
		return 0;
	}
	for ( i = 0; i + nl <= n; i++ ) {
		if ( memcmp( buf + i, needle, nl ) == 0 ) {
			return 1;
		}
	}
	return 0;
}
