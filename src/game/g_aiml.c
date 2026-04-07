/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

AIML engine implementation.

Parses AIML XML and builds an in-memory pattern database. User input
is matched against patterns using a linear scan with wildcard support.
Templates are processed to generate responses with variable substitution.

Supported AIML tags:
  <category>, <pattern>, <template>, <that>
  <random> + <li>   — random response selection
  <srai>            — redirect to another pattern (up to 8 deep)
  <star/>, <star index="N"/>  — wildcard match references
  <bot name="X"/>   — bot property lookup
  <get name="X"/>   — user variable lookup
  <set name="X">V</set>  — user variable assignment
  <think>...</think>     — silent processing (no output)
  <condition>       — conditional branching
  <uppercase/>, <lowercase/>  — text transformation
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_aiml.h"
#include <ctype.h>

/* ---- Data structures ---- */

typedef struct {
	char key[64];
	char value[256];
} aimlProp_t;

typedef struct {
	char pattern[AIML_MAX_PATTERN_LEN];
	char that[AIML_MAX_PATTERN_LEN];
	char templateText[AIML_MAX_RESPONSE_LEN];
} aimlCategory_t;

typedef struct {
	char userId[64];
	aimlProp_t vars[AIML_MAX_USER_VARS];
	int numVars;
	char lastResponse[AIML_MAX_RESPONSE_LEN];
} aimlUser_t;

typedef struct {
	char name[64];
	qboolean active;
	aimlCategory_t categories[AIML_MAX_CATEGORIES];
	int numCategories;
	aimlProp_t properties[AIML_MAX_PROPERTIES];
	int numProperties;
	aimlUser_t users[16];
	int numUsers;
} aimlBot_t;

static aimlBot_t bots[AIML_MAX_BOTS];
static int numBots = 0;
static qboolean aimlInitialized = qfalse;

#define VALID_BOT(h) ((h) >= 0 && (h) < numBots && bots[(h)].active)

/* ---- Helpers ---- */

static void AIML_UpperCase( char *s ) {
	while ( *s ) { *s = toupper( (unsigned char)*s ); s++; }
}

static void AIML_StripPunctuation( char *s ) {
	char *d = s;
	while ( *s ) {
		if ( isalnum( (unsigned char)*s ) || *s == ' ' || *s == '*' || *s == '_' ) {
			*d++ = *s;
		}
		s++;
	}
	*d = '\0';
}

static void AIML_Normalize( const char *input, char *out, int outSize ) {
	Q_strncpyz( out, input, outSize );
	AIML_UpperCase( out );
	AIML_StripPunctuation( out );

	/* Collapse multiple spaces */
	char *r = out, *w = out;
	qboolean lastSpace = qtrue;
	while ( *r ) {
		if ( *r == ' ' ) {
			if ( !lastSpace ) *w++ = ' ';
			lastSpace = qtrue;
		} else {
			*w++ = *r;
			lastSpace = qfalse;
		}
		r++;
	}
	if ( w > out && *( w - 1 ) == ' ' ) w--;
	*w = '\0';
}

/* ---- Pattern matching ---- */

static char matchStars[AIML_MAX_STAR][256];
static int  numMatchStars;

static qboolean AIML_PatternMatch( const char *pattern, const char *input ) {
	numMatchStars = 0;

	while ( *pattern && *input ) {
		if ( *pattern == '*' || *pattern == '_' ) {
			pattern++;
			while ( *pattern == ' ' ) pattern++;

			if ( !*pattern ) {
				if ( numMatchStars < AIML_MAX_STAR ) {
					Q_strncpyz( matchStars[numMatchStars++], input, sizeof( matchStars[0] ) );
				}
				return qtrue;
			}

			/* Try matching rest of pattern from each position */
			const char *bestMatch = NULL;
			int bestLen = 0;
			const char *p = input;

			while ( *p ) {
				if ( toupper( (unsigned char)*p ) == toupper( (unsigned char)*pattern ) ) {
					if ( AIML_PatternMatch( pattern, p ) ) {
						bestMatch = p;
						bestLen = (int)( p - input );
						break;
					}
				}
				p++;
			}

			if ( bestMatch ) {
				if ( numMatchStars < AIML_MAX_STAR && bestLen > 0 ) {
					int sl = bestLen;
					if ( sl >= (int)sizeof( matchStars[0] ) ) sl = (int)sizeof( matchStars[0] ) - 1;
					Com_Memcpy( matchStars[numMatchStars], input, sl );
					matchStars[numMatchStars][sl] = '\0';
					/* Trim trailing space */
					while ( sl > 0 && matchStars[numMatchStars][sl-1] == ' ' ) {
						matchStars[numMatchStars][--sl] = '\0';
					}
					numMatchStars++;
				}
				return qtrue;
			}
			return qfalse;
		}

		if ( toupper( (unsigned char)*pattern ) != toupper( (unsigned char)*input ) ) {
			return qfalse;
		}
		pattern++;
		input++;
	}

	while ( *pattern == ' ' ) pattern++;
	while ( *input == ' ' ) input++;

	return ( !*pattern && !*input );
}

