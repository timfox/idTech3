/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

AIML 3.0 Core-oriented interpreter (draft-spec aligned, in progress).

Implements: tokenized normalization (loose default), * / _ pattern tokens,
<that> conditioning, <topic> blocks, <intent> alias, JSON category packs,
<condition>, <srai> with stack save/restore, <star/> and <thatstar/>,
<think>, <get>/<set> (set emits empty in Core 3.0), <random>,
g_aiml3 and g_aimlJson cvars.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_aiml.h"
#include "g_eda.h"
#include <ctype.h>
#include <string.h>

/* Public entry points used before definition (template -> SetUserVar/GetUserVar/GetBotProperty) */
const char *AIML_GetBotProperty( aimlBotHandle_t h, const char *key );
void    AIML_SetBotProperty( aimlBotHandle_t h, const char *key, const char *value );
void    AIML_SetUserVar( aimlBotHandle_t h, const char *userId, const char *key, const char *value );
const char *AIML_GetUserVar( aimlBotHandle_t h, const char *userId, const char *key );

/* ---- Internals ---- */

#define AIML_MAX_LINE_TOKENS	48
#define AIML_MAX_PAT_TOKS		32
#define AIML_TOKW				32

#define PT_LIT		0
#define PT_STAR		1
#define PT_UNDERSC	2

typedef struct {
	char key[64];
	char value[256];
} aimlProp_t;

typedef struct {
	char pattern[AIML_MAX_PATTERN_LEN];
	char that[AIML_MAX_PATTERN_LEN];
	char topic[64];
	char templateText[AIML_MAX_RESPONSE_LEN];
	int nPat, nThat, nTopic;
	int patKind[AIML_MAX_PAT_TOKS];
	int thatKind[AIML_MAX_PAT_TOKS];
	int topicKind[AIML_MAX_PAT_TOKS];
	char patTok[AIML_MAX_PAT_TOKS][AIML_TOKW];
	char thatTok[AIML_MAX_PAT_TOKS][AIML_TOKW];
	char topicTok[AIML_MAX_PAT_TOKS][AIML_TOKW];
} aimlCategory_t;

