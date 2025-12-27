#ifndef __TR_FLARES_ENHANCED_H__
#define __TR_FLARES_ENHANCED_H__

#include "tr_local.h"

#define MAX_FLARE_ELEMENTS		8
#define MAX_ENHANCED_FLARES		512

// Flare element types
typedef enum {
	FLARE_ELEMENT_CORE,		// Core bright spot
	FLARE_ELEMENT_GHOST,	// Ghost flare (larger, dimmer)
	FLARE_ELEMENT_HALO,		// Halo ring
	FLARE_ELEMENT_STREAK,	// Streak/ray
	FLARE_ELEMENT_SPARKLE	// Sparkle effect
} flareElementType_t;

// Flare element structure
typedef struct {
	flareElementType_t	type;
	float				size;			// Size multiplier
	float				distance;		// Distance from center (0.0 = center)
	float				angle;			// Angle for streaks (degrees)
	vec3_t				color;			// Element color
	float				intensity;		// Intensity multiplier
	qhandle_t			shader;			// Custom shader (-1 = use default)
	float				rotationSpeed;	// Rotation speed (degrees/sec)
	float				rotation;		// Current rotation
	qboolean			occluded;		// Is this element occluded?
} flareElement_t;

// Enhanced flare structure
typedef struct {
	// Basic properties
	vec3_t				origin;
	vec3_t				color;
	float				intensity;
	qhandle_t			shader;			// Base shader
	
	// Multi-element support
	flareElement_t		elements[MAX_FLARE_ELEMENTS];
	int					numElements;
	
	// Occlusion
	qboolean			occluded;
	float				occlusionFactor;	// 0.0 = fully occluded, 1.0 = fully visible
	float				lastOcclusionTest;
	int					occlusionSamples;	// Number of samples for occlusion
	
	// Animation
	float				pulseSpeed;
	float				pulsePhase;
	float				pulseMin;
	float				pulseMax;
	
	// Fade
	float				fadeInTime;
	float				fadeOutTime;
	float				currentFade;
	
	// State
	qboolean			active;
	int					addedFrame;
	portalView_t		portalView;
	int					frameSceneNum;
	int					fogNum;
	
	// Screen space
	int					windowX, windowY;
	float				eyeZ;
	float				drawZ;
	float				drawIntensity;
	
	// Scripting
	int					scriptId;		// Lua script ID (-1 = none)
} flareEnhanced_t;

// Enhanced flare system functions
void	R_InitFlaresEnhanced(void);
void	R_ShutdownFlaresEnhanced(void);
void	R_ClearFlaresEnhanced(void);

// Flare management
int		R_CreateFlareEnhanced(const vec3_t origin, const vec3_t color, float intensity, qhandle_t shader);
void	R_DestroyFlareEnhanced(int flareId);
void	R_SetFlareEnhancedActive(int flareId, qboolean active);
void	R_SetFlareEnhancedColor(int flareId, const vec3_t color);
void	R_SetFlareEnhancedIntensity(int flareId, float intensity);

// Flare elements
void	R_AddFlareElement(int flareId, flareElementType_t type, float size, float distance, float angle, const vec3_t color, float intensity);
void	R_RemoveFlareElement(int flareId, int elementIndex);
void	R_SetFlareElementShader(int flareId, int elementIndex, qhandle_t shader);
void	R_SetFlareElementRotation(int flareId, int elementIndex, float speed);

// Flare animation
void	R_SetFlarePulse(int flareId, float speed, float min, float max);
void	R_SetFlareFade(int flareId, float fadeIn, float fadeOut);

// Flare occlusion
void	R_SetFlareOcclusionSamples(int flareId, int samples);
qboolean R_TestFlareOcclusion(int flareId);

// Flare rendering
void	R_AddFlareEnhanced(void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal);
void	R_UpdateFlaresEnhanced(void);
void	R_RenderFlaresEnhanced(void);

// Utility
int		R_GetActiveFlareCount(void);
void	R_SetFlareOcclusionEnabled(qboolean enabled);

// CVAR declarations (extern)
extern cvar_t *r_flaresEnhanced __attribute__((visibility("hidden")));

#endif // __TR_FLARES_ENHANCED_H__

