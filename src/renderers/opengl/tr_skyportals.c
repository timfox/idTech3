/*
===========================================================================
Sky Portal System
Enhanced portal system for large outdoor environments with better culling
===========================================================================
*/

#include "tr_local.h"
#include "tr_skyportals.h"

static skyPortal_t skyPortals[MAX_SKY_PORTALS];
static int numSkyPortals = 0;
static cvar_t *r_skyPortals;
static cvar_t *r_skyPortalCulling;

/*
===================
R_InitSkyPortals
===================
*/
void R_InitSkyPortals(void)
{
	int i;
	
	Com_Memset(skyPortals, 0, sizeof(skyPortals));
	numSkyPortals = 0;
	
	for (i = 0; i < MAX_SKY_PORTALS; i++) {
		skyPortals[i].entityNum = -1;
		skyPortals[i].skyShader = 0;
		skyPortals[i].active = qfalse;
	}
	
	r_skyPortals = ri.Cvar_Get("r_skyPortals", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_skyPortals, "Enable sky portal system");
	}
	
	r_skyPortalCulling = ri.Cvar_Get("r_skyPortalCulling", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_skyPortalCulling, "Enable sky portal culling optimization");
	}
	
	ri.Printf(PRINT_ALL, "Sky portal system initialized\n");
}

/*
===================
R_ShutdownSkyPortals
===================
*/
void R_ShutdownSkyPortals(void)
{
	R_ClearSkyPortals();
}

/*
===================
R_ClearSkyPortals
===================
*/
void R_ClearSkyPortals(void)
{
	Com_Memset(skyPortals, 0, sizeof(skyPortals));
	numSkyPortals = 0;
}

