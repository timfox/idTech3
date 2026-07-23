#pragma once

/*
 * Host frustum-sphere test shared by GPU visibility and unit tests.
 * No Vulkan / engine includes — pure C math.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sphere: xyz + radius.
 * planeNormals[i] · p - planeDists[i] >= -radius for all i ⇒ visible.
 * Returns 1 if visible, 0 if culled. Rejects NaN/Inf radius or center.
 */
static inline int GpuFrustum_SphereVisible( const float sphere[4],
	const float planeNormals[4][3], const float planeDists[4] )
{
	int i;
	float r;
	float cx, cy, cz;

	if ( !sphere || !planeNormals || !planeDists ) {
		return 0;
	}
	cx = sphere[0];
	cy = sphere[1];
	cz = sphere[2];
	r = sphere[3];
	/* NaN/Inf → keep visible (conservative). */
	if ( !( r == r ) || !( cx == cx ) || !( cy == cy ) || !( cz == cz ) ) {
		return 1;
	}
	if ( r < 0.0f ) {
		r = 0.0f;
	}
	/* Very large bounds: retain. */
	if ( r > 1.0e6f ) {
		return 1;
	}
	for ( i = 0; i < 4; i++ ) {
		float d = planeNormals[i][0] * cx + planeNormals[i][1] * cy +
			planeNormals[i][2] * cz - planeDists[i];
		if ( !( d == d ) ) {
			return 1;
		}
		if ( d < -r ) {
			return 0;
		}
	}
	return 1;
}

#ifdef __cplusplus
}
#endif
