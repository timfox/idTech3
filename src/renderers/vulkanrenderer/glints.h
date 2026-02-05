#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef GLINT_DICT_MAX_ENTRIES
#define GLINT_DICT_MAX_ENTRIES 512
#endif

#ifndef GLINT_DICT_MAX_LEVELS
#define GLINT_DICT_MAX_LEVELS 8
#endif

#ifndef GLINT_DICT_MAX_SIZE
#define GLINT_DICT_MAX_SIZE 128
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---- Glint dictionary API ----
//
// Parameters that control dictionary generation.
typedef struct glint_dict_params_s {
	int entries;
	int levels;
	int size;
	float alpha;
	float lobeSigma;
	int mode;
	uint32_t seed;
} glint_dict_params_t;

size_t R_Glints_CalcDictionarySize( const glint_dict_params_t *params );
void   R_Glints_GenerateDictionary( const glint_dict_params_t *params, float *out );
int    R_Glints_GetSampleCount( const glint_dict_params_t *params );

#ifdef __cplusplus
} // extern "C"
#endif
