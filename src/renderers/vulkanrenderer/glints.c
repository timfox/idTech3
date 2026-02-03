#include "glints.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

static uint32_t glint_rand_state;
static float glint_dictionary[GLINT_DICT_ENTRIES * GLINT_DICT_LEVELS * GLINT_DICT_SIZE];
static int glint_initialized = 0;

#define GLINT_DICT_ALPHA 0.5f
#define GLINT_DICT_LOBE_SIGMA 0.02f
#define GLINT_DICT_DOMAIN (2.0f * GLINT_DICT_ALPHA) // 2 * alpha, matches paper domain.

enum {
	GLINT_DICT_MAX_LOBES = 1 << (GLINT_DICT_LEVELS + 1)
};

static uint32_t Glint_RandUInt(void) {
	glint_rand_state = glint_rand_state * 1664525u + 1013904223u;
	return glint_rand_state;
}

static float Glint_RandFloat(void) {
	return (Glint_RandUInt() & 0x00FFFFFF) / (float)0x01000000;
}

static void Glint_Seed(uint32_t seed) {
	glint_rand_state = seed ? seed : 0xC0FFEE;
}

static float Glint_RandNormal(float sigma)
{
	const float u1 = fmaxf(Glint_RandFloat(), 1e-6f);
	const float u2 = Glint_RandFloat();
	const float r = sqrtf(-2.0f * logf(u1));
	const float theta = 2.0f * (float)M_PI * u2;
	return sigma * r * cosf(theta);
}

static void Glint_NormalizeDistribution(float *dist)
{
	float sum = 0.0f;
	for (int i = 0; i < GLINT_DICT_SIZE; i++) {
		sum += dist[i];
	}
	if (sum > 0.0f) {
		const float inv = 1.0f / sum;
		for (int i = 0; i < GLINT_DICT_SIZE; i++) {
			dist[i] *= inv;
		}
	}
}

static void Glint_ComputeTargetDistribution(float *out)
{
	const float sigma = GLINT_DICT_ALPHA / (float)M_SQRT2;
	for (int i = 0; i < GLINT_DICT_SIZE; i++) {
		const float x = ((float)i / (float)(GLINT_DICT_SIZE - 1)) * GLINT_DICT_DOMAIN;
		const float value = expf(-(x * x) / (2.0f * sigma * sigma));
		out[i] = value;
	}
	Glint_NormalizeDistribution(out);
}

static void Glint_GenerateEntry(int entry, float *out)
{
	float lobes[GLINT_DICT_MAX_LOBES];
	float accum[GLINT_DICT_SIZE];
	float target[GLINT_DICT_SIZE];
	int prevCount = 0;

	Glint_Seed(0xC0FFEEu ^ (uint32_t)entry * 0x9E3779B9u);

	{
		const float sigma = GLINT_DICT_ALPHA / (float)M_SQRT2;
		for (int i = 0; i < GLINT_DICT_MAX_LOBES; i++) {
			const float sample = fabsf(Glint_RandNormal(sigma));
			lobes[i] = fminf(sample, GLINT_DICT_DOMAIN);
		}
	}

	memset(accum, 0, sizeof(accum));
	Glint_ComputeTargetDistribution(target);

	for (int level = 0; level < GLINT_DICT_LEVELS; level++) {
		const int lobeCount = 1 << (level + 1);

		for (int i = prevCount; i < lobeCount; i++) {
			const float center = lobes[i];
			for (int x = 0; x < GLINT_DICT_SIZE; x++) {
				const float pos = ((float)x / (float)(GLINT_DICT_SIZE - 1)) * GLINT_DICT_DOMAIN;
				const float dx = pos - center;
				accum[x] += expf(-(dx * dx) / (2.0f * GLINT_DICT_LOBE_SIGMA * GLINT_DICT_LOBE_SIGMA));
			}
		}

		prevCount = lobeCount;

		{
			float *row = out + (size_t)level * GLINT_DICT_SIZE;
			for (int i = 0; i < GLINT_DICT_SIZE; i++) {
				row[i] = accum[i];
			}
			Glint_NormalizeDistribution(row);
		}
	}

	// Enforce convergence to the Beckmann target at the last level.
	{
		float *row = out + (size_t)(GLINT_DICT_LEVELS - 1) * GLINT_DICT_SIZE;
		for (int i = 0; i < GLINT_DICT_SIZE; i++) {
			row[i] = target[i];
		}
	}
}

void R_Glints_InitDictionary(void)
{
	for (int entry = 0; entry < GLINT_DICT_ENTRIES; entry++) {
		size_t offset = (size_t)entry * GLINT_DICT_LEVELS * GLINT_DICT_SIZE;
		Glint_GenerateEntry(entry, &glint_dictionary[offset]);
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
