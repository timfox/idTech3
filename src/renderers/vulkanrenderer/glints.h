#ifndef TR_GLINTS_H
#define TR_GLINTS_H

#include <stddef.h>
#include <stdint.h>

#define GLINT_DICT_MAX_ENTRIES 192
#define GLINT_DICT_MAX_LEVELS 16
#define GLINT_DICT_MAX_SIZE 64

typedef struct glint_dict_params_s {
	int entries;
	int levels;
	int size;
	float alpha;
	float lobeSigma;
	int mode;
	uint32_t seed;
} glint_dict_params_t;

size_t R_Glints_CalcDictionarySize(const glint_dict_params_t *params);
void R_Glints_GenerateDictionary(const glint_dict_params_t *params, float *out);
int R_Glints_GetSampleCount(const glint_dict_params_t *params);

#endif // TR_GLINTS_H
