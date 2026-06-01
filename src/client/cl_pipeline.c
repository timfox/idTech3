/*
===========================================================================
Shared shell-template expansion for external generative pipelines.
===========================================================================
*/

#include "client.h"
#include "cl_pipeline.h"

#if defined( USE_FLUX ) || defined( USE_TRELLIS ) || defined( USE_SPEC_ENERGY )

qboolean CL_ShellEscapeArg( const char *in, char *out, size_t out_size ) {
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

static qboolean CL_PipelineAppendEscaped( char *out, size_t *len, size_t maxlen, const char *raw ) {
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
		const cl_pipeline_expand_t *ex ) {
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
		out[o++] = *p++;
	}
	out[o] = '\0';
	return qtrue;
}

#endif