/*
===================
R_CreateSkyPortal
===================
*/
int R_CreateSkyPortal(skyPortalType_t type, const vec3_t origin, const vec3_t mins, const vec3_t maxs, float radius, float height)
{
	int i;
	skyPortal_t *portal;
	
	for (i = 0; i < MAX_SKY_PORTALS; i++) {
		if (!skyPortals[i].active) {
			portal = &skyPortals[i];
			Com_Memset(portal, 0, sizeof(skyPortal_t));
			
			portal->type = type;
			VectorCopy(origin, portal->origin);
			VectorCopy(mins, portal->mins);
			VectorCopy(maxs, portal->maxs);
			portal->radius = radius;
			portal->height = height;
			
			portal->numPlanes = 0;
			portal->skyShader = 0;
			VectorSet(portal->fogColor, 0.5f, 0.5f, 0.5f);
			portal->fogDensity = 0.0f;
			portal->fogStart = 0.0f;
			portal->fogEnd = 0.0f;
			
			portal->hasTransition = qfalse;
			portal->transitionFade = 0.0f;
			VectorClear(portal->transitionColor);
			
			portal->active = qtrue;
			portal->entityNum = -1;
			portal->lastFrameChecked = -1;
			portal->wasVisible = qfalse;
			portal->visibilityFactor = 1.0f;
			
			portal->rotationSpeed = 0.0f;
			portal->rotation = 0.0f;
			VectorClear(portal->scrollSpeed);
			
			// Generate planes based on type
			R_SetSkyPortalCulling(i, NULL, 0);
			
			numSkyPortals++;
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_DestroySkyPortal
===================
*/
void R_DestroySkyPortal(int portalId)
{
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	if (skyPortals[portalId].active) {
		skyPortals[portalId].active = qfalse;
		numSkyPortals--;
	}
}

/*
===================
R_SetSkyPortalActive
===================
*/
void R_SetSkyPortalActive(int portalId, qboolean active)
{
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	skyPortals[portalId].active = active;
}

/*
===================
R_SetSkyPortalShader
===================
*/
void R_SetSkyPortalShader(int portalId, qhandle_t shader)
{
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	skyPortals[portalId].skyShader = shader;
}

/*
===================
R_SetSkyPortalFog
===================
*/
void R_SetSkyPortalFog(int portalId, const vec3_t color, float density, float start, float end)
{
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	VectorCopy(color, skyPortals[portalId].fogColor);
	skyPortals[portalId].fogDensity = density;
	skyPortals[portalId].fogStart = start;
	skyPortals[portalId].fogEnd = end;
}

/*
===================
R_SetSkyPortalTransition
===================
*/
void R_SetSkyPortalTransition(int portalId, qboolean enabled, float fade, const vec3_t color)
{
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	skyPortals[portalId].hasTransition = enabled;
	skyPortals[portalId].transitionFade = fade;
	if (enabled) {
		VectorCopy(color, skyPortals[portalId].transitionColor);
	}
}

/*
===================
R_SetSkyPortalAnimation
===================
*/
void R_SetSkyPortalAnimation(int portalId, float rotationSpeed, const vec3_t scrollSpeed)
{
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	skyPortals[portalId].rotationSpeed = rotationSpeed;
	VectorCopy(scrollSpeed, skyPortals[portalId].scrollSpeed);
}

/*
===================
R_SetSkyPortalEntity
===================
*/
void R_SetSkyPortalEntity(int portalId, int entityNum)
{
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	skyPortals[portalId].entityNum = entityNum;
}

/*
===================
R_SetSkyPortalCulling
===================
*/
void R_SetSkyPortalCulling(int portalId, const cplane_t *planes, int numPlanes)
{
	skyPortal_t *portal;
	int i;
	
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	portal = &skyPortals[portalId];
	
	if (planes && numPlanes > 0) {
		portal->numPlanes = (numPlanes > MAX_SKY_PORTAL_PLANES) ? MAX_SKY_PORTAL_PLANES : numPlanes;
		for (i = 0; i < portal->numPlanes; i++) {
			portal->planes[i] = planes[i];
		}
	} else {
		// Generate default planes based on portal type
		cplane_t defaultPlanes[6];
		vec3_t normal;
		
		portal->numPlanes = 0;
		
		switch (portal->type) {
			case SKYPORTAL_STANDARD:
			case SKYPORTAL_TRANSITION:
				// Box portal - 6 planes
				portal->numPlanes = 6;
				
				// Top
				VectorSet(normal, 0, 0, 1);
				VectorCopy(normal, defaultPlanes[0].normal);
				defaultPlanes[0].dist = DotProduct(portal->origin, normal) + portal->maxs[2];
				
				// Bottom
				VectorSet(normal, 0, 0, -1);
				VectorCopy(normal, defaultPlanes[1].normal);
				defaultPlanes[1].dist = DotProduct(portal->origin, normal) - portal->mins[2];
				
				// Front
				VectorSet(normal, 0, 1, 0);
				VectorCopy(normal, defaultPlanes[2].normal);
				defaultPlanes[2].dist = DotProduct(portal->origin, normal) + portal->maxs[1];
				
				// Back
				VectorSet(normal, 0, -1, 0);
				VectorCopy(normal, defaultPlanes[3].normal);
				defaultPlanes[3].dist = DotProduct(portal->origin, normal) - portal->mins[1];
				
				// Right
				VectorSet(normal, 1, 0, 0);
				VectorCopy(normal, defaultPlanes[4].normal);
				defaultPlanes[4].dist = DotProduct(portal->origin, normal) + portal->maxs[0];
				
				// Left
				VectorSet(normal, -1, 0, 0);
				VectorCopy(normal, defaultPlanes[5].normal);
				defaultPlanes[5].dist = DotProduct(portal->origin, normal) - portal->mins[0];
				
				for (i = 0; i < portal->numPlanes; i++) {
					portal->planes[i] = defaultPlanes[i];
				}
				break;
				
			case SKYPORTAL_DOME:
				// Dome portal - single plane (bottom)
				portal->numPlanes = 1;
				VectorSet(normal, 0, 0, -1);
				VectorCopy(normal, portal->planes[0].normal);
				portal->planes[0].dist = DotProduct(portal->origin, normal) - portal->radius;
				break;
				
			case SKYPORTAL_CYLINDRICAL:
				// Cylindrical portal - 2 planes (top and bottom)
				portal->numPlanes = 2;
				
				// Top
				VectorSet(normal, 0, 0, 1);
				VectorCopy(normal, portal->planes[0].normal);
				portal->planes[0].dist = DotProduct(portal->origin, normal) + portal->height;
				
				// Bottom
				VectorSet(normal, 0, 0, -1);
				VectorCopy(normal, portal->planes[1].normal);
				portal->planes[1].dist = DotProduct(portal->origin, normal);
				break;
		}
	}
}

/*
===================
R_IsPointInSkyPortal
===================
*/
qboolean R_IsPointInSkyPortal(int portalId, const vec3_t point)
{
	skyPortal_t *portal;
	vec3_t diff;
	float dist;
	int i;
	
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return qfalse;
	}
	
	portal = &skyPortals[portalId];
	
	if (!portal->active) {
		return qfalse;
	}
	
	switch (portal->type) {
		case SKYPORTAL_STANDARD:
		case SKYPORTAL_TRANSITION:
			// Check if point is inside box
			if (point[0] < portal->origin[0] + portal->mins[0] ||
				point[0] > portal->origin[0] + portal->maxs[0] ||
				point[1] < portal->origin[1] + portal->mins[1] ||
				point[1] > portal->origin[1] + portal->maxs[1] ||
				point[2] < portal->origin[2] + portal->mins[2] ||
				point[2] > portal->origin[2] + portal->maxs[2]) {
				return qfalse;
			}
			break;
			
		case SKYPORTAL_DOME:
			// Check if point is inside dome
			VectorSubtract(point, portal->origin, diff);
			dist = VectorLength(diff);
			if (dist > portal->radius || diff[2] < 0) {
				return qfalse;
			}
			break;
			
		case SKYPORTAL_CYLINDRICAL:
			// Check if point is inside cylinder
			VectorSubtract(point, portal->origin, diff);
			dist = sqrt(diff[0] * diff[0] + diff[1] * diff[1]);
			if (dist > portal->radius || diff[2] < 0 || diff[2] > portal->height) {
				return qfalse;
			}
			break;
	}
	
	// Check against culling planes
	for (i = 0; i < portal->numPlanes; i++) {
		float d = DotProduct(point, portal->planes[i].normal) - portal->planes[i].dist;
		if (d < 0) {
			return qfalse;
		}
	}
	
	return qtrue;
}

/*
===================
R_IsBoxInSkyPortal
===================
*/
qboolean R_IsBoxInSkyPortal(int portalId, const vec3_t mins, const vec3_t maxs)
{
	vec3_t corners[8];
	int i;
	
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return qfalse;
	}
	
	// Generate box corners
	corners[0][0] = mins[0]; corners[0][1] = mins[1]; corners[0][2] = mins[2];
	corners[1][0] = maxs[0]; corners[1][1] = mins[1]; corners[1][2] = mins[2];
	corners[2][0] = mins[0]; corners[2][1] = maxs[1]; corners[2][2] = mins[2];
	corners[3][0] = maxs[0]; corners[3][1] = maxs[1]; corners[3][2] = mins[2];
	corners[4][0] = mins[0]; corners[4][1] = mins[1]; corners[4][2] = maxs[2];
	corners[5][0] = maxs[0]; corners[5][1] = mins[1]; corners[5][2] = maxs[2];
	corners[6][0] = mins[0]; corners[6][1] = maxs[1]; corners[6][2] = maxs[2];
	corners[7][0] = maxs[0]; corners[7][1] = maxs[1]; corners[7][2] = maxs[2];
	
	// Check if any corner is inside portal
	for (i = 0; i < 8; i++) {
		if (R_IsPointInSkyPortal(portalId, corners[i])) {
			return qtrue;
		}
	}
	
	return qfalse;
}

/*
===================
R_FindSkyPortalForPoint
===================
*/
int R_FindSkyPortalForPoint(const vec3_t point)
{
	int i;
	
	for (i = 0; i < MAX_SKY_PORTALS; i++) {
		if (skyPortals[i].active && R_IsPointInSkyPortal(i, point)) {
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_FindSkyPortalForView
===================
*/
int R_FindSkyPortalForView(const viewParms_t *viewParms)
{
	vec3_t viewOrigin;
	
	if (!viewParms) {
		return -1;
	}
	
	VectorCopy(viewParms->or.origin, viewOrigin);
	return R_FindSkyPortalForPoint(viewOrigin);
}

/*
===================
R_UpdateSkyPortals
===================
*/
void R_UpdateSkyPortals(void)
{
	int i;
	skyPortal_t *portal;
	float deltaTime;
	static int lastSkyPortalUpdateTime = 0;
	
	if (!r_skyPortals || r_skyPortals->integer == 0) {
		return;
	}
	
	if (lastSkyPortalUpdateTime == 0) {
		lastSkyPortalUpdateTime = tr.refdef.time;
	}
	deltaTime = (tr.refdef.time - lastSkyPortalUpdateTime) / 1000.0f;
	lastSkyPortalUpdateTime = tr.refdef.time;
	
	for (i = 0; i < MAX_SKY_PORTALS; i++) {
		portal = &skyPortals[i];
		
		if (!portal->active) {
			continue;
		}
		
		// Update entity position if attached
		if (portal->entityNum >= 0) {
			// TODO: Update portal origin from entity
		}
		
		// Update rotation
		if (portal->rotationSpeed != 0.0f) {
			portal->rotation += portal->rotationSpeed * deltaTime;
			if (portal->rotation >= 360.0f) {
				portal->rotation -= 360.0f;
			}
		}
	}
}

/*
===================
R_RenderSkyPortal
===================
*/
void R_RenderSkyPortal(int portalId, const viewParms_t *viewParms)
{
	(void)viewParms; // Unused parameter - kept for API compatibility
	skyPortal_t *portal;
	
	if (portalId < 0 || portalId >= MAX_SKY_PORTALS) {
		return;
	}
	
	portal = &skyPortals[portalId];
	
	if (!portal->active) {
		return;
	}
	
	// TODO: Implement sky portal rendering
	// This would involve:
	// 1. Setting up sky shader
	// 2. Applying fog if configured
	// 3. Rendering sky geometry based on portal type
	// 4. Applying transitions if enabled
	// 5. Applying rotation/scroll animations
}

/*
===================
R_RenderSkyPortals
===================
*/
void R_RenderSkyPortals(const viewParms_t *viewParms)
{
	int portalId;
	
	if (!r_skyPortals || r_skyPortals->integer == 0) {
		return;
	}
	
	// Find portal for current view
	portalId = R_FindSkyPortalForView(viewParms);
	if (portalId >= 0) {
		R_RenderSkyPortal(portalId, viewParms);
	}
}

/*
===================
R_GetActiveSkyPortalCount
===================
*/
int R_GetActiveSkyPortalCount(void)
{
	return numSkyPortals;
}

