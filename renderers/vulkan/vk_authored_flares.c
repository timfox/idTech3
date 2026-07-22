/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Authored lens-flare definitions (ÜberTools-compatible interchange).
Format: flare <name>
  texture <path>
  size <float>
  intensity <float>
  color <r> <g> <b>
===========================================================================
*/

#include "tr_local.h"
#include "vk_lens_flare.h"

#define FLARE_DEF_MAX 32

typedef struct authoredFlare_s {
	char name[64];
	char texture[MAX_QPATH];
	float size;
	float intensity;
	vec3_t color;
	qboolean used;
} authoredFlare_t;

static authoredFlare_t s_flares[FLARE_DEF_MAX];
static int s_flareCount;
static cvar_t *r_authoredFlares;

void R_AuthoredFlares_Init( void ) {
	s_flareCount = 0;
	Com_Memset( s_flares, 0, sizeof( s_flares ) );
	r_authoredFlares = ri.Cvar_Get( "r_authoredFlares", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_authoredFlares, "Enable authored flare definitions (flares/*.flare)." );
	ri.Printf( PRINT_ALL, "AuthoredFlares: ready (r_authoredFlares=%d)\n", r_authoredFlares->integer );
}

int R_AuthoredFlares_Load( const char *path ) {
	union { char *c; void *v; } buf;
	int len;
	const char *p;
	char line[512];
	authoredFlare_t *cur = NULL;
	int loaded = 0;

	if ( !r_authoredFlares || !r_authoredFlares->integer ) {
		return 0;
	}
	if ( !path || strstr( path, ".." ) ) {
		return 0;
	}
	len = ri.FS_ReadFile( path, &buf.v );
	if ( len <= 0 || !buf.c ) {
		return 0;
	}

	p = buf.c;
	while ( *p ) {
		const char *le = strchr( p, '\n' );
		size_t n;
		char *tok, *rest;
		if ( !le ) {
			le = p + strlen( p );
		}
		n = (size_t)( le - p );
		if ( n >= sizeof( line ) ) {
			n = sizeof( line ) - 1;
		}
		Com_Memcpy( line, p, n );
		line[n] = '\0';
		p = *le ? le + 1 : le;
		while ( line[0] == ' ' || line[0] == '\t' ) {
			memmove( line, line + 1, strlen( line ) );
		}
		if ( !line[0] || line[0] == '#' ) {
			continue;
		}
		tok = line;
		rest = line;
		while ( *rest && *rest != ' ' && *rest != '\t' ) {
			rest++;
		}
		if ( *rest ) {
			*rest++ = '\0';
			while ( *rest == ' ' || *rest == '\t' ) {
				rest++;
			}
		}
		if ( !Q_stricmp( tok, "flare" ) ) {
			if ( s_flareCount >= FLARE_DEF_MAX ) {
				break;
			}
			cur = &s_flares[s_flareCount++];
			Com_Memset( cur, 0, sizeof( *cur ) );
			Q_strncpyz( cur->name, rest, sizeof( cur->name ) );
			cur->size = 1.0f;
			cur->intensity = 1.0f;
			VectorSet( cur->color, 1, 1, 1 );
			cur->used = qtrue;
			loaded++;
		} else if ( cur ) {
			if ( !Q_stricmp( tok, "texture" ) ) {
				Q_strncpyz( cur->texture, rest, sizeof( cur->texture ) );
			} else if ( !Q_stricmp( tok, "size" ) ) {
				cur->size = (float)atof( rest );
			} else if ( !Q_stricmp( tok, "intensity" ) ) {
				cur->intensity = (float)atof( rest );
			} else if ( !Q_stricmp( tok, "color" ) ) {
				sscanf( rest, "%f %f %f", &cur->color[0], &cur->color[1], &cur->color[2] );
			}
		}
	}
	ri.FS_FreeFile( buf.v );
	ri.Printf( PRINT_ALL, "AuthoredFlares: loaded %d from %s\n", loaded, path );
	return loaded;
}

const authoredFlare_t *R_AuthoredFlares_Find( const char *name ) {
	int i;
	if ( !name ) {
		return NULL;
	}
	for ( i = 0; i < s_flareCount; i++ ) {
		if ( s_flares[i].used && !Q_stricmp( s_flares[i].name, name ) ) {
			return &s_flares[i];
		}
	}
	return NULL;
}

qboolean R_AuthoredFlares_Get( const char *name, char *textureOut, int textureSize,
	float *sizeOut, float *intensityOut, vec3_t colorOut ) {
	const authoredFlare_t *f = R_AuthoredFlares_Find( name );
	if ( !f ) {
		return qfalse;
	}
	if ( textureOut && textureSize > 0 ) {
		Q_strncpyz( textureOut, f->texture, textureSize );
	}
	if ( sizeOut ) {
		*sizeOut = f->size;
	}
	if ( intensityOut ) {
		*intensityOut = f->intensity;
	}
	if ( colorOut ) {
		VectorCopy( f->color, colorOut );
	}
	return qtrue;
}

int R_AuthoredFlares_Count( void ) {
	return s_flareCount;
}
