/*
===========================================================================
Shared shell-template expansion for external generative pipelines (FLUX/TRELLIS/SEGA).
===========================================================================
*/

#ifndef CL_PIPELINE_H
#define CL_PIPELINE_H

#include "../qcommon/q_shared.h"

#if defined( USE_FLUX ) || defined( USE_TRELLIS ) || defined( USE_SEGA )

typedef struct {
	const char *repo;
	const char *base;
	const char *engine;
	const char *py;
	const char *conda;
	const char *image;
	const char *output;
	const char *model;
	const char *decimation;
	const char *texture_size;
	const char *args;
} cl_pipeline_expand_t;

qboolean CL_ShellEscapeArg( const char *in, char *out, size_t out_size );
qboolean CL_PipelineExpandTemplate( char *out, size_t maxlen, const char *tmpl,
		const cl_pipeline_expand_t *ex );

#endif

#endif /* CL_PIPELINE_H */
