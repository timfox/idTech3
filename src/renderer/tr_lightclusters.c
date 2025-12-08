/*
===========================================================================
Light Clustering Stub (legacy GL renderer)

Placeholder for CPU light clustering/tiling. Currently a no-op; will be
populated with binning logic feeding clustered/forward+ shaders.
===========================================================================
*/

#include "tr_local.h"
#include "../renderercommon/tr_lightclusters.h"
#include <math.h>

extern cvar_t *r_clusteredLight;

// Config: tile size and Z slices
static const int lc_tileSize = 16; // pixels
static const int lc_slicesZ = 16;  // depth slices

// Temporary CPU buffers
static lc_cluster_header_t lc_headers[LC_MAX_CLUSTERS];
static int lc_indices[LC_MAX_CLUSTERS * LC_MAX_LIGHTS_PER_CLUSTER];
static GLuint lcHeaderBuffer = 0;
static GLuint lcIndexBuffer = 0;

// Compute grid dimensions for current viewport
static lc_grid_params_t LC_ComputeGrid(void) {
	lc_grid_params_t g = {0};
	const int width = glConfig.vidWidth;
	const int height = glConfig.vidHeight;
	g.tilesX = (width  + lc_tileSize - 1) / lc_tileSize;
	g.tilesY = (height + lc_tileSize - 1) / lc_tileSize;
	g.slicesZ = lc_slicesZ;
	g.zNear = r_znear->value;
	g.zFar = tr.viewParms.zFar;
	// log2 slicing helper
	g.invLogZ = 1.0f / logf(g.zFar / g.zNear);
	if (g.tilesX * g.tilesY * g.slicesZ > LC_MAX_CLUSTERS) {
		// clamp to safety cap
		const float scale = sqrtf((float)LC_MAX_CLUSTERS / (float)(g.tilesX * g.tilesY * g.slicesZ));
		g.tilesX = (int)(g.tilesX * scale);
		g.tilesY = (int)(g.tilesY * scale);
		if (g.tilesX < 1) g.tilesX = 1;
		if (g.tilesY < 1) g.tilesY = 1;
	}
	return g;
}

// Map depth to slice using logarithmic distribution
static int LC_DepthToSlice(const lc_grid_params_t *g, float viewZ) {
	const float z = -viewZ; // view space forward is -Z
	if (z <= g->zNear) return 0;
	if (z >= g->zFar) return g->slicesZ - 1;
	const float n = logf(z / g->zNear) * g->invLogZ; // 0..1
	int slice = (int)(n * g->slicesZ);
	if (slice < 0) slice = 0;
	if (slice >= g->slicesZ) slice = g->slicesZ - 1;
	return slice;
}

// Project a point to screen; returns false if behind the near plane
static qboolean LC_ProjectToScreen(const vec3_t viewPos, int *outX, int *outY) {
	// Simple perspective projection using current projection matrix
	const float *m = tr.viewParms.projectionMatrix;
	const float x = viewPos[0], y = viewPos[1], z = viewPos[2];
	const float clipX = m[0]*x + m[4]*y + m[8]*z + m[12];
	const float clipY = m[1]*x + m[5]*y + m[9]*z + m[13];
	const float clipW = m[3]*x + m[7]*y + m[11]*z + m[15];
	if (clipW >= 0.0f) {
		return qfalse; // behind eye
	}
	const float invW = 1.0f / clipW;
	const float ndcX = clipX * invW;
	const float ndcY = clipY * invW;
	*outX = (int)((ndcX * 0.5f + 0.5f) * glConfig.vidWidth);
	*outY = (int)((ndcY * 0.5f + 0.5f) * glConfig.vidHeight);
	return qtrue;
}

static void LC_InitBuffers( void ) {
	static qboolean initialized = qfalse;
	if ( initialized ) {
		return;
	}
	qglGenBuffers( 1, &lcHeaderBuffer );
	qglGenBuffers( 1, &lcIndexBuffer );
	initialized = qtrue;
}