typedef struct {
	char userId[64];
	aimlProp_t vars[AIML_MAX_USER_VARS];
	int numVars;
	char lastResponse[AIML_MAX_RESPONSE_LEN];
	char topic[64];
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

static aimlBot_t		bots[AIML_MAX_BOTS];
static int			numBots;
static cvar_t		*g_aiml3;
static cvar_t		*g_aimlJson;
static cvar_t		*g_aimlDebug;

/* Match-time captures: input and <that> patterns */
static char		in_stars[AIML_MAX_STAR][256];
static int		in_nStars;
static char		that_stars[AIML_MAX_STAR][256];
static int		that_nStars;
static char		topic_stars[AIML_MAX_STAR][256];
static int		topic_nStars;

/* Active buffer for TryMatch */
static char		(*g_starBuf)[256];
static int		*g_starCount;

#define VALID_BOT( h ) ( (h) >= 0 && (h) < numBots && bots[(h)].active )

/* ---- Case / normalize (Core default, ASCII) ---- */

static void AIML_AsciiUpper( char *s ) {
	while ( *s ) {
		*s = (char)toupper( (unsigned char)*s );
		s++;
	}
}

static void AIML_CleanToken( const char *in, char *out, int olen ) {
	int w = 0;
	while ( *in && w < olen - 1 ) {
		if ( isalnum( (unsigned char)*in ) ) {
			out[w++] = (char)toupper( (unsigned char)*in );
		}
		in++;
	}
	out[w] = '\0';
}

static void AIML_CoreNormalizeLine( const char *input, char *out, int outSize ) {
	char wk[1024];
	char *r, *wp;
	int lastsp;

	Q_strncpyz( wk, input, sizeof( wk ) );
	AIML_AsciiUpper( wk );
	r = wk;
	wp = wk;
	lastsp = 1;
	while ( *r ) {
		if ( *r == ' ' || *r == '\t' || *r == '\n' || *r == '\r' ) {
			if ( !lastsp ) {
				*wp++ = ' ';
			}
			lastsp = 1;
		} else if ( isalnum( (unsigned char)*r ) ) {
			*wp++ = *r;
			lastsp = 0;
		}
		r++;
	}
	*wp = '\0';
	if ( wp > wk && wp[-1] == ' ' ) {
		*--wp = '\0';
	}
	while ( wk[0] == ' ' ) {
		memmove( wk, wk + 1, strlen( wk ) + 1 );
	}
	{
		int n = (int)strlen( wk );
		while ( n > 0 && wk[n - 1] == ' ' ) {
			wk[--n] = '\0';
		}
	}
	Q_strncpyz( out, wk, outSize );
}

static int AIML_SplitTokens( const char *line, char t[AIML_MAX_LINE_TOKENS][AIML_TOKW] ) {
	int n;
	const char *p, *start;
	char c[AIML_TOKW];

	n = 0;
	p = line;
	while ( *p && n < AIML_MAX_LINE_TOKENS ) {
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		start = p;
		while ( *p && *p != ' ' ) p++;
		{
			int L = (int)( p - start );
			if ( L >= (int)sizeof( c ) ) L = (int)sizeof( c ) - 1;
			Com_Memcpy( c, start, L );
			c[L] = '\0';
		}
		AIML_CleanToken( c, t[n], AIML_TOKW );
		if ( t[n][0] ) n++;
	}
	return n;
}

static void AIML_CompilePat( const char *raw, int *nO, int kind[AIML_MAX_PAT_TOKS], char tb[AIML_MAX_PAT_TOKS][AIML_TOKW] ) {
	char m[AIML_MAX_PATTERN_LEN];
	const char *p;
	char w[AIML_TOKW];
	int n, L;

	Q_strncpyz( m, raw, sizeof( m ) );
	AIML_AsciiUpper( m );
	{
		char *r = m, *o = m;
		while ( *r ) {
			if ( isalnum( (unsigned char)*r ) || *r == ' ' || *r == '*' || *r == '_' ) {
				*o++ = *r;
			}
			r++;
		}
		*o = '\0';
	}
	n = 0;
	p = m;
	while ( *p && n < AIML_MAX_PAT_TOKS ) {
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		if ( *p == '*' || *p == '_' ) {
			kind[n] = ( *p == '*' ) ? PT_STAR : PT_UNDERSC;
			tb[n][0] = '\0';
			n++;
			p++;
			continue;
		}
		{
			const char *s = p;
			while ( *p && *p != ' ' ) p++;
			L = (int)( p - s );
			if ( L >= AIML_TOKW ) L = AIML_TOKW - 1;
			Com_Memcpy( w, s, L );
			w[L] = '\0';
		}
		AIML_CleanToken( w, tb[n], AIML_TOKW );
		kind[n] = PT_LIT;
		n++;
	}
	*nO = n;
}

/* Recursive backtracking match; stars go to g_starBuf[ *g_starCount ] */
static qboolean AIML_TryMatch( int pi, int pN, int *kinds, char *ptb[AIML_MAX_PAT_TOKS], int canHaveZeroStar,
		int ui, int uN, char ut[AIML_MAX_LINE_TOKENS][AIML_TOKW] ) {
	int k, a, cpos, sidx;
	char cap[256];
	int L;

	(void)canHaveZeroStar;

	if ( pi == pN ) {
		return ( ui == uN ) ? qtrue : qfalse;
	}
	if ( kinds[pi] == PT_STAR ) {
		for ( k = ui; k <= uN; k++ ) {
			cpos = 0;
			cap[0] = '\0';
			for ( a = ui; a < k; a++ ) {
				if ( a > ui && cpos < (int)sizeof( cap ) - 1 ) {
					cap[cpos++] = ' ';
				}
				L = (int)strlen( ut[a] );
				if ( cpos + L >= (int)sizeof( cap ) ) {
					L = (int)sizeof( cap ) - 1 - cpos;
				}
				if ( L > 0 ) {
					Com_Memcpy( cap + cpos, ut[a], L );
					cpos += L;
				}
			}
			cap[cpos] = '\0';
			sidx = *g_starCount;
			if ( *g_starCount < AIML_MAX_STAR ) {
				Q_strncpyz( g_starBuf[(*g_starCount)++], cap, 256 );
			}
			if ( AIML_TryMatch( pi + 1, pN, kinds, ptb, 1, k, uN, ut ) ) {
				return qtrue;
			}
			*g_starCount = sidx;
		}
		return qfalse;
	}
	if ( kinds[pi] == PT_UNDERSC ) {
		if ( ui >= uN ) {
			return qfalse;
		}
		sidx = *g_starCount;
		if ( *g_starCount < AIML_MAX_STAR ) {
			Q_strncpyz( g_starBuf[(*g_starCount)++], ut[ui], 256 );
		}
		if ( AIML_TryMatch( pi + 1, pN, kinds, ptb, 0, ui + 1, uN, ut ) ) {
			return qtrue;
		}
		*g_starCount = sidx;
		return qfalse;
	}
	if ( ui >= uN ) {
		return qfalse;
	}
	if ( Q_stricmp( ptb[pi], ut[ui] ) != 0 ) {
		return qfalse;
	}
	return AIML_TryMatch( pi + 1, pN, kinds, ptb, 0, ui + 1, uN, ut );
}

static qboolean AIML_MatchExpr( int nP, int *kinds, char ptk[AIML_MAX_PAT_TOKS][AIML_TOKW], int nU, char ut[AIML_MAX_LINE_TOKENS][AIML_TOKW], char sbuf[AIML_MAX_STAR][256], int *nS ) {
	/* use pointer array for char[][TOKW] */
	char *pp[AIML_MAX_PAT_TOKS];
	int j;
	for ( j = 0; j < nP; j++ ) {
		pp[j] = ptk[j];
	}
	g_starBuf = sbuf;
	g_starCount = nS;
	*nS = 0;
	return AIML_TryMatch( 0, nP, kinds, pp, 0, 0, nU, ut );
}

static int AIML_SpecScore( aimlCategory_t *c ) {
	int s = 0, i;
	for ( i = 0; i < c->nPat; i++ ) {
		if ( c->patKind[i] == PT_LIT ) {
			s += 2;
		} else if ( c->patKind[i] == PT_UNDERSC ) {
			s += 1;
		}
	}
	/* Slight boost when category pins <that> / <topic> patterns (more specific). */
	if ( c->nThat > 0 ) { s += 1; }
	if ( c->nTopic > 0 ) { s += 1; }
	return s;
}

/* ---- XML / JSON ---- */

static const char *AIML_FindTag( const char *xml, const char *tag, const char **a, const char **b ) {
	char o[64], c[64];
	const char *s, *e;

	Com_sprintf( o, sizeof( o ), "<%s", tag );
	Com_sprintf( c, sizeof( c ), "</%s>", tag );
	s = strstr( xml, o );
	if ( !s ) {
		return NULL;
	}
	s = strchr( s, '>' );
	if ( !s ) {
		return NULL;
	}
	s++;
	e = strstr( s, c );
	if ( !e ) {
		return NULL;
	}
	*a = s;
	*b = e;
	return e + strlen( c );
}

static const char *AIML_GetAttr( const char *topen, const char *attr, char *out, int olen ) {
	char q[64];
	const char *p, *e2;
	Com_sprintf( q, sizeof( q ), "%s=\"", attr );
	p = strstr( topen, q );
	if ( !p ) {
		return NULL;
	}
	p += strlen( q );
	e2 = strchr( p, '"' );
	if ( !e2 ) {
		return NULL;
	}
	{
		int n = (int)( e2 - p );
		if ( n >= olen ) n = olen - 1;
		Com_Memcpy( out, p, n );
		out[n] = '\0';
		return e2 + 1;
	}
}

static int AIML_JsonOneString( const char *obj, const char *key, char *out, int olen ) {
	char pfx[48];
	const char *p, *c;
	Com_sprintf( pfx, sizeof( pfx ), "\"%s\"", key );
	p = strstr( obj, pfx );
	if ( !p ) {
		return 0;
	}
	p = strchr( p, ':' );
	if ( !p ) {
		return 0;
	}
	p++;
	while ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) p++;
	if ( *p != '"' ) {
		return 0;
	}
	p++;
	c = p;
	while ( *c && *c != '"' ) {
		if ( *c == '\\' && c[1] ) c += 2; else c++;
	}
	{
		int n = (int)( c - p );
		if ( n >= olen ) n = olen - 1;
		Com_Memcpy( out, p, n );
		out[n] = '\0';
	}
	return 1;
}

