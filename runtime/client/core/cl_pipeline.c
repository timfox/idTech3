/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_pipeline.h"

#include <stdio.h>
#include <string.h>

qboolean CL_ShellEscapeArg( const char *in, char *out, size_t out_size )
{
	size_t pos = 0;
	if ( !in || !out || out_size == 0 ) {
		return qfalse;
	}

	for ( const char *p = in; *p; ++p ) {
		char c = *p;
		if ( c == '\n' || c == '\r' || c == '\t' ) {
			c = ' ';
		}
		if ( c == '"' || c == '\\' || c == '$' || c == '`' ) {
			if ( pos + 2 >= out_size ) {
				return qfalse;
			}
			out[pos++] = '\\';
			out[pos++] = c;
		} else {
			if ( pos + 1 >= out_size ) {
				return qfalse;
			}
			out[pos++] = c;
		}
	}
	out[pos] = '\0';
	return qtrue;
}

static qboolean CL_PipelineAppendEscaped( char *out, size_t *len, size_t maxlen, const char *raw )
{
	char esc[4096];
	size_t add;

	if ( !raw ) {
		raw = "";
	}
	if ( !CL_ShellEscapeArg( raw, esc, sizeof( esc ) ) ) {
		return qfalse;
	}
	add = strlen( esc );
	if ( *len + add >= maxlen ) {
		return qfalse;
	}
	memcpy( out + *len, esc, add + 1 );
	*len += add;
	return qtrue;
}

qboolean CL_PipelineExpandTemplate( char *out, size_t maxlen, const char *tmpl,
		const cl_pipeline_expand_t *ex )
{
	size_t o = 0;
	const char *p;

	if ( !tmpl || !*tmpl || maxlen < 8 || !ex ) {
		return qfalse;
	}
	out[0] = '\0';
	for ( p = tmpl; *p && o + 1 < maxlen; ) {
		if ( p[0] == '%' && p[1] == '%' ) {
			out[o++] = '%';
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'R' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->repo ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'B' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->base ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'E' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->engine ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'P' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->py ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'N' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->conda ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'I' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->image ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'O' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->output ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'M' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->model ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'D' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->decimation ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'T' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->texture_size ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'A' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->args ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'G' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->genome ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		if ( p[0] == '%' && p[1] == 'S' ) {
			if ( !CL_PipelineAppendEscaped( out, &o, maxlen, ex->slot ) ) {
				return qfalse;
			}
			p += 2;
			continue;
		}
		out[o++] = *p++;
	}
	out[o] = '\0';
	return qtrue;
}

qboolean CL_PipelineRunTemplate( const char *tmpl, const cl_pipeline_expand_t *ex,
		qboolean quiet, const char *logPrefix )
{
	char cmd[8192];

	if ( !CL_PipelineExpandTemplate( cmd, sizeof( cmd ), tmpl, ex ) ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "%s: expanded command too long or bad path characters\n",
				logPrefix ? logPrefix : "pipeline" );
		}
		return qfalse;
	}
	if ( !quiet ) {
		Com_Printf( "%s: executing: %s\n", logPrefix ? logPrefix : "pipeline", cmd );
	}
	if ( system( cmd ) != 0 ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "%s: shell returned non-zero\n", logPrefix ? logPrefix : "pipeline" );
		}
		return qfalse;
	}
	return qtrue;
}

qboolean CL_PipelineFileExists( const char *path )
{
	FILE *f;

	if ( !path || !path[0] ) {
		return qfalse;
	}
	f = fopen( path, "rb" );
	if ( f ) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}

void CL_PipelineResolvePath( const char *base, const char *in, char *out, size_t out_size )
{
	if ( !in || !in[0] || !out || out_size == 0 ) {
		if ( out && out_size > 0 ) {
			out[0] = '\0';
		}
		return;
	}
	if ( in[0] == '/' || ( in[0] && in[1] == ':' ) ) {
		Q_strncpyz( out, in, out_size );
		return;
	}
	if ( base && base[0] ) {
		Com_sprintf( out, out_size, "%s/%s", base, in );
	} else {
		Q_strncpyz( out, in, out_size );
	}
}

void CL_PipelineEnsureOutputDir( const char *fullPath )
{
	char dir[MAX_OSPATH];
	char *slash;

	if ( !fullPath || !fullPath[0] ) {
		return;
	}
	Q_strncpyz( dir, fullPath, sizeof( dir ) );
	slash = strrchr( dir, '/' );
	if ( !slash ) {
		slash = strrchr( dir, '\\' );
	}
	if ( slash ) {
		*slash = '\0';
		Sys_Mkdir( dir );
	}
}
