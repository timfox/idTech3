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
	GLINT_DICT_MAX_LOBES = 1 << (GLINT_DICT_MAX_LEVELS + 1),
	GLINT_DICT_MAX_SAMPLES = 4096
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

static float Glint_GetBaseSigma(const glint_dict_params_t *params)
{
	return params->alpha / (float)M_SQRT2;
}

static float Glint_GetDomain(const glint_dict_params_t *params)
{
	return 4.0f * Glint_GetBaseSigma(params);
}

static int Glint_ComputeSampleCount(const glint_dict_params_t *params)
{
	int count;

	if (!params) {
		return 0;
	}

	count = params->size * 8;
	if (count < 128) {
		count = 128;
	}
	if (count > GLINT_DICT_MAX_SAMPLES) {
		count = GLINT_DICT_MAX_SAMPLES;
	}
	return count;
}

static float Glint_ComputeMinSigma(const glint_dict_params_t *params)
{
	const float baseSigma = Glint_GetBaseSigma(params);
	const float domain = Glint_GetDomain(params);
	float minSigma = params->lobeSigma;
	float sampleStep = 0.0f;

	if (params->size > 1) {
		sampleStep = domain / (float)(params->size - 1);
	}

	if (minSigma < 0.0f) {
		minSigma = 0.0f;
	}

	if (sampleStep > 0.0f) {
		minSigma = fmaxf(minSigma, sampleStep * 0.5f);
	}

	if (baseSigma > 0.0f) {
		minSigma = fminf(minSigma, baseSigma);
	}

	return minSigma;
}

static float Glint_ComputeSigmaForLevel(const glint_dict_params_t *params, int level)
{
	const float baseSigma = Glint_GetBaseSigma(params);
	const float minSigma = Glint_ComputeMinSigma(params);
	const int levels = params->levels;
	const int j = (levels - 1) - level;
	float sigma = baseSigma;

	if (levels > 1) {
		sigma = baseSigma / sqrtf(powf(2.0f, (float)j));
	}

	if (sigma < minSigma) {
		sigma = minSigma;
	}

	return sigma;
}

static void Glint_ComputeTargetDistribution(const glint_dict_params_t *params, float *out)
{
	const float sigma = Glint_GetBaseSigma(params);
	const float domain = Glint_GetDomain(params);
	const int size = params->size;
	for (int i = 0; i < size; i++) {
		const float x = ((float)i / (float)(size - 1)) * domain;
		const float value = expf(-(x * x) / (2.0f * sigma * sigma));
		out[i] = value;
	}
	Glint_NormalizeDistribution(out, size);
}

static void Glint_GenerateEntry_Legacy(int entry, const glint_dict_params_t *params, float *out)
{
	float lobes[GLINT_DICT_MAX_LOBES];
	float accum[GLINT_DICT_MAX_SIZE];
	float target[GLINT_DICT_MAX_SIZE];
	int prevCount = 0;
	const float sigma = Glint_GetBaseSigma(params);
	const float domain = Glint_GetDomain(params);
	const int levels = params->levels;
	const int size = params->size;

	Glint_Seed(params->seed ^ (uint32_t)entry * 0x9E3779B9u);

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

static void Glint_GenerateEntry_Chermain(int entry, const glint_dict_params_t *params, float *out)
{
	float accum[GLINT_DICT_MAX_SIZE];
	float target[GLINT_DICT_MAX_SIZE];
	float samples[GLINT_DICT_MAX_SAMPLES];
	const float domain = Glint_GetDomain(params);
	const int levels = params->levels;
	const int size = params->size;
	const int sampleCount = Glint_ComputeSampleCount(params);

	Glint_Seed(params->seed ^ (uint32_t)entry * 0x9E3779B9u);

	for (int i = 0; i < sampleCount; i++) {
		const float sample = fabsf(Glint_RandNormal(Glint_GetBaseSigma(params)));
		samples[i] = fminf(sample, domain);
	}

	memset(accum, 0, sizeof(accum));
	Glint_ComputeTargetDistribution(params, target);

	for (int level = 0; level < levels; level++) {
		const float sigma = Glint_ComputeSigmaForLevel(params, level);

		for (int i = 0; i < sampleCount; i++) {
			const float center = samples[i];
			for (int x = 0; x < size; x++) {
				const float pos = ((float)x / (float)(size - 1)) * domain;
				const float dx = pos - center;
				accum[x] += expf(-(dx * dx) / (2.0f * sigma * sigma));
			}
		}

		{
			float *row = out + (size_t)level * size;
			for (int i = 0; i < size; i++) {
				row[i] = accum[i];
			}
			Glint_NormalizeDistribution(row, size);
		}
	}

	// Blend the final level towards the Beckmann target to stabilize energy.
	{
		float *row = out + (size_t)(levels - 1) * size;
		const float blend = 0.5f;
		for (int i = 0; i < size; i++) {
			row[i] = row[i] * (1.0f - blend) + target[i] * blend;
		}
		Glint_NormalizeDistribution(row, size);
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
		if (params->mode >= 2) {
			Glint_GenerateEntry_Chermain(entry, params, out + offset);
		} else {
			Glint_GenerateEntry_Legacy(entry, params, out + offset);
		}
	}
}

int R_Glints_GetSampleCount(const glint_dict_params_t *params)
{
	return Glint_ComputeSampleCount(params);
}
