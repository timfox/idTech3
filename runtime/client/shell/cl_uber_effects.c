/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Declarative particle effect defs mapped from TIKI/Babble events.
===========================================================================
*/

#include "client.h"
#include "cl_particles.h"
#include "cl_uber_effects.h"

#define FX_DEF_MAX 64

typedef struct fxDef_s {
	char name[64];
	char kind[32]; /* smoke, spark, blood, fire */
	float scale;
	int count;
	qboolean used;
} fxDef_t;

static fxDef_t s_fx[FX_DEF_MAX];
static int s_fxCount;
static cvar_t *cl_uberEffects;

void CL_UberEffects_Init( void ) {
	s_fxCount = 0;
	Com_Memset( s_fx, 0, sizeof( s_fx ) );
	cl_uberEffects = Cvar_Get( "cl_uberEffects", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_uberEffects, "Enable ÜberTools declarative particle effect defs." );
	Com_Printf( "UberEffects: ready (cl_uberEffects=%d)\n", cl_uberEffects->integer );
}

int CL_UberEffects_Load( const char *path ) {
	void *buf;
	int len;
	const char *p;
	char line[512];
	fxDef_t *cur = NULL;
	int loaded = 0;

	if ( !cl_uberEffects || !cl_uberEffects->integer ) {
		return 0;
	}
	if ( !path || strstr( path, ".." ) ) {
		return 0;
	}
	len = FS_ReadFile( path, &buf );
	if ( len <= 0 || !buf ) {
		return 0;
	}
	p = (const char *)buf;
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
		if ( !Q_stricmp( tok, "effect" ) ) {
			if ( s_fxCount >= FX_DEF_MAX ) {
				break;
			}
			cur = &s_fx[s_fxCount++];
			Com_Memset( cur, 0, sizeof( *cur ) );
			Q_strncpyz( cur->name, rest, sizeof( cur->name ) );
			Q_strncpyz( cur->kind, "smoke", sizeof( cur->kind ) );
			cur->scale = 1.0f;
			cur->count = 8;
			cur->used = qtrue;
			loaded++;
		} else if ( cur ) {
			if ( !Q_stricmp( tok, "kind" ) || !Q_stricmp( tok, "type" ) ) {
				Q_strncpyz( cur->kind, rest, sizeof( cur->kind ) );
			} else if ( !Q_stricmp( tok, "scale" ) ) {
				cur->scale = (float)atof( rest );
			} else if ( !Q_stricmp( tok, "count" ) ) {
				cur->count = atoi( rest );
			}
		}
	}
	FS_FreeFile( buf );
	Com_Printf( "UberEffects: loaded %d from %s\n", loaded, path );
	return loaded;
}

qboolean CL_UberEffects_Emit( const char *name, float x, float y, float z ) {
	int i;
	vec3_t org;
	vec3_t up = { 0, 0, 1 };
	vec3_t vel = { 0, 0, 50 };

	if ( !cl_uberEffects || !cl_uberEffects->integer || !name ) {
		return qfalse;
	}
	VectorSet( org, x, y, z );
	for ( i = 0; i < s_fxCount; i++ ) {
		if ( !s_fx[i].used || Q_stricmp( s_fx[i].name, name ) ) {
			continue;
		}
		if ( !Q_stricmp( s_fx[i].kind, "spark" ) ) {
			Particles_EmitSparks( org, vel, s_fx[i].count, s_fx[i].scale * 8.0f, 200.0f );
		} else if ( !Q_stricmp( s_fx[i].kind, "blood" ) ) {
			Particles_EmitBlood( 0, org, up, s_fx[i].scale * 500.0f );
		} else if ( !Q_stricmp( s_fx[i].kind, "dust" ) ) {
			Particles_EmitDust( org, up, s_fx[i].scale, 0 );
		} else {
			Particles_EmitSmoke( 0, org, up, 2000.0f, s_fx[i].scale, s_fx[i].scale * 1.5f, 0.6f, 0 );
		}
		return qtrue;
	}
	return qfalse;
}