/* ---- User / bot state ---- */

static aimlUser_t *AIML_FindUser( aimlBot_t *bot, const char *userId, qboolean create ) {
	int i;
	for ( i = 0; i < bot->numUsers; i++ ) {
		if ( !Q_stricmp( bot->users[i].userId, userId ) ) {
			return &bot->users[i];
		}
	}
	if ( !create || bot->numUsers >= 16 ) {
		return NULL;
	}
	Com_Memset( &bot->users[bot->numUsers], 0, sizeof( bot->users[0] ) );
	Q_strncpyz( bot->users[bot->numUsers].userId, userId, sizeof( bot->users[0].userId ) );
	return &bot->users[bot->numUsers++];
}

static void AIML_SyncTopic( aimlUser_t *u ) {
	int i;
	for ( i = 0; i < u->numVars; i++ ) {
		if ( !Q_stricmp( u->vars[i].key, "topic" ) ) {
			AIML_CoreNormalizeLine( u->vars[i].value, u->topic, sizeof( u->topic ) );
			return;
		}
	}
	u->topic[0] = '\0';
}

static void AIML_AddOneCategory( int h, const char *pat, const char *th, const char *ttp, const char *top, const char *templ ) {
	aimlCategory_t *cat;
	if ( bots[h].numCategories >= AIML_MAX_CATEGORIES ) {
		Com_Printf( S_COLOR_YELLOW "AIML: max categories %d\n", AIML_MAX_CATEGORIES );
		return;
	}
	cat = &bots[h].categories[bots[h].numCategories];
	Com_Memset( cat, 0, sizeof( *cat ) );
	if ( pat && pat[0] ) {
		Q_strncpyz( cat->pattern, pat, sizeof( cat->pattern ) );
	} else if ( ttp && ttp[0] ) {
		Q_strncpyz( cat->pattern, ttp, sizeof( cat->pattern ) );
	}
	if ( th && th[0] ) {
		Q_strncpyz( cat->that, th, sizeof( cat->that ) );
	}
	if ( top && top[0] ) {
		Q_strncpyz( cat->topic, top, sizeof( cat->topic ) );
	}
	Q_strncpyz( cat->templateText, templ, sizeof( cat->templateText ) );
	if ( !cat->pattern[0] || !cat->templateText[0] ) {
		return;
	}
	AIML_CompilePat( cat->pattern, &cat->nPat, cat->patKind, cat->patTok );
	if ( cat->that[0] ) {
		AIML_CompilePat( cat->that, &cat->nThat, cat->thatKind, cat->thatTok );
	} else { cat->nThat = 0; }
	if ( cat->topic[0] ) {
		char z[64];
		AIML_CoreNormalizeLine( cat->topic, z, sizeof( z ) );
		Q_strncpyz( cat->topic, z, sizeof( cat->topic ) );
		AIML_CompilePat( cat->topic, &cat->nTopic, cat->topicKind, cat->topicTok );
	} else { cat->nTopic = 0; }
	bots[h].numCategories++;
}

