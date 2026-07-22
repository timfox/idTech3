/*
 * Unit test: irradiance / reflection probe priority selection.
 *
 * Contract from vk_indirect_light.h + reflection hierarchy docs:
 * prefer in-radius probes, higher priority flag, nearer distance, newer generation.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

#define PROBE_FLAG_SPECULAR  1u
#define PROBE_FLAG_IRRADIANCE 2u

typedef struct {
	float pos[3];
	float radius;
	uint32_t generation;
	uint32_t flags;
} probe_t;

static float probe_dist_sq( const float a[3], const float b[3] )
{
	float dx = a[0] - b[0];
	float dy = a[1] - b[1];
	float dz = a[2] - b[2];
	return dx * dx + dy * dy + dz * dz;
}

static int select_probe( const probe_t *probes, int count, const float samplePos[3], uint32_t wantFlags )
{
	int best = -1;
	float bestScore = 1e30f;
	int i;

	for ( i = 0; i < count; i++ ) {
		float distSq;
		float score;
		uint32_t flags = probes[i].flags;

		if ( ( flags & wantFlags ) == 0u ) {
			continue;
		}
		distSq = probe_dist_sq( samplePos, probes[i].pos );
		if ( distSq > probes[i].radius * probes[i].radius ) {
			continue;
		}
		score = distSq - (float)probes[i].generation * 1e-4f;
		if ( ( flags & PROBE_FLAG_SPECULAR ) != 0u ) {
			score -= 0.01f;
		}
		if ( score < bestScore ) {
			bestScore = score;
			best = i;
		}
	}
	return best;
}

int main( void )
{
	const float p[3] = { 0.0f, 0.0f, 0.0f };
	probe_t probes[3] = {
		{ { 10.0f, 0.0f, 0.0f }, 8.0f, 1u, PROBE_FLAG_IRRADIANCE },
		{ { 2.0f, 0.0f, 0.0f }, 8.0f, 1u, PROBE_FLAG_IRRADIANCE | PROBE_FLAG_SPECULAR },
		{ { 1.0f, 0.0f, 0.0f }, 8.0f, 2u, PROBE_FLAG_IRRADIANCE }
	};

	ASSERT( select_probe( probes, 3, p, PROBE_FLAG_IRRADIANCE ) == 2,
		"nearest in-radius irradiance probe wins" );

	ASSERT( select_probe( probes, 3, p, PROBE_FLAG_SPECULAR ) == 1,
		"specular selection requires specular flag" );

	probes[1].radius = 1.0f;
	ASSERT( select_probe( probes, 3, p, PROBE_FLAG_SPECULAR ) == -1,
		"no specular probe when OOB" );
	ASSERT( select_probe( probes, 3, p, PROBE_FLAG_IRRADIANCE ) == 2,
		"irradiance falls back to nearest in-radius probe" );

	printf( "unit_probe_selection: PASS\n" );
	return 0;
}