/* ---- XML parsing (minimal, tag-level) ---- */

static const char *AIML_FindTag( const char *xml, const char *tag, const char **contentStart, const char **contentEnd ) {
	char openTag[64], closeTag[64];
	const char *start, *end;

	Com_sprintf( openTag, sizeof( openTag ), "<%s", tag );
	Com_sprintf( closeTag, sizeof( closeTag ), "</%s>", tag );

	start = strstr( xml, openTag );
	if ( !start ) return NULL;

	start = strchr( start, '>' );
	if ( !start ) return NULL;
	start++;

	end = strstr( start, closeTag );
	if ( !end ) return NULL;

	*contentStart = start;
	*contentEnd = end;
	return end + strlen( closeTag );
}

static const char *AIML_GetAttr( const char *tag, const char *attr, char *out, int outSize ) {
	char search[64];
	const char *p, *q;

	Com_sprintf( search, sizeof( search ), "%s=\"", attr );
	p = strstr( tag, search );
	if ( !p ) return NULL;
	p += strlen( search );
	q = strchr( p, '"' );
	if ( !q ) return NULL;

	int len = (int)( q - p );
	if ( len >= outSize ) len = outSize - 1;
	Com_Memcpy( out, p, len );
	out[len] = '\0';
	return q + 1;
}

/* ---- Template processing ---- */

static aimlUser_t *AIML_FindUser( aimlBot_t *bot, const char *userId, qboolean create ) {
	int i;
	for ( i = 0; i < bot->numUsers; i++ ) {
		if ( Q_stricmp( bot->users[i].userId, userId ) == 0 ) return &bot->users[i];
	}
	if ( !create || bot->numUsers >= 16 ) return NULL;
	aimlUser_t *u = &bot->users[bot->numUsers++];
	Com_Memset( u, 0, sizeof( *u ) );
	Q_strncpyz( u->userId, userId, sizeof( u->userId ) );
	return u;
}

static void AIML_ProcessTemplate( aimlBot_t *bot, const char *userId, const char *tmpl,
	char *out, int outSize, int sraiDepth );