void R_BuildLightClusters( void ) {
	if ( !tr.registered || tr.refdef.num_dlights <= 0 ) {
		return;
	}

	if ( !r_clusteredLight || !r_clusteredLight->integer ) {
		return;
	}

	const lc_grid_params_t grid = LC_ComputeGrid();
	const int clusterCount = grid.tilesX * grid.tilesY * grid.slicesZ;
	if (clusterCount <= 0) {
		return;
	}

	// Reset headers
	for (int i = 0; i < clusterCount; ++i) {
		lc_headers[i].lightOffset = i * LC_MAX_LIGHTS_PER_CLUSTER;
		lc_headers[i].lightCount = 0;
	}

	// For each light, determine affected cluster bounds and append
	for (int li = 0; li < tr.refdef.num_dlights && li < LC_MAX_LIGHTS; ++li) {
		const dlight_t *dl = &tr.refdef.dlights[li];

		// Transform to view space (without mutating source)
		vec3_t vpos;
		for (int a = 0; a < 3; ++a) {
			vpos[a] = DotProduct(dl->origin, tr.orientation.axis[a]) - tr.orientation.origin[a];
		}

		// Depth slice range
		const float radius = dl->radius;
		const float zMin = vpos[2] - radius;
		const float zMax = vpos[2] + radius;
		int sliceMin = LC_DepthToSlice(&grid, zMax);
		int sliceMax = LC_DepthToSlice(&grid, zMin);
		if (sliceMax < 0 || sliceMin >= grid.slicesZ) {
			continue; // out of range
		}
		if (sliceMin < 0) sliceMin = 0;
		if (sliceMax >= grid.slicesZ) sliceMax = grid.slicesZ - 1;

		// Screen-space bounds: project center and approximate radius in screen space
		int sx = 0, sy = 0;
		if ( !LC_ProjectToScreen(vpos, &sx, &sy) ) {
			continue;
		}
		if (vpos[2] >= -grid.zNear) {
			continue; // behind or too close
		}

		// Approximate screen radius (pixels) using projection matrix scale
		const float invZ = -1.0f / vpos[2];
		const float projScaleX = tr.viewParms.projectionMatrix[0];
		const float projScaleY = tr.viewParms.projectionMatrix[5];
		const float srX = radius * projScaleX * invZ * 0.5f * glConfig.vidWidth;
		const float srY = radius * projScaleY * invZ * 0.5f * glConfig.vidHeight;

		const int minX = Com_Clamp(0, glConfig.vidWidth  - 1, (int)floorf(sx - srX));
		const int maxX = Com_Clamp(0, glConfig.vidWidth  - 1, (int)ceilf (sx + srX));
		const int minY = Com_Clamp(0, glConfig.vidHeight - 1, (int)floorf(sy - srY));
		const int maxY = Com_Clamp(0, glConfig.vidHeight - 1, (int)ceilf (sy + srY));

		const int tileMinX = Com_Clamp(0, grid.tilesX - 1, minX / lc_tileSize);
		const int tileMaxX = Com_Clamp(0, grid.tilesX - 1, maxX / lc_tileSize);
		const int tileMinY = Com_Clamp(0, grid.tilesY - 1, minY / lc_tileSize);
		const int tileMaxY = Com_Clamp(0, grid.tilesY - 1, maxY / lc_tileSize);

		for (int z = sliceMin; z <= sliceMax; ++z) {
			for (int ty = tileMinY; ty <= tileMaxY; ++ty) {
				for (int tx = tileMinX; tx <= tileMaxX; ++tx) {
					const int clusterIndex = (z * grid.tilesY + ty) * grid.tilesX + tx;
					lc_cluster_header_t *hdr = &lc_headers[clusterIndex];
					if (hdr->lightCount < LC_MAX_LIGHTS_PER_CLUSTER) {
						lc_indices[ hdr->lightOffset + hdr->lightCount ] = li;
						++hdr->lightCount;
					}
				}
			}
		}
	}

	// Upload to GPU buffers
	LC_InitBuffers();

	qglBindBuffer( GL_SHADER_STORAGE_BUFFER, lcHeaderBuffer );
	qglBufferData( GL_SHADER_STORAGE_BUFFER, clusterCount * sizeof( lc_cluster_header_t ), lc_headers, GL_DYNAMIC_DRAW );
	qglBindBufferBase( GL_SHADER_STORAGE_BUFFER, 6, lcHeaderBuffer ); // binding slot 6 (arbitrary)

	const int indexCount = clusterCount * LC_MAX_LIGHTS_PER_CLUSTER;
	qglBindBuffer( GL_SHADER_STORAGE_BUFFER, lcIndexBuffer );
	qglBufferData( GL_SHADER_STORAGE_BUFFER, indexCount * sizeof( int ), lc_indices, GL_DYNAMIC_DRAW );
	qglBindBufferBase( GL_SHADER_STORAGE_BUFFER, 7, lcIndexBuffer ); // binding slot 7 (arbitrary)

	qglBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
}



