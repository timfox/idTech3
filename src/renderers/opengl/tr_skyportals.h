#ifndef __TR_SKYPORTALS_H__
#define __TR_SKYPORTALS_H__

#include "tr_local.h"

#define MAX_SKY_PORTALS		64
#define MAX_SKY_PORTAL_PLANES	8

// Sky portal types
typedef enum {
	SKYPORTAL_STANDARD,		// Standard sky portal
	SKYPORTAL_TRANSITION,	// Transitional sky portal (fade between skies)
	SKYPORTAL_DOME,			// Dome-shaped sky portal
	SKYPORTAL_CYLINDRICAL	// Cylindrical sky portal
} skyPortalType_t;

// Sky portal structure
typedef struct {
	skyPortalType_t	type;
	vec3_t			origin;
	vec3_t			mins;
	vec3_t			maxs;
	float			radius;			// For dome/cylindrical portals
	float			height;			// For cylindrical portals
	
	// Portal planes for culling
	cplane_t		planes[MAX_SKY_PORTAL_PLANES];
	int				numPlanes;
	
	// Sky properties
	qhandle_t		skyShader;
	vec3_t			fogColor;
	float			fogDensity;
	float			fogStart;
	float			fogEnd;
	
	// Transition properties
	qboolean		hasTransition;
	float			transitionFade;	// 0.0 to 1.0
	vec3_t			transitionColor;
	
	// Culling
	qboolean		active;
	int				entityNum;		// Associated entity (-1 = none)
	
	// Performance
	int				lastFrameChecked;
	qboolean		wasVisible;
	float			visibilityFactor;	// 0.0 to 1.0
	
	// Animation
	float			rotationSpeed;
	float			rotation;
	vec3_t			scrollSpeed;	// UV scroll speed
} skyPortal_t;

// Sky portal system functions
void	R_InitSkyPortals(void);
void	R_ShutdownSkyPortals(void);
void	R_ClearSkyPortals(void);

// Sky portal management
int		R_CreateSkyPortal(skyPortalType_t type, const vec3_t origin, const vec3_t mins, const vec3_t maxs, float radius, float height);
void	R_DestroySkyPortal(int portalId);
void	R_SetSkyPortalActive(int portalId, qboolean active);
void	R_SetSkyPortalShader(int portalId, qhandle_t shader);
void	R_SetSkyPortalFog(int portalId, const vec3_t color, float density, float start, float end);
void	R_SetSkyPortalTransition(int portalId, qboolean enabled, float fade, const vec3_t color);
void	R_SetSkyPortalAnimation(int portalId, float rotationSpeed, const vec3_t scrollSpeed);
void	R_SetSkyPortalEntity(int portalId, int entityNum);

// Sky portal rendering
qboolean R_IsPointInSkyPortal(int portalId, const vec3_t point);
qboolean R_IsBoxInSkyPortal(int portalId, const vec3_t mins, const vec3_t maxs);
int		R_FindSkyPortalForPoint(const vec3_t point);
int		R_FindSkyPortalForView(const viewParms_t *viewParms);
void	R_UpdateSkyPortals(void);
void	R_RenderSkyPortal(int portalId, const viewParms_t *viewParms);
void	R_RenderSkyPortals(const viewParms_t *viewParms);

// Utility
int		R_GetActiveSkyPortalCount(void);
void	R_SetSkyPortalCulling(int portalId, const cplane_t *planes, int numPlanes);

#endif // __TR_SKYPORTALS_H__

