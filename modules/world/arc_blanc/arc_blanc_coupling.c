/*
 * Fluid-solid and solid-fluid coupling (Arc Blanc §4–5).
 */
#include "arc_blanc_internal.h"
#include "qcommon.h"
#include <math.h>
#include <string.h>

#define AB_RHO_AIR   1.204f

static void ab_vec3_sub( const float a[3], const float b[3], float out[3] )
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

static float ab_vec3_len( const float v[3] )
{
	return sqrtf( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
}

static void ab_waterline_add_point( abHull_t *hull, const abVec3_t *p )
{
	int i;

	if ( !hull || !p || hull->waterlineCount >= AB_MAX_WATERLINE ) {
		return;
	}
	for ( i = 0; i < hull->waterlineCount; i++ ) {
		const float dx = hull->waterline[i].v[0] - p->v[0];
		const float dz = hull->waterline[i].v[2] - p->v[2];
		if ( dx * dx + dz * dz < 0.01f ) {
			return;
		}
	}
	hull->waterline[hull->waterlineCount++] = *p;
}

static void ab_waterline_add_edge( abHull_t *hull, float d0, const abVec3_t *v0, float d1, const abVec3_t *v1 )
{
	abVec3_t p;
	float t;

	if ( d0 * d1 >= 0.0f ) {
		return;
	}
	t = d0 / ( d0 - d1 );
	p.v[0] = v0->v[0] + t * ( v1->v[0] - v0->v[0] );
	p.v[1] = v0->v[1] + t * ( v1->v[1] - v0->v[1] );
	p.v[2] = v0->v[2] + t * ( v1->v[2] - v0->v[2] );
	ab_waterline_add_point( hull, &p );
}

static qboolean ab_point_in_waterline_poly( float x, float z, const abHull_t *hull )
{
	int i, j, crossings = 0;

	if ( !hull || hull->waterlineCount < 3 ) {
		return qfalse;
	}

	for ( i = 0; i < hull->waterlineCount; i++ ) {
		const float x0 = hull->waterline[i].v[0];
		const float z0 = hull->waterline[i].v[2];
		j = ( i + 1 ) % hull->waterlineCount;
		{
			const float x1 = hull->waterline[j].v[0];
			const float z1 = hull->waterline[j].v[2];
			if ( ( z0 <= z && z1 > z ) || ( z1 <= z && z0 > z ) ) {
				const float t = ( z - z0 ) / ( z1 - z0 );
				const float ix = x0 + t * ( x1 - x0 );
				if ( ix > x ) {
					crossings++;
				}
			}
		}
	}
	return ( crossings & 1 ) != 0;
}

static float ab_mask_height_model( const abHull_t *hull, float lx, float lz, float hf, float hb )
{
	float cx, cz, bx, bz, a, yaw, rx, rz, fx, gz;

	cx = 0.5f * ( hull->boundsMin[0] + hull->boundsMax[0] );
	cz = 0.5f * ( hull->boundsMin[2] + hull->boundsMax[2] );
	bx = hull->boundsMax[0] - hull->boundsMin[0];
	bz = hull->boundsMax[2] - hull->boundsMin[2];
	if ( bx < 1e-3f ) {
		bx = 1.0f;
	}
	if ( bz < 1e-3f ) {
		bz = 1.0f;
	}

	yaw = atan2f( hull->velocity[0], hull->velocity[2] );
	rx = cosf( yaw ) * ( lx - cx ) - sinf( yaw ) * ( lz - cz );
	rz = sinf( yaw ) * ( lx - cx ) + cosf( yaw ) * ( lz - cz );

	a = ( hf - hb ) / bz;
	fx = fabsf( rx ) / bx;
	gz = a * ( rz + bz * 0.5f ) + hb;
	return hull->maskBw * ( fx + gz );
}

static void ab_triangle_area_normal( const abVec3_t *v0, const abVec3_t *v1, const abVec3_t *v2,
	float *area, abVec3_t *normal )
{
	float e1[3], e2[3], n[3];
	ab_vec3_sub( v1->v, v0->v, e1 );
	ab_vec3_sub( v2->v, v0->v, e2 );
	CrossProduct( e1, e2, n );
	*area = 0.5f * ab_vec3_len( n );
	if ( *area > 1e-8f ) {
		VectorNormalize( n );
		normal->v[0] = n[0];
		normal->v[1] = n[1];
		normal->v[2] = n[2];
	} else {
		VectorClear( normal->v );
	}
}

void AB_Coupling_UpdateHullGeometry( abHull_t *hull, const abOceanState_t *ocean )
{
	int i;
	vec3_t sum;
	float totalVol = 0.0f;

	if ( !hull || !hull->active || !ocean ) {
		return;
	}

	VectorClear( sum );
	VectorClear( hull->immersionCenter );
	hull->submergedVolume = 0.0f;
	hull->waterlineCount = 0;

	for ( i = 0; i < hull->triangleCount; i++ ) {
		abTriangle_t *tri = &hull->triangles[i];
		float d0, d1, d2, area, centroid[3], depth;
		abVec3_t normal;

		d0 = AB_Ocean_SampleHeightWorld( ocean, tri->v0.v[0], tri->v0.v[2] ) - tri->v0.v[1];
		d1 = AB_Ocean_SampleHeightWorld( ocean, tri->v1.v[0], tri->v1.v[2] ) - tri->v1.v[1];
		d2 = AB_Ocean_SampleHeightWorld( ocean, tri->v2.v[0], tri->v2.v[2] ) - tri->v2.v[1];

		ab_triangle_area_normal( &tri->v0, &tri->v1, &tri->v2, &area, &normal );
		tri->area = area;
		tri->normal = normal;

		centroid[0] = ( tri->v0.v[0] + tri->v1.v[0] + tri->v2.v[0] ) / 3.0f;
		centroid[1] = ( tri->v0.v[1] + tri->v1.v[1] + tri->v2.v[1] ) / 3.0f;
		centroid[2] = ( tri->v0.v[2] + tri->v1.v[2] + tri->v2.v[2] ) / 3.0f;
		depth = ( d0 + d1 + d2 ) / 3.0f;
		tri->centroidDepth = depth;

		if ( d0 < 0.0f && d1 < 0.0f && d2 < 0.0f ) {
			float contrib = area * depth * normal.v[1];
			tri->submergedArea = area;
			hull->submergedVolume += contrib;
			VectorMA( sum, contrib, centroid, sum );
			totalVol += contrib;
		} else if ( d0 >= 0.0f && d1 >= 0.0f && d2 >= 0.0f ) {
			tri->submergedArea = 0.0f;
		} else {
			tri->submergedArea = area * 0.5f;
			{
				float contrib = tri->submergedArea * depth * normal.v[1];
				hull->submergedVolume += contrib;
				VectorMA( sum, contrib, centroid, sum );
				totalVol += contrib;
			}
			ab_waterline_add_edge( hull, d0, &tri->v0, d1, &tri->v1 );
			ab_waterline_add_edge( hull, d1, &tri->v1, d2, &tri->v2 );
			ab_waterline_add_edge( hull, d2, &tri->v2, d0, &tri->v0 );
		}
	}

	if ( totalVol > 1e-6f ) {
		VectorScale( sum, 1.0f / totalVol, hull->immersionCenter );
	}
}

void AB_Coupling_ComputeForces( abHull_t *hull, const abOceanState_t *ocean )
{
	int i;
	vec3_t waterForce, airForce;

	if ( !hull || !hull->active || !ocean ) {
		return;
	}

	VectorClear( waterForce );
	VectorClear( airForce );
	VectorClear( hull->forceBuoyancy );

	{
		const float rho = AB_Spectrum_WaterDensity( hull->immersionCenter[1] );
		hull->forceBuoyancy[1] = hull->submergedVolume * rho * AB_GRAVITY;
	}

	for ( i = 0; i < hull->triangleCount; i++ ) {
		const abTriangle_t *tri = &hull->triangles[i];
		vec3_t center, fluidVel, relVel, drag;
		float speed, areaProj;

		center[0] = ( tri->v0.v[0] + tri->v1.v[0] + tri->v2.v[0] ) / 3.0f;
		center[1] = ( tri->v0.v[1] + tri->v1.v[1] + tri->v2.v[1] ) / 3.0f;
		center[2] = ( tri->v0.v[2] + tri->v1.v[2] + tri->v2.v[2] ) / 3.0f;

		if ( tri->submergedArea > 0.0f ) {
			AB_Ocean_SampleVelocityWorld( ocean, center[0], center[1], center[2], fluidVel );
			ab_vec3_sub( hull->velocity, fluidVel, relVel );
			speed = ab_vec3_len( relVel );
			areaProj = tri->submergedArea;
			if ( speed > 1e-4f ) {
				const float rho = AB_Spectrum_WaterDensity( center[1] );
				float scale = -0.5f * hull->cdWater * rho * areaProj * speed;
				drag[0] = scale * relVel[0];
				drag[1] = scale * relVel[1];
				drag[2] = scale * relVel[2];
				VectorAdd( waterForce, drag, waterForce );
			}
		} else if ( tri->area > 0.0f ) {
			ab_vec3_sub( hull->velocity, vec3_origin, relVel );
			speed = ab_vec3_len( relVel );
			areaProj = tri->area;
			if ( speed > 1e-4f ) {
				float scale = -0.5f * hull->cdAir * AB_RHO_AIR * areaProj * speed;
				drag[0] = scale * relVel[0];
				drag[1] = scale * relVel[1];
				drag[2] = scale * relVel[2];
				VectorAdd( airForce, drag, airForce );
			}
		}
	}

	VectorCopy( waterForce, hull->forceWater );
	VectorCopy( airForce, hull->forceAir );
}

static float ab_fdm_grid_sample( const float *grid, int n, int i, int j )
{
	if ( i < 0 || i >= n || j < 0 || j >= n ) {
		return 0.0f;
	}
	return grid[j * n + i];
}

void AB_Coupling_StepFDM( abHull_t *hull, const abOceanState_t *ocean, float dt )
{
	const float d0 = 0.98f;
	const float dMax = 0.999f;
	const float vMax = 5.0f;
	float speed, damp, a, c, delta;
	int n = AB_FDM_GRID;
	int i, j;
	float *next;
	float shiftXt, shiftZt, shiftXo, shiftZo;
	static float scratch[AB_FDM_GRID * AB_FDM_GRID];

	(void)ocean;
	if ( !hull || !hull->active || dt <= 0.0f ) {
		return;
	}

	speed = ab_vec3_len( hull->velocity );
	{
		float u = speed / vMax;
		if ( u < 0.0f ) {
			u = 0.0f;
		}
		if ( u > 1.0f ) {
			u = 1.0f;
		}
		damp = ( 1.0f - u ) * d0 + u * dMax;
	}

	delta = hull->fdmZoneSize / (float)n;
	if ( speed < 1.0f ) {
		delta = fminf( delta, 0.999f * dt );
	} else {
		delta = fminf( delta, powf( speed, 0.999f ) * dt );
	}
	c = sqrtf( 0.49f ) * delta / dt;
	a = ( c * c * dt * dt ) / ( delta * delta );

	shiftXt = ( hull->origin[0] - hull->fdmPrevOrigin[0] ) / delta;
	shiftZt = ( hull->origin[2] - hull->fdmPrevOrigin[2] ) / delta;
	shiftXo = ( hull->origin[0] - hull->fdmPrev2Origin[0] ) / delta;
	shiftZo = ( hull->origin[2] - hull->fdmPrev2Origin[2] ) / delta;

	next = scratch;
	for ( j = 1; j < n - 1; j++ ) {
		for ( i = 1; i < n - 1; i++ ) {
			const int idx = j * n + i;
			const int k = i - (int)floorf( shiftXt );
			const int l = j - (int)floorf( shiftZt );
			const int o = i - (int)floorf( shiftXo );
			const int p = j - (int)floorf( shiftZo );
			const float center = ab_fdm_grid_sample( hull->fdmGrid, n, k, l );
			const float lap = ab_fdm_grid_sample( hull->fdmGrid, n, k - 1, l )
				+ ab_fdm_grid_sample( hull->fdmGrid, n, k + 1, l )
				+ ab_fdm_grid_sample( hull->fdmGrid, n, k, l - 1 )
				+ ab_fdm_grid_sample( hull->fdmGrid, n, k, l + 1 )
				- 4.0f * center;
			const float prev = ab_fdm_grid_sample( hull->fdmPrev, n, o, p );

			next[idx] = damp * ( a * lap + 2.0f * center - prev );
		}
	}

	memcpy( hull->fdmPrev, hull->fdmGrid, sizeof( hull->fdmGrid ) );
	memcpy( hull->fdmGrid, next, sizeof( hull->fdmGrid ) );

	/* inject mask */
	for ( j = 0; j < n; j++ ) {
		for ( i = 0; i < n; i++ ) {
			const int idx = j * n + i;
			if ( hull->fdmMask[idx] != 0.0f ) {
				hull->fdmGrid[idx] += hull->fdmMask[idx];
				hull->fdmMask[idx] = 0.0f;
			}
		}
	}

	VectorCopy( hull->fdmPrevOrigin, hull->fdmPrev2Origin );
	VectorCopy( hull->origin, hull->fdmPrevOrigin );
}

float AB_Coupling_SampleWakeHeight( const abHull_t *hull, float worldX, float worldZ )
{
	int n = AB_FDM_GRID;
	float lx, lz, u, v, halfZone;
	int x0, z0, x1, z1;
	float fx, fz, h00, h10, h01, h11;

	if ( !hull || !hull->active ) {
		return 0.0f;
	}

	halfZone = hull->fdmZoneSize * 0.5f;
	lx = worldX - ( hull->origin[0] - halfZone );
	lz = worldZ - ( hull->origin[2] - halfZone );
	if ( lx < 0.0f || lx > hull->fdmZoneSize || lz < 0.0f || lz > hull->fdmZoneSize ) {
		return 0.0f;
	}

	u = lx / hull->fdmZoneSize;
	v = lz / hull->fdmZoneSize;
	x0 = (int)( u * ( n - 1 ) );
	z0 = (int)( v * ( n - 1 ) );
	x1 = ( x0 + 1 < n ) ? x0 + 1 : n - 1;
	z1 = ( z0 + 1 < n ) ? z0 + 1 : n - 1;
	fx = u * ( n - 1 ) - (float)x0;
	fz = v * ( n - 1 ) - (float)z0;

	h00 = hull->fdmGrid[z0 * n + x0];
	h10 = hull->fdmGrid[z0 * n + x1];
	h01 = hull->fdmGrid[z1 * n + x0];
	h11 = hull->fdmGrid[z1 * n + x1];
	return ( 1.0f - fx ) * ( 1.0f - fz ) * h00 + fx * ( 1.0f - fz ) * h10
		+ ( 1.0f - fx ) * fz * h01 + fx * fz * h11;
}

void AB_Coupling_BuildMask( abHull_t *hull, const abOceanState_t *ocean )
{
	int n = AB_FDM_GRID;
	float hf, hb, speed, halfZone;
	int j, i;

	if ( !hull || !hull->active || !ocean ) {
		return;
	}

	Com_Memset( hull->fdmMask, 0, sizeof( hull->fdmMask ) );

	speed = ab_vec3_len( hull->velocity );
	hb = -0.05f;
	hf = speed * hull->hullHeight * 0.25f;
	if ( hull->hullVolume > 1e-3f ) {
		hf *= hull->submergedVolume / hull->hullVolume;
	}

	halfZone = hull->fdmZoneSize * 0.5f;

	for ( j = 0; j < n; j++ ) {
		for ( i = 0; i < n; i++ ) {
			const float wx = hull->origin[0] - halfZone + ( (float)i / (float)( n - 1 ) ) * hull->fdmZoneSize;
			const float wz = hull->origin[2] - halfZone + ( (float)j / (float)( n - 1 ) ) * hull->fdmZoneSize;
			qboolean inside = qfalse;

			if ( hull->waterlineCount >= 3 ) {
				inside = ab_point_in_waterline_poly( wx, wz, hull );
			} else if ( wx >= hull->boundsMin[0] && wx <= hull->boundsMax[0]
				&& wz >= hull->boundsMin[2] && wz <= hull->boundsMax[2] ) {
				inside = qtrue;
			}

			if ( inside ) {
				hull->fdmMask[j * n + i] = ab_mask_height_model( hull, wx, wz, hf, hb );
			}
		}
	}
}

void AB_Coupling_ApplyWakesToHeightGrid( abOceanState_t *ocean, const abHull_t *hulls, int hullCount,
	float wakeScale )
{
	int ix, iz, h, n;

	if ( !ocean || !hulls || hullCount <= 0 || wakeScale == 0.0f ) {
		return;
	}

	n = ocean->gridN;
	if ( n < 2 ) {
		return;
	}

	for ( iz = 0; iz < n; iz++ ) {
		for ( ix = 0; ix < n; ix++ ) {
			const int idx = iz * n + ix;
			const float wx = ( (float)ix / (float)( n - 1 ) ) * ocean->tileSize;
			const float wz = ( (float)iz / (float)( n - 1 ) ) * ocean->tileSize;
			float wake = 0.0f;

			for ( h = 0; h < hullCount; h++ ) {
				if ( hulls[h].active ) {
					wake += AB_Coupling_SampleWakeHeight( &hulls[h], wx, wz );
				}
			}
			ocean->combinedHeight[idx] += wakeScale * wake;
		}
	}
}