static void AIML_ProcessTemplate( aimlBot_t *bot, const char *userId, const char *tmpl,
	char *out, int outSize, int sraiDepth ) {
	const char *p = tmpl;
	int written = 0;

	while ( *p && written < outSize - 1 ) {
		if ( *p == '<' ) {
			/* <star/> or <star index="N"/> */
			if ( !Q_strncmp( p, "<star", 5 ) ) {
				int idx = 0;
				const char *end = strchr( p, '>' );
				if ( !end ) { p++; continue; }
				char attrBuf[16];
				if ( AIML_GetAttr( p, "index", attrBuf, sizeof( attrBuf ) ) ) {
					idx = atoi( attrBuf ) - 1;
				}
				if ( idx >= 0 && idx < numMatchStars ) {
					int sl = (int)strlen( matchStars[idx] );
					if ( written + sl < outSize - 1 ) {
						Com_Memcpy( out + written, matchStars[idx], sl );
						written += sl;
					}
				}
				p = end + 1;
				continue;
			}

			/* <bot name="X"/> */
			if ( !Q_strncmp( p, "<bot ", 5 ) ) {
				const char *end = strchr( p, '>' );
				if ( end ) {
					char nameBuf[64];
					if ( AIML_GetAttr( p, "name", nameBuf, sizeof( nameBuf ) ) ) {
						const char *val = AIML_GetBotProperty( (int)( bot - bots ), nameBuf );
						if ( val ) {
							int sl = (int)strlen( val );
							if ( written + sl < outSize - 1 ) {
								Com_Memcpy( out + written, val, sl );
								written += sl;
							}
						}
					}
					p = end + 1;
					continue;
				}
			}

			/* <get name="X"/> */
			if ( !Q_strncmp( p, "<get ", 5 ) ) {
				const char *end = strchr( p, '>' );
				if ( end ) {
					char nameBuf[64];
					if ( AIML_GetAttr( p, "name", nameBuf, sizeof( nameBuf ) ) ) {
						const char *val = AIML_GetUserVar( (int)( bot - bots ), userId, nameBuf );
						if ( val && val[0] ) {
							int sl = (int)strlen( val );
							if ( written + sl < outSize - 1 ) {
								Com_Memcpy( out + written, val, sl );
								written += sl;
							}
						}
					}
					p = end + 1;
					continue;
				}
			}

			/* <set name="X">value</set> */
			if ( !Q_strncmp( p, "<set ", 5 ) ) {
				char nameBuf[64];
				const char *cs, *ce;
				if ( AIML_GetAttr( p, "name", nameBuf, sizeof( nameBuf ) ) && AIML_FindTag( p, "set", &cs, &ce ) ) {
					char valBuf[256];
					int vl = (int)( ce - cs );
					if ( vl >= (int)sizeof( valBuf ) ) vl = (int)sizeof( valBuf ) - 1;
					Com_Memcpy( valBuf, cs, vl );
					valBuf[vl] = '\0';
					AIML_SetUserVar( (int)( bot - bots ), userId, nameBuf, valBuf );
					/* Also output the value */
					if ( written + vl < outSize - 1 ) {
						Com_Memcpy( out + written, valBuf, vl );
						written += vl;
					}
					p = ce + 6; /* skip </set> */
					continue;
				}
			}

			/* <think>...</think> — silent processing */
			if ( !Q_strncmp( p, "<think>", 7 ) ) {
				const char *cs, *ce;
				if ( AIML_FindTag( p, "think", &cs, &ce ) ) {
					char dummy[AIML_MAX_RESPONSE_LEN];
					char inner[AIML_MAX_RESPONSE_LEN];
					int il = (int)( ce - cs );
					if ( il >= (int)sizeof( inner ) ) il = (int)sizeof( inner ) - 1;
					Com_Memcpy( inner, cs, il );
					inner[il] = '\0';
					AIML_ProcessTemplate( bot, userId, inner, dummy, sizeof( dummy ), sraiDepth );
					p = ce + 8; /* skip </think> */
					continue;
				}
			}

			/* <random><li>A</li><li>B</li></random> */
			if ( !Q_strncmp( p, "<random>", 8 ) ) {
				const char *cs, *ce;
				if ( AIML_FindTag( p, "random", &cs, &ce ) ) {
					char items[AIML_MAX_RANDOM_ITEMS][256];
					int itemCount = 0;
					const char *scan = cs;
					while ( scan < ce && itemCount < AIML_MAX_RANDOM_ITEMS ) {
						const char *lis, *lie;
						const char *next = AIML_FindTag( scan, "li", &lis, &lie );
						if ( !next || lis >= ce ) break;
						int ll = (int)( lie - lis );
						if ( ll >= (int)sizeof( items[0] ) ) ll = (int)sizeof( items[0] ) - 1;
						Com_Memcpy( items[itemCount], lis, ll );
						items[itemCount][ll] = '\0';
						itemCount++;
						scan = next;
					}
					if ( itemCount > 0 ) {
						int pick = rand() % itemCount;
						char processed[AIML_MAX_RESPONSE_LEN];
						AIML_ProcessTemplate( bot, userId, items[pick], processed, sizeof( processed ), sraiDepth );
						int sl = (int)strlen( processed );
						if ( written + sl < outSize - 1 ) {
							Com_Memcpy( out + written, processed, sl );
							written += sl;
						}
					}
					p = ce + 9; /* skip </random> */
					continue;
				}
			}

			/* <srai>pattern</srai> — recursive lookup */
			if ( !Q_strncmp( p, "<srai>", 6 ) ) {
				const char *cs, *ce;
				if ( AIML_FindTag( p, "srai", &cs, &ce ) && sraiDepth < AIML_MAX_SRAI_DEPTH ) {
					char sraiInput[AIML_MAX_PATTERN_LEN];
					int sl = (int)( ce - cs );
					if ( sl >= (int)sizeof( sraiInput ) ) sl = (int)sizeof( sraiInput ) - 1;
					Com_Memcpy( sraiInput, cs, sl );
					sraiInput[sl] = '\0';

					/* Process any tags inside the srai content first */
					char processedInput[AIML_MAX_PATTERN_LEN];
					AIML_ProcessTemplate( bot, userId, sraiInput, processedInput, sizeof( processedInput ), sraiDepth + 1 );

					const char *response = AIML_GetResponse( (int)( bot - bots ), userId, processedInput );
					if ( response && response[0] ) {
						int rl = (int)strlen( response );
						if ( written + rl < outSize - 1 ) {
							Com_Memcpy( out + written, response, rl );
							written += rl;
						}
					}
					p = ce + 7; /* skip </srai> */
					continue;
				}
			}

			/* Skip unknown tags */
			const char *end = strchr( p + 1, '>' );
			if ( end ) { p = end + 1; continue; }
		}

		out[written++] = *p++;
	}
	out[written] = '\0';
}