/* Scan for <category>...</category> in [p0, end) */
static void AIML_LoadCategoryBlock( int h, const char *p0, const char *end, const char *deftopic ) {
	const char *p, *a, *b, *nxt, *copen;
	aimlCategory_t *cat;

	if ( !p0 || !end || p0 >= end ) { return; }
	p = p0;
	for ( ; ; ) {
		copen = p;
		while ( copen < end - 9 && strncmp( copen, "<category", 9 ) != 0 ) {
			copen++;
		}
		if ( copen >= end - 9 || strncmp( copen, "<category", 9 ) != 0 ) {
			break;
		}
		nxt = AIML_FindTag( copen, "category", &a, &b );
		if ( !a || !b ) { break; }
		if ( a >= end ) { break; }
		if ( b > end ) { b = end; }
		if ( bots[h].numCategories >= AIML_MAX_CATEGORIES ) {
			Com_Printf( S_COLOR_YELLOW "AIML: max categories %d\n", AIML_MAX_CATEGORIES );
			break;
		}
		cat = &bots[h].categories[bots[h].numCategories];
		Com_Memset( cat, 0, sizeof( *cat ) );
		if ( deftopic && deftopic[0] ) {
			Q_strncpyz( cat->topic, deftopic, sizeof( cat->topic ) );
		}
		{
			const char *ps, *pe, *is, *ie, *ts, *te, *h0, *h1;
			if ( AIML_FindTag( a, "pattern", &ps, &pe ) ) { (void)0; }
			else if ( AIML_FindTag( a, "intent", &is, &ie ) ) { ps = is; pe = ie; }
			else { p = nxt && nxt < end ? nxt : end; if ( p >= end || !nxt ) { break; } continue; }
			{
				int L = (int)( pe - ps );
				if ( L >= AIML_MAX_PATTERN_LEN ) { L = AIML_MAX_PATTERN_LEN - 1; }
				Com_Memcpy( cat->pattern, ps, L );
				cat->pattern[L] = '\0';
			}
			if ( !AIML_FindTag( a, "template", &ts, &te ) ) { p = nxt && nxt < end ? nxt : end; if ( p >= end || !nxt ) { break; } continue; }
			{
				int L = (int)( te - ts );
				if ( L >= AIML_MAX_RESPONSE_LEN ) { L = AIML_MAX_RESPONSE_LEN - 1; }
				Com_Memcpy( cat->templateText, ts, L );
				cat->templateText[L] = '\0';
			}
			if ( AIML_FindTag( a, "that", &h0, &h1 ) ) {
				int L2 = (int)( h1 - h0 );
				if ( L2 >= AIML_MAX_PATTERN_LEN ) { L2 = AIML_MAX_PATTERN_LEN - 1; }
				Com_Memcpy( cat->that, h0, L2 );
				cat->that[L2] = '\0';
			}
			if ( AIML_FindTag( a, "topic", &h0, &h1 ) ) {
				int L2 = (int)( h1 - h0 );
				if ( L2 >= (int)sizeof( cat->topic ) - 1 ) { L2 = (int)sizeof( cat->topic ) - 1; }
				Com_Memcpy( cat->topic, h0, L2 );
				cat->topic[L2] = '\0';
			}
		}
		AIML_CompilePat( cat->pattern, &cat->nPat, cat->patKind, cat->patTok );
		if ( cat->that[0] ) {
			AIML_CompilePat( cat->that, &cat->nThat, cat->thatKind, cat->thatTok );
		} else { cat->nThat = 0; }
		if ( cat->topic[0] ) {
			char z2[64];
			AIML_CoreNormalizeLine( cat->topic, z2, sizeof( z2 ) );
			Q_strncpyz( cat->topic, z2, sizeof( cat->topic ) );
			AIML_CompilePat( cat->topic, &cat->nTopic, cat->topicKind, cat->topicTok );
		} else { cat->nTopic = 0; }
		bots[h].numCategories++;
		p = nxt ? nxt : end;
		if ( p >= end || !nxt ) { break; }
	}
}

static void AIML_LoadXml( int h, const char *t ) {
	const char *e = t + strlen( t ), *p, *topen, *tclose, *in0, *na;
	char tnm[64], *gt;
	p = t;
	for ( ; ; ) {
		topen = strstr( p, "<topic" );
		if ( !topen || topen >= e ) {
			AIML_LoadCategoryBlock( h, p, e, NULL );
			return;
		}
		if ( topen > p ) { AIML_LoadCategoryBlock( h, p, topen, NULL ); }
		tnm[0] = '\0';
		na = strstr( topen, "name=\"" );
		if ( na ) {
			na += 6;
			{ const char *e2 = strchr( na, '"' );
			  if ( e2 ) { int n = (int)( e2 - na ); if ( n >= (int)sizeof( tnm ) - 1 ) n = (int)sizeof( tnm ) - 1;
			  Com_Memcpy( tnm, na, n ); tnm[n] = '\0'; } }
		}
		gt = strchr( topen, '>' );
		if ( !gt ) { return; }
		in0 = gt + 1;
		tclose = strstr( in0, "</topic>" );
		if ( !tclose || tclose > e ) {
			AIML_LoadCategoryBlock( h, in0, e, tnm[0] ? tnm : NULL );
			return;
		}
		AIML_LoadCategoryBlock( h, in0, tclose, tnm[0] ? tnm : NULL );
		p = tclose + 8;
	}
}

/* Minimal JSON: objects in "categories" array with "pattern"|"intent", "template" */
static void AIML_LoadJson( int h, const char *j ) {
	const char *arr, *c1, *c0, *d;
	arr = strstr( j, "categories" );
	if ( !arr ) { return; }
	c0 = strchr( arr, '[' );
	if ( !c0 ) { return; }
	c1 = c0 + 1;
	for ( ; ; ) {
		c1 = strchr( c1, '{' );
		if ( !c1 ) { break; }
		{ int depth; const char *x = c1; depth = 1; x++;
		  while ( *x && depth > 0 ) { if ( *x == '{' ) depth++; if ( *x == '}' ) depth--; x++; }
		  d = x;
		}
		if ( d - c1 > 1800 ) { break; }
		{ char fr[2000], pb[256], tp[256], th[256], to[64], tbuf[AIML_MAX_RESPONSE_LEN];
		int n = (int)( d - c1 );
		if ( n >= (int)sizeof( fr ) - 1 ) n = (int)sizeof( fr ) - 2;
		Com_Memcpy( fr, c1, n ); fr[n] = '\0';
		pb[0] = tbuf[0] = th[0] = to[0] = '\0'; tp[0] = '\0';
		if ( !AIML_JsonOneString( fr, "pattern", pb, sizeof( pb ) ) ) { AIML_JsonOneString( fr, "intent", pb, sizeof( pb ) ); }
		AIML_JsonOneString( fr, "template", tbuf, sizeof( tbuf ) );
		AIML_JsonOneString( fr, "that", th, sizeof( th ) );
		AIML_JsonOneString( fr, "topic", to, sizeof( to ) );
		AIML_AddOneCategory( h, pb, th, tp, to, tbuf );
		}
		c1 = d;
	}
}

