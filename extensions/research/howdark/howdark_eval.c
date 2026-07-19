/*
===========================================================================
How Dark is Dark — THR/TIS/Rs interpolation + perceptual ranking.
===========================================================================
*/

#include "howdark/howdark_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

float HowDark_InterpTheta( const float *samples, float thetaI_deg )
{
	int i;
	float t0, t1, u;

	if ( !samples ) {
		return 0.0f;
	}
	if ( thetaI_deg <= howdark_theta_i[0] ) {
		return samples[0];
	}
	if ( thetaI_deg >= howdark_theta_i[HOWDARK_THETA_SAMPLES - 1] ) {
		return samples[HOWDARK_THETA_SAMPLES - 1];
	}

	for ( i = 0; i < HOWDARK_THETA_SAMPLES - 1; i++ ) {
		t0 = howdark_theta_i[i];
		t1 = howdark_theta_i[i + 1];
		if ( thetaI_deg >= t0 && thetaI_deg <= t1 ) {
			u = ( thetaI_deg - t0 ) / ( t1 - t0 );
			return samples[i] * ( 1.0f - u ) + samples[i + 1] * u;
		}
	}
	return samples[HOWDARK_THETA_SAMPLES - 1];
}

float HowDark_THR( int id, float thetaI_deg )
{
	if ( id < 0 || id >= HOWDARK_MATERIAL_COUNT ) {
		return 0.0f;
	}
	return HowDark_InterpTheta( howdark_curves[id].thr, thetaI_deg );
}

float HowDark_TIS( int id, float thetaI_deg )
{
	if ( id < 0 || id >= HOWDARK_MATERIAL_COUNT ) {
		return 0.0f;
	}
	return HowDark_InterpTheta( howdark_curves[id].tis, thetaI_deg );
}

float HowDark_Specular( int id, float thetaI_deg )
{
	if ( id < 0 || id >= HOWDARK_MATERIAL_COUNT ) {
		return 0.0f;
	}
	return HowDark_InterpTheta( howdark_curves[id].rs, thetaI_deg );
}

float HowDark_PerceivedDarkness( int id, int intensityScale )
{
	int idx;

	if ( id < 0 || id >= HOWDARK_MATERIAL_COUNT ) {
		return 0.0f;
	}
	if ( intensityScale <= 1 ) {
		idx = 0;
	} else if ( intensityScale <= 10 ) {
		idx = 1;
	} else {
		idx = 2;
	}
	return howdark_curves[id].darkness[idx];
}

int HowDark_RankByDarkness( int intensityScale, int *outIds, int outCap )
{
	int i, j;
	int ids[HOWDARK_MATERIAL_COUNT];
	float scores[HOWDARK_MATERIAL_COUNT];
	int n;

	if ( !outIds || outCap <= 0 ) {
		return 0;
	}

	n = HOWDARK_MATERIAL_COUNT;
	for ( i = 0; i < n; i++ ) {
		ids[i] = i;
		scores[i] = HowDark_PerceivedDarkness( i, intensityScale );
	}

	/* Descending darkness score (darkest first). Stable bubble for n=6. */
	for ( i = 0; i < n - 1; i++ ) {
		for ( j = 0; j < n - 1 - i; j++ ) {
			if ( scores[j] < scores[j + 1] ) {
				float ts = scores[j];
				int ti = ids[j];
				scores[j] = scores[j + 1];
				ids[j] = ids[j + 1];
				scores[j + 1] = ts;
				ids[j + 1] = ti;
			}
		}
	}

	if ( n > outCap ) {
		n = outCap;
	}
	memcpy( outIds, ids, (size_t)n * sizeof( int ) );
	return n;
}