/* ---- Public API ---- */

void AIML_Init( void ) {
	Com_Memset( bots, 0, sizeof( bots ) );
	numBots = 0;
	aimlInitialized = qtrue;
	Com_Printf( "AIML engine initialized\n" );
}

void AIML_Shutdown( void ) {
	numBots = 0;
	aimlInitialized = qfalse;
}

aimlBotHandle_t AIML_CreateBot( const char *name ) {
	if ( numBots >= AIML_MAX_BOTS ) return -1;
	int idx = numBots++;
	Com_Memset( &bots[idx], 0, sizeof( aimlBot_t ) );
	Q_strncpyz( bots[idx].name, name, sizeof( bots[idx].name ) );
	bots[idx].active = qtrue;

	AIML_SetBotProperty( idx, "name", name );
	AIML_SetBotProperty( idx, "version", "2.1" );

	Com_Printf( "AIML: created bot '%s' (handle %d)\n", name, idx );
	return idx;
}

void AIML_DestroyBot( aimlBotHandle_t h ) {
	if ( VALID_BOT( h ) ) bots[h].active = qfalse;
}

void AIML_SetBotProperty( aimlBotHandle_t h, const char *key, const char *value ) {
	int i;
	if ( !VALID_BOT( h ) ) return;
	for ( i = 0; i < bots[h].numProperties; i++ ) {
		if ( Q_stricmp( bots[h].properties[i].key, key ) == 0 ) {
			Q_strncpyz( bots[h].properties[i].value, value, sizeof( bots[h].properties[i].value ) );
			return;
		}
	}
	if ( bots[h].numProperties < AIML_MAX_PROPERTIES ) {
		aimlProp_t *p = &bots[h].properties[bots[h].numProperties++];
		Q_strncpyz( p->key, key, sizeof( p->key ) );
		Q_strncpyz( p->value, value, sizeof( p->value ) );
	}
}

const char *AIML_GetBotProperty( aimlBotHandle_t h, const char *key ) {
	int i;
	if ( !VALID_BOT( h ) ) return "";
	for ( i = 0; i < bots[h].numProperties; i++ ) {
		if ( Q_stricmp( bots[h].properties[i].key, key ) == 0 )
			return bots[h].properties[i].value;
	}
	return "";
}

qboolean AIML_LoadFile( aimlBotHandle_t h, const char *filename ) {
	void *buf;
	int len;
	const char *p;

	if ( !VALID_BOT( h ) ) return qfalse;

	len = FS_ReadFile( filename, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( S_COLOR_YELLOW "AIML: could not load %s\n", filename );
		return qfalse;
	}

	p = (const char *)buf;
	while ( p && *p ) {
		const char *catStart, *catEnd;
		const char *next = AIML_FindTag( p, "category", &catStart, &catEnd );
		if ( !next ) break;

		if ( bots[h].numCategories >= AIML_MAX_CATEGORIES ) {
			Com_Printf( S_COLOR_YELLOW "AIML: max categories reached (%d)\n", AIML_MAX_CATEGORIES );
			break;
		}

		aimlCategory_t *cat = &bots[h].categories[bots[h].numCategories];
		Com_Memset( cat, 0, sizeof( *cat ) );

		const char *ps, *pe;
		if ( AIML_FindTag( catStart, "pattern", &ps, &pe ) ) {
			int pl = (int)( pe - ps );
			if ( pl >= AIML_MAX_PATTERN_LEN ) pl = AIML_MAX_PATTERN_LEN - 1;
			Com_Memcpy( cat->pattern, ps, pl );
			cat->pattern[pl] = '\0';
			AIML_UpperCase( cat->pattern );
		}

		const char *ts, *te;
		if ( AIML_FindTag( catStart, "template", &ts, &te ) ) {
			int tl = (int)( te - ts );
			if ( tl >= AIML_MAX_RESPONSE_LEN ) tl = AIML_MAX_RESPONSE_LEN - 1;
			Com_Memcpy( cat->templateText, ts, tl );
			cat->templateText[tl] = '\0';
		}

		const char *ths, *the;
		if ( AIML_FindTag( catStart, "that", &ths, &the ) ) {
			int thl = (int)( the - ths );
			if ( thl >= AIML_MAX_PATTERN_LEN ) thl = AIML_MAX_PATTERN_LEN - 1;
			Com_Memcpy( cat->that, ths, thl );
			cat->that[thl] = '\0';
			AIML_UpperCase( cat->that );
		}

		if ( cat->pattern[0] && cat->templateText[0] ) {
			bots[h].numCategories++;
		}

		p = next;
	}

	FS_FreeFile( buf );
	Com_Printf( "AIML: loaded %s (%d categories for bot '%s')\n",
		filename, bots[h].numCategories, bots[h].name );
	return qtrue;
}