static char s_aimlResponse[AIML_MAX_RESPONSE_LEN];
static void AIML_Append( char *out, int *w, int olen, const char *s ) {
	int n;
	if ( !s || *w >= olen - 1 ) { return; }
	n = (int)strlen( s );
	if ( *w + n >= olen - 1 ) n = olen - 1 - *w;
	if ( n > 0 ) { Com_Memcpy( out + *w, s, n ); *w += n; }
	out[*w] = '\0';
}

static void AIML_ProcTmpl( aimlBot_t *bot, int h, const char *userId, const char *tmpl, char *out, int olen, int sraiD );
static void AIML_InnerGetResponse( int h, const char *userId, const char *rawIn, char *out, int olen, int sraiD ) {
	aimlBot_t *B;
	aimlUser_t *U;
	char norm[AIML_MAX_PATTERN_LEN], thNorm[AIML_MAX_PATTERN_LEN], topNorm[64];
	char ut[AIML_MAX_LINE_TOKENS][AIML_TOKW], thT[AIML_MAX_LINE_TOKENS][AIML_TOKW], tTop[AIML_MAX_LINE_TOKENS][AIML_TOKW];
	int nU, nTh, nTop, i, bi, bsc, sc;

	B = &bots[h];
	out[0] = '\0';
	if ( olen < 1 ) { return; }
	U = AIML_FindUser( B, userId, qtrue );
	AIML_SyncTopic( U );
	AIML_CoreNormalizeLine( rawIn, norm, sizeof( norm ) );
	nU = AIML_SplitTokens( norm, ut );
	AIML_CoreNormalizeLine( U->lastResponse, thNorm, sizeof( thNorm ) );
	nTh = AIML_SplitTokens( thNorm, thT );
	/* Topic stack value as tokens (for <topic>... pattern and <topicstar/>). */
	AIML_CoreNormalizeLine( U->topic, topNorm, sizeof( topNorm ) );
	nTop = AIML_SplitTokens( topNorm, tTop );

	bi = -1;
	bsc = -1;
	for ( i = 0; i < B->numCategories; i++ ) {
		aimlCategory_t *c = &B->categories[i];
		if ( c->nTopic > 0 ) {
			topic_nStars = 0;
			if ( !AIML_MatchExpr( c->nTopic, c->topicKind, c->topicTok, nTop, tTop, topic_stars, &topic_nStars ) ) {
				continue;
			}
		} else {
			topic_nStars = 0;
		}
		if ( c->nThat > 0 ) {
			that_nStars = 0;
			if ( !AIML_MatchExpr( c->nThat, c->thatKind, c->thatTok, nTh, thT, that_stars, &that_nStars ) ) {
				continue;
			}
		} else {
			that_nStars = 0;
		}
		in_nStars = 0;
		if ( !AIML_MatchExpr( c->nPat, c->patKind, c->patTok, nU, ut, in_stars, &in_nStars ) ) {
			continue;
		}
		sc = AIML_SpecScore( c ) * 10 + c->nThat + c->nTopic;
		if ( bi < 0 || sc > bsc || ( sc == bsc && i < bi ) ) {
			bi = i;
			bsc = sc;
		}
	}
	if ( bi < 0 ) {
		out[0] = '\0';
		if ( sraiD == 0 && g_aimlDebug && g_aimlDebug->integer > 0 ) {
			Com_Printf( "AIML: no match (bot %d user %.48s in=%.64s)\n", h, userId, norm );
		}
		if ( sraiD == 0 && EDA_IsEnabled() ) {
			char ev[EDA_MAX_PAYLOAD];
			Com_sprintf( ev, sizeof( ev ), "bot=%d no_match user=%.32s in=%.120s", h, userId, norm );
			(void)EDA_Publish( "aiml.nomatch", ev );
		}
		return;
	}
	{
		aimlCategory_t *c = &B->categories[bi];
		if ( c->nTopic > 0 ) {
			topic_nStars = 0;
			AIML_MatchExpr( c->nTopic, c->topicKind, c->topicTok, nTop, tTop, topic_stars, &topic_nStars );
		} else {
			topic_nStars = 0;
		}
		if ( c->nThat > 0 ) {
			that_nStars = 0;
			AIML_MatchExpr( c->nThat, c->thatKind, c->thatTok, nTh, thT, that_stars, &that_nStars );
		} else {
			that_nStars = 0;
		}
		in_nStars = 0;
		AIML_MatchExpr( c->nPat, c->patKind, c->patTok, nU, ut, in_stars, &in_nStars );
		AIML_ProcTmpl( B, h, userId, c->templateText, out, olen, sraiD );
		if ( sraiD == 0 && g_aimlDebug && g_aimlDebug->integer > 0 ) {
			Com_Printf( "AIML: match cat %d (bot %d) pat=%.80s score=%d out=%.120s\n", bi, h, c->pattern, bsc, out );
		}
	}
	if ( sraiD == 0 && U && out[0] ) {
		Q_strncpyz( U->lastResponse, out, sizeof( U->lastResponse ) );
	}
}

