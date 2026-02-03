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

enum {
	GLINT_DICT_MAX_LOBES = 1 << (GLINT_DICT_MAX_LEVELS + 1)
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

static void Glint_NormalizeDistribution(float *dist, int size)
{
	float sum = 0.0f;
	for (int i = 0; i < size; i++) {
		sum += dist[i];
	}
	if (sum > 0.0f) {
		const float inv = 1.0f / sum;
		for (int i = 0; i < size; i++) {
			dist[i] *= inv;
		}
	}
}

static void Glint_ComputeTargetDistribution(const glint_dict_params_t *params, float *out)
{
	const float sigma = params->alpha / (float)M_SQRT2;
	const float domain = 2.0f * params->alpha;
	const int size = params->size;
	for (int i = 0; i < size; i++) {
		const float x = ((float)i / (float)(size - 1)) * domain;
		const float value = expf(-(x * x) / (2.0f * sigma * sigma));
		out[i] = value;
	}
	Glint_NormalizeDistribution(out, size);
}

static void Glint_GenerateEntry(int entry, const glint_dict_params_t *params, float *out)
{
	float lobes[GLINT_DICT_MAX_LOBES];
	float accum[GLINT_DICT_MAX_SIZE];
	float target[GLINT_DICT_MAX_SIZE];
	int prevCount = 0;
	const float sigma = params->alpha / (float)M_SQRT2;
	const float domain = 2.0f * params->alpha;
	const int levels = params->levels;
	const int size = params->size;

	Glint_Seed(0xC0FFEEu ^ (uint32_t)entry * 0x9E3779B9u);

	{
		const int maxLobes = 1 << (levels + 1);
		for (int i = 0; i < maxLobes; i++) {
			const float sample = fabsf(Glint_RandNormal(sigma));
			lobes[i] = fminf(sample, domain);
		}
	}

	memset(accum, 0, sizeof(accum));
	Glint_ComputeTargetDistribution(params, target);

	for (int level = 0; level < levels; level++) {
		const int lobeCount = 1 << (level + 1);

		for (int i = prevCount; i < lobeCount; i++) {
			const float center = lobes[i];
			for (int x = 0; x < size; x++) {
				const float pos = ((float)x / (float)(size - 1)) * domain;
				const float dx = pos - center;
				accum[x] += expf(-(dx * dx) / (2.0f * params->lobeSigma * params->lobeSigma));
			}
		}

		prevCount = lobeCount;

		{
			float *row = out + (size_t)level * size;
			for (int i = 0; i < size; i++) {
				row[i] = accum[i];
			}
			Glint_NormalizeDistribution(row, size);
		}
	}

	// Enforce convergence to the Beckmann target at the last level.
	{
		float *row = out + (size_t)(levels - 1) * size;
		for (int i = 0; i < size; i++) {
			row[i] = target[i];
		}
	}
}

size_t R_Glints_CalcDictionarySize(const glint_dict_params_t *params)
{
	if (!params) {
		return 0;
	}
	return (size_t)params->entries * (size_t)params->levels * (size_t)params->size * sizeof(float);
}

void R_Glints_GenerateDictionary(const glint_dict_params_t *params, float *out)
{
	if (!params || !out) {
		return;
	}

	for (int entry = 0; entry < params->entries; entry++) {
		size_t offset = (size_t)entry * (size_t)params->levels * (size_t)params->size;
		Glint_GenerateEntry(entry, params, out + offset);
	}
}