int AIML_GetCategoryCount( aimlBotHandle_t h ) {
	return VALID_BOT( h ) ? bots[h].numCategories : 0;
}

const char *AIML_GetResponse( aimlBotHandle_t h, const char *userId, const char *input ) {
	static char response[AIML_MAX_RESPONSE_LEN];
	char normalized[AIML_MAX_PATTERN_LEN];
	int i, bestMatch = -1, bestScore = -1;
	aimlBot_t *bot;
	aimlUser_t *user;

	if ( !VALID_BOT( h ) ) return "";

	bot = &bots[h];
	user = AIML_FindUser( bot, userId, qtrue );

	AIML_Normalize( input, normalized, sizeof( normalized ) );

	/* Priority: exact match > _ wildcard > * wildcard */
	for ( i = 0; i < bot->numCategories; i++ ) {
		aimlCategory_t *cat = &bot->categories[i];
		int score = 0;

		/* Check <that> context if specified */
		if ( cat->that[0] && user ) {
			char normThat[AIML_MAX_PATTERN_LEN];
			AIML_Normalize( user->lastResponse, normThat, sizeof( normThat ) );
			if ( !AIML_PatternMatch( cat->that, normThat ) ) continue;
			score += 100;
		}

		if ( AIML_PatternMatch( cat->pattern, normalized ) ) {
			/* Score: exact chars are worth more than wildcards */
			int patLen = (int)strlen( cat->pattern );
			int wildcards = 0;
			const char *pp = cat->pattern;
			while ( *pp ) { if ( *pp == '*' || *pp == '_' ) wildcards++; pp++; }
			score += patLen - wildcards * 2;
			if ( cat->pattern[0] == '_' ) score += 50;

			if ( score > bestScore ) {
				bestScore = score;
				bestMatch = i;
			}
		}
	}

	if ( bestMatch < 0 ) {
		response[0] = '\0';
		return response;
	}

	/* Re-match to populate stars */
	AIML_PatternMatch( bot->categories[bestMatch].pattern, normalized );

	AIML_ProcessTemplate( bot, userId, bot->categories[bestMatch].templateText,
		response, sizeof( response ), 0 );

	if ( user ) {
		Q_strncpyz( user->lastResponse, response, sizeof( user->lastResponse ) );
	}

	return response;
}

void AIML_SetUserVar( aimlBotHandle_t h, const char *userId, const char *key, const char *value ) {
	int i;
	aimlUser_t *u;
	if ( !VALID_BOT( h ) ) return;
	u = AIML_FindUser( &bots[h], userId, qtrue );
	if ( !u ) return;
	for ( i = 0; i < u->numVars; i++ ) {
		if ( Q_stricmp( u->vars[i].key, key ) == 0 ) {
			Q_strncpyz( u->vars[i].value, value, sizeof( u->vars[i].value ) );
			return;
		}
	}
	if ( u->numVars < AIML_MAX_USER_VARS ) {
		Q_strncpyz( u->vars[u->numVars].key, key, sizeof( u->vars[0].key ) );
		Q_strncpyz( u->vars[u->numVars].value, value, sizeof( u->vars[0].value ) );
		u->numVars++;
	}
}

const char *AIML_GetUserVar( aimlBotHandle_t h, const char *userId, const char *key ) {
	int i;
	aimlUser_t *u;
	if ( !VALID_BOT( h ) ) return "";
	u = AIML_FindUser( &bots[h], userId, qfalse );
	if ( !u ) return "";
	for ( i = 0; i < u->numVars; i++ ) {
		if ( Q_stricmp( u->vars[i].key, key ) == 0 ) return u->vars[i].value;
	}
	return "";
}

void AIML_ResetUser( aimlBotHandle_t h, const char *userId ) {
	aimlUser_t *u;
	if ( !VALID_BOT( h ) ) return;
	u = AIML_FindUser( &bots[h], userId, qfalse );
	if ( u ) {
		u->numVars = 0;
		u->lastResponse[0] = '\0';
	}
}