static void AIML_ProcTmpl( aimlBot_t *bot, int h, const char *userId, const char *tmpl, char *out, int olen, int sraiD ) {
	const char *p;
	int w;
	char sraiExp[AIML_MAX_PATTERN_LEN], sub[AIML_MAX_RESPONSE_LEN], saveI[AIML_MAX_STAR][256], saveT[AIML_MAX_STAR][256], saveV[AIML_MAX_STAR][256];
	int nsi, nst, nsv, idx;
	char ab[16], nbuf[64], vbuf[512], cnm[64], cval[256], liVal[64];
	const char *a, *b, *c1, *c2, *nli, *ra, *rb, *l1, *l2, *ic, *ie, *tli;
	int pick, j, k, L2, cnt;
	char it[AIML_MAX_RANDOM_ITEMS][256];

	(void)bot;
	if ( !tmpl || olen < 2 || sraiD > AIML_MAX_SRAI_DEPTH ) { if ( out ) { out[0] = '\0'; } return; }
	p = tmpl; w = 0; out[0] = '\0';
	while ( *p && w < olen - 1 ) {
		if ( *p != '<' ) { out[w++] = *p++; out[w] = '\0'; continue; }
		/* <star */
		if ( !Q_strncmp( p, "<star", 5 ) ) {
			if ( ( a = strchr( p, '>' ) ) != NULL ) {
				idx = 0;
				if ( AIML_GetAttr( p, "index", ab, sizeof( ab ) ) ) { idx = atoi( ab ) - 1; }
				if ( idx < 0 ) { idx = 0; }
				if ( idx < in_nStars && in_stars[idx][0] ) { AIML_Append( out, &w, olen, in_stars[idx] ); }
				p = a + 1;
			} else { p++; }
			continue;
		}
		/* <thatstar */
		if ( !Q_strncmp( p, "<thatstar", 9 ) ) {
			if ( ( a = strchr( p, '>' ) ) != NULL ) {
				idx = 0;
				if ( AIML_GetAttr( p, "index", ab, sizeof( ab ) ) ) { idx = atoi( ab ) - 1; }
				if ( idx < 0 ) { idx = 0; }
				if ( idx < that_nStars && that_stars[idx][0] ) { AIML_Append( out, &w, olen, that_stars[idx] ); }
				p = a + 1;
			} else { p++; }
			continue;
		}
		/* <topicstar */
		if ( !Q_strncmp( p, "<topicstar", 10 ) ) {
			if ( ( a = strchr( p, '>' ) ) != NULL ) {
				idx = 0;
				if ( AIML_GetAttr( p, "index", ab, sizeof( ab ) ) ) { idx = atoi( ab ) - 1; }
				if ( idx < 0 ) { idx = 0; }
				if ( idx < topic_nStars && topic_stars[idx][0] ) { AIML_Append( out, &w, olen, topic_stars[idx] ); }
				p = a + 1;
			} else { p++; }
			continue;
		}
		/* <bot */
		if ( !Q_strncmp( p, "<bot ", 5 ) && ( a = strchr( p, '>' ) ) && AIML_GetAttr( p, "name", nbuf, sizeof( nbuf ) ) ) {
			AIML_Append( out, &w, olen, AIML_GetBotProperty( h, nbuf ) );
			p = a + 1; continue;
		}
		/* <get */
		if ( !Q_strncmp( p, "<get ", 5 ) && ( a = strchr( p, '>' ) ) && AIML_GetAttr( p, "name", nbuf, sizeof( nbuf ) ) ) {
			{ const char *g = AIML_GetUserVar( h, userId, nbuf );
			if ( g && g[0] ) { AIML_Append( out, &w, olen, g ); } }
			p = a + 1; continue;
		}
		/* <set: no output (AIML 3 Core) */
		if ( !Q_strncmp( p, "<set ", 5 ) && AIML_GetAttr( p, "name", nbuf, sizeof( nbuf ) ) && AIML_FindTag( p, "set", &a, &b ) ) {
			char evb[512];
			k = (int)( b - a );
			if ( k >= (int)sizeof( vbuf ) - 1 ) { k = (int)sizeof( vbuf ) - 1; }
			Com_Memcpy( vbuf, a, k );
			vbuf[k] = '\0';
			AIML_ProcTmpl( bot, h, userId, vbuf, evb, sizeof( evb ), sraiD );
			AIML_SetUserVar( h, userId, nbuf, evb );
			{ aimlUser_t *uu = AIML_FindUser( &bots[h], userId, qfalse );
			if ( uu && !Q_stricmp( nbuf, "topic" ) ) { AIML_SyncTopic( uu ); } }
			p = b;
			if ( p[0] == '<' && !Q_strncmp( p, "</set", 4 ) && ( p = strchr( p, '>' ) ) ) { p++; }
			continue;
		}
		/* redacted_thinking */
		if ( !Q_strncmp( p, "<redacted_thinking", 18 ) && AIML_FindTag( p, "redacted_thinking", &ic, &ie ) ) {
			k = (int)( ie - ic );
			if ( k >= (int)sizeof( vbuf ) - 1 ) { k = (int)sizeof( vbuf ) - 1; }
			Com_Memcpy( vbuf, ic, k );
			vbuf[k] = '\0';
			AIML_ProcTmpl( bot, h, userId, vbuf, sub, sizeof( sub ), sraiD );
			p = ie;
			if ( p[0] == '<' && !Q_strncmp( p, "</redacted_thinking", 19 ) && ( p = strchr( p, '>' ) ) ) { p++; }
			continue;
		}
		/* <random> */
		if ( !Q_strncmp( p, "<random>", 8 ) && AIML_FindTag( p, "random", &ra, &rb ) ) {
			cnt = 0; tli = ra;
			while ( tli < rb && cnt < AIML_MAX_RANDOM_ITEMS && ( nli = AIML_FindTag( tli, "li", &l1, &l2 ) ) && l1 < rb ) {
				L2 = (int)( l2 - l1 );
				if ( L2 >= 256 ) { L2 = 255; }
				Com_Memcpy( it[cnt], l1, L2 );
				it[cnt][L2] = '\0';
				cnt++;
				tli = nli;
			}
			if ( cnt > 0 ) { pick = rand() % cnt; AIML_ProcTmpl( bot, h, userId, it[pick], sub, sizeof( sub ), sraiD ); AIML_Append( out, &w, olen, sub ); }
			p = rb; if ( !Q_strncmp( p, "</random>", 9 ) ) { p += 9; }
			continue;
		}
		/* <condition name=...> */
		if ( !Q_strncmp( p, "<condition", 10 ) && AIML_GetAttr( p, "name", cnm, sizeof( cnm ) ) && AIML_FindTag( p, "condition", &a, &b ) ) {
			const char *def1 = NULL, *def2 = NULL;
			char tmpN[256];
			Q_strncpyz( cval, AIML_GetUserVar( h, userId, cnm ), sizeof( cval ) );
			AIML_CoreNormalizeLine( cval, tmpN, sizeof( tmpN ) );
			Q_strncpyz( cval, tmpN, sizeof( cval ) );
			tli = a; j = 0;
			while ( tli < b && ( nli = AIML_FindTag( tli, "li", &c1, &c2 ) ) && c1 < b ) {
				if ( AIML_GetAttr( tli, "value", liVal, sizeof( liVal ) ) ) {
					char nlv[64];
					AIML_CoreNormalizeLine( liVal, nlv, sizeof( nlv ) );
					if ( cval[0] && nlv[0] && !Q_stricmp( cval, nlv ) ) {
						L2 = (int)( c2 - c1 );
						if ( L2 >= (int)sizeof( vbuf ) - 1 ) { L2 = (int)sizeof( vbuf ) - 1; }
						Com_Memcpy( vbuf, c1, L2 );
						vbuf[L2] = '\0';
						AIML_ProcTmpl( bot, h, userId, vbuf, sub, sizeof( sub ), sraiD );
						AIML_Append( out, &w, olen, sub );
						j = 1;
						break;
					}
				} else if ( !def1 ) { def1 = c1; def2 = c2; }
				tli = nli;
			}
			if ( !j && def1 && def2 ) {
				L2 = (int)( def2 - def1 );
				if ( L2 >= (int)sizeof( vbuf ) - 1 ) { L2 = (int)sizeof( vbuf ) - 1; }
				Com_Memcpy( vbuf, def1, L2 );
				vbuf[L2] = '\0';
				AIML_ProcTmpl( bot, h, userId, vbuf, sub, sizeof( sub ), sraiD );
				AIML_Append( out, &w, olen, sub );
			}
			{ const char *ec2 = strstr( a, "</condition>" );
			p = ec2 ? ec2 + 12 : b; }
			continue;
		}
		/* <srai> */
		if ( !Q_strncmp( p, "<srai>", 6 ) && sraiD < AIML_MAX_SRAI_DEPTH && AIML_FindTag( p, "srai", &a, &b ) ) {
			char sraiWork[AIML_MAX_PATTERN_LEN];
			k = (int)( b - a );
			if ( k >= (int)sizeof( sraiExp ) - 1 ) { k = (int)sizeof( sraiExp ) - 1; }
			Com_Memcpy( sraiExp, a, k );
			sraiExp[k] = '\0';
			AIML_ProcTmpl( bot, h, userId, sraiExp, sraiWork, sizeof( sraiWork ), sraiD + 1 );
			Com_Memcpy( saveI, in_stars, sizeof( saveI ) );
			Com_Memcpy( saveT, that_stars, sizeof( saveT ) );
			Com_Memcpy( saveV, topic_stars, sizeof( saveV ) );
			nsi = in_nStars; nst = that_nStars; nsv = topic_nStars;
			AIML_InnerGetResponse( h, userId, sraiWork, sub, sizeof( sub ), sraiD + 1 );
			Com_Memcpy( in_stars, saveI, sizeof( saveI ) );
			Com_Memcpy( that_stars, saveT, sizeof( saveT ) );
			Com_Memcpy( topic_stars, saveV, sizeof( saveV ) );
			in_nStars = nsi; that_nStars = nst; topic_nStars = nsv;
			AIML_Append( out, &w, olen, sub );
			p = b; if ( !Q_strncmp( p, "</srai>", 7 ) ) p += 7;
			continue;
		}
		{ const char *ex = strchr( p, '>' );
		p = ex ? ex + 1 : p + 1; }
	}
}

