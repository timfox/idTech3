/*
===========================================================================
GCC-FER / CA-FER benchmark tables (Tables III–V, DFEW Table IV).
===========================================================================
*/

#include "gccfer/gccfer.h"

#include <string.h>

static const gccfer_benchmark_row_t gccfer_table[] = {
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 43.91f, 49.78f }, "Former-DFER" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 44.00f, 51.41f }, "NR-DFERNet" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 49.73f, 52.81f }, "M3DFEL" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 54.25f, 56.65f }, "3D CNN" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 54.20f, 58.10f }, "Culture-Agnostic ViViT" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 56.12f, 59.47f }, "DPCNet" },
	{ GCCFER_METHOD_RANDOM_EMBED, { 57.00f, 61.20f }, "Random Embedding Init" },
	{ GCCFER_METHOD_ONEHOT, { 58.99f, 63.48f }, "One-hot Concatenation" },
	{ GCCFER_METHOD_SEPARATE_HEADS, { 59.39f, 64.17f }, "Separate Cultural Heads" },
	{ GCCFER_METHOD_AU_CONCAT_FIXED, { 59.54f, 63.80f }, "AU-concat Fixed" },
	{ GCCFER_METHOD_CAFER, { 61.70f, 64.80f }, "CA-FER (Proposed)" },
};

static const gccfer_benchmark_row_t dfew_table[] = {
	{ GCCFER_METHOD_CAFER, { 65.45f, 76.03f }, "S2D (pretrained)" },
	{ GCCFER_METHOD_CAFER, { 63.41f, 74.43f }, "MAE-DFER (pretrained)" },
	{ GCCFER_METHOD_CAFER, { 62.83f, 74.27f }, "SVFAP (pretrained)" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 57.11f, 66.32f }, "DPCNet" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 57.09f, 69.87f }, "SlowR50-SA" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 56.10f, 69.25f }, "M3DFEL" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 55.71f, 69.24f }, "IAL" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 55.77f, 68.19f }, "NR-DFERNet" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 54.58f, 66.65f }, "STT" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 54.28f, 66.81f }, "Mamba-style SSM" },
	{ GCCFER_METHOD_CULTURE_AGNOSTIC, { 53.69f, 65.70f }, "Former-DFER" },
	{ GCCFER_METHOD_CAFER, { 63.93f, 65.21f }, "CA-FER (Proposed)" },
};

static const gccfer_metrics_t per_culture[GCCFER_NUM_CULTURES] = {
	{ 63.1f, 62.7f }, /* Caucasian */
	{ 60.6f, 62.9f }, /* East Asian */
	{ 58.3f, 61.7f }, /* South Asian */
	{ 60.4f, 68.3f }, /* African */
};

const char *Gccfer_MethodName( gccfer_method_t method )
{
	switch ( method ) {
	case GCCFER_METHOD_CULTURE_AGNOSTIC: return "culture_agnostic";
	case GCCFER_METHOD_RANDOM_EMBED: return "random_embed";
	case GCCFER_METHOD_ONEHOT: return "onehot";
	case GCCFER_METHOD_SEPARATE_HEADS: return "separate_heads";
	case GCCFER_METHOD_AU_CONCAT_FIXED: return "au_concat_fixed";
	case GCCFER_METHOD_CAFER: return "cafer";
	default: return "unknown";
	}
}

const gccfer_benchmark_row_t *Gccfer_GccferBenchmarks( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( gccfer_table ) / sizeof( gccfer_table[0] ) );
	}
	return gccfer_table;
}

const gccfer_benchmark_row_t *Gccfer_DfewBenchmarks( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( dfew_table ) / sizeof( dfew_table[0] ) );
	}
	return dfew_table;
}

gccfer_metrics_t Gccfer_LookupPerCulture( gccfer_culture_t culture )
{
	if ( culture >= 0 && culture < GCCFER_NUM_CULTURES ) {
		return per_culture[culture];
	}
	{
		gccfer_metrics_t zero = { 0.0f, 0.0f };
		return zero;
	}
}

void Gccfer_ModelEvaluate( gccfer_method_t method, gccfer_metrics_t *out )
{
	size_t i;

	if ( !out ) {
		return;
	}

	memset( out, 0, sizeof( *out ) );
	for ( i = 0; i < sizeof( gccfer_table ) / sizeof( gccfer_table[0] ); i++ ) {
		if ( gccfer_table[i].method == method ) {
			*out = gccfer_table[i].metrics;
			return;
		}
	}
}
