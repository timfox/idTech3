#include "glints.h"

#include <math.h>
#include <stdint.h>

static uint32_t glint_rand_state;
static float glint_dictionary[GLINT_DICT_ENTRIES * GLINT_DICT_LEVELS * GLINT_DICT_SIZE];
static int glint_initialized = 0;

static uint32_t Glint_RandUInt(void) {
	glint_rand_state = glint_rand_state * 1664525u + 1013904223u;
	return glint_rand_state;
}

static float Glint_RandFloat(void) {
	return (Glint_RandUInt() & 0x00FFFFFF) / (float)0x01000000;
}

static void Glint_Seed(void) {
	glint_rand_state = 0xC0FFEE;
}

static void Glint_GenerateEntry(int entry, int level, float *out)
{
	(void)entry;
	(void)level;
	const float sigma = 0.02f * (1.0f + 0.25f * level);
	float sum = 0.0f;

	for (int i = 0; i < GLINT_DICT_SIZE; i++) {
		const float x = (float)i / (GLINT_DICT_SIZE - 1);
		const float distance = (x - 0.5f);
		const float base = expf(-(distance * distance) / (2.0f * sigma * sigma));
		const float jitter = (Glint_RandFloat() - 0.5f) * 0.1f;
		const float value = fmaxf(base * (1.0f + jitter), 1e-5f);
		out[i] = value;
		sum += value;
	}

	if (sum > 0.0f) {
		const float inv = 1.0f / sum;
		for (int i = 0; i < GLINT_DICT_SIZE; i++) {
			out[i] *= inv;
		}
	}
}

void R_Glints_InitDictionary(void)
{
	Glint_Seed();
	for (int entry = 0; entry < GLINT_DICT_ENTRIES; entry++) {
		for (int level = 0; level < GLINT_DICT_LEVELS; level++) {
			size_t offset = (size_t)entry * GLINT_DICT_LEVELS * GLINT_DICT_SIZE + level * GLINT_DICT_SIZE;
			Glint_GenerateEntry(entry, level, &glint_dictionary[offset]);
		}
	}
	glint_initialized = 1;
}

void R_Glints_ShutdownDictionary(void)
{
	glint_initialized = 0;
}

float *R_Glints_GetPackedDictionary(size_t *outSize)
{
	if (outSize) {
		*outSize = sizeof(glint_dictionary);
	}
	return glint_initialized ? glint_dictionary : NULL;
}