void AIML_Init( void ) {
	Com_Memset( bots, 0, sizeof( bots ) );
	numBots = 0;
	g_aiml3 = Cvar_Get( "g_aiml3", "1", CVAR_ARCHIVE );
	g_aimlJson = Cvar_Get( "g_aimlJson", "0", CVAR_ARCHIVE );
	g_aimlDebug = Cvar_Get( "g_aimlDebug", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( g_aimlDebug, "AIML: log top match / no-match to console; 0=off, 1=on." );
	if ( g_aiml3->integer ) {
		Com_Printf( "AIML: AIML 3.0 Core path (token * / _ , <that>, <topic>, JSON with g_aimlJson 1; cvars g_aiml3, g_aimlJson, g_aimlDebug)\n" );
	}
	if ( g_aimlDebug && g_aimlDebug->integer ) {
		Com_Printf( "AIML: g_aimlDebug=1 (verbose matching enabled)\n" );
	}
}

void AIML_Shutdown( void ) {
	numBots = 0;
	Com_Memset( bots, 0, sizeof( bots ) );
}

aimlBotHandle_t AIML_CreateBot( const char *name ) {
	if ( numBots >= AIML_MAX_BOTS ) {
		return -1;
	}
	{
		int idx = numBots++;
		Com_Memset( &bots[idx], 0, sizeof( aimlBot_t ) );
		Q_strncpyz( bots[idx].name, name, sizeof( bots[idx].name ) );
		bots[idx].active = qtrue;
		AIML_SetBotProperty( idx, "name", name );
		AIML_SetBotProperty( idx, "version", "3.0" );
		Com_Printf( "AIML: bot '%s' (handle %d)\n", name, idx );
		return idx;
	}
}

void AIML_DestroyBot( aimlBotHandle_t h ) {
	if ( VALID_BOT( h ) ) {
		bots[h].active = qfalse;
	}
}

void AIML_SetBotProperty( aimlBotHandle_t h, const char *key, const char *value ) {
	int i;
	if ( !VALID_BOT( h ) ) {
		return;
	}
	for ( i = 0; i < bots[h].numProperties; i++ ) {
		if ( !Q_stricmp( bots[h].properties[i].key, key ) ) {
			Q_strncpyz( bots[h].properties[i].value, value, sizeof( bots[h].properties[i].value ) );
			return;
		}
	}
	if ( bots[h].numProperties < AIML_MAX_PROPERTIES ) {
		Q_strncpyz( bots[h].properties[bots[h].numProperties].key, key, sizeof( bots[h].properties[0].key ) );
		Q_strncpyz( bots[h].properties[bots[h].numProperties].value, value, sizeof( bots[h].properties[0].value ) );
		bots[h].numProperties++;
	}
}

const char *AIML_GetBotProperty( aimlBotHandle_t h, const char *key ) {
	int i;
	if ( !VALID_BOT( h ) ) {
		return "";
	}
	for ( i = 0; i < bots[h].numProperties; i++ ) {
		if ( !Q_stricmp( bots[h].properties[i].key, key ) ) {
			return bots[h].properties[i].value;
		}
	}
	return "";
}

qboolean AIML_LoadFile( aimlBotHandle_t h, const char *filename ) {
	void *buf;
	int len;
	const char *ext;
	if ( !VALID_BOT( h ) ) {
		return qfalse;
	}
	if ( !g_aiml3 ) {
		g_aiml3 = Cvar_Get( "g_aiml3", "1", CVAR_ARCHIVE );
	}
	if ( !g_aimlJson ) {
		g_aimlJson = Cvar_Get( "g_aimlJson", "0", CVAR_ARCHIVE );
	}
	len = FS_ReadFile( filename, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( S_COLOR_YELLOW "AIML: could not load %s\n", filename );
		return qfalse;
	}
	ext = COM_GetExtension( filename );
	if ( ( ext && !Q_stricmp( ext, "json" ) ) || ( g_aimlJson->integer > 0 && strstr( (const char *)buf, "\"categories\"" ) != NULL ) ) {
		AIML_LoadJson( h, (const char *)buf );
	} else {
		AIML_LoadXml( h, (const char *)buf );
	}
	FS_FreeFile( buf );
	Com_Printf( "AIML: %s -> %d categories (bot %s)\n", filename, bots[h].numCategories, bots[h].name );
	return qtrue;
}

int AIML_GetCategoryCount( aimlBotHandle_t h ) {
	return VALID_BOT( h ) ? bots[h].numCategories : 0;
}

const char *AIML_GetResponse( aimlBotHandle_t h, const char *userId, const char *input ) {
	if ( !VALID_BOT( h ) || !userId || !input ) {
		s_aimlResponse[0] = '\0';
		return s_aimlResponse;
	}
	AIML_InnerGetResponse( h, userId, input, s_aimlResponse, sizeof( s_aimlResponse ), 0 );
	if ( s_aimlResponse[0] && EDA_IsEnabled() ) {
		char ev[EDA_MAX_PAYLOAD];
		Com_sprintf( ev, sizeof( ev ), "bot=%d user=%.48s r=%.150s", h, userId, s_aimlResponse );
		(void)EDA_Publish( "aiml.response", ev );
	}
	return s_aimlResponse;
}

void AIML_SetUserVar( aimlBotHandle_t h, const char *userId, const char *key, const char *value ) {
	aimlUser_t *u;
	int i;
	if ( !VALID_BOT( h ) ) {
		return;
	}
	u = AIML_FindUser( &bots[h], userId, qtrue );
	if ( !u ) {
		return;
	}
	for ( i = 0; i < u->numVars; i++ ) {
		if ( !Q_stricmp( u->vars[i].key, key ) ) {
			Q_strncpyz( u->vars[i].value, value, sizeof( u->vars[i].value ) );
			if ( !Q_stricmp( key, "topic" ) ) {
				AIML_SyncTopic( u );
			}
			return;
		}
	}
	if ( u->numVars < AIML_MAX_USER_VARS ) {
		Q_strncpyz( u->vars[u->numVars].key, key, sizeof( u->vars[0].key ) );
		Q_strncpyz( u->vars[u->numVars].value, value, sizeof( u->vars[0].value ) );
		u->numVars++;
		if ( !Q_stricmp( key, "topic" ) ) {
			AIML_SyncTopic( u );
		}
	}
}

const char *AIML_GetUserVar( aimlBotHandle_t h, const char *userId, const char *key ) {
	aimlUser_t *u;
	int i;
	if ( !VALID_BOT( h ) ) {
		return "";
	}
	u = AIML_FindUser( &bots[h], userId, qfalse );
	if ( !u ) {
		return "";
	}
	for ( i = 0; i < u->numVars; i++ ) {
		if ( !Q_stricmp( u->vars[i].key, key ) ) {
			return u->vars[i].value;
		}
	}
	return "";
}

void AIML_ResetUser( aimlBotHandle_t h, const char *userId ) {
	aimlUser_t *u;
	if ( !VALID_BOT( h ) ) {
		return;
	}
	u = AIML_FindUser( &bots[h], userId, qfalse );
	if ( u ) {
		u->numVars = 0;
		u->lastResponse[0] = '\0';
		u->topic[0] = '\0';
	}
}
