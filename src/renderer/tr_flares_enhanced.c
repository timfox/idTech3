/*
===========================================================================
Enhanced Lens Flare System
Multi-element flares with improved occlusion and animation
===========================================================================
*/

#include "tr_local.h"
#include "tr_flares_enhanced.h"

static flareEnhanced_t flaresEnhanced[MAX_ENHANCED_FLARES];
static int numActiveFlares = 0;
static int nextFreeFlare = 0;
cvar_t *r_flaresEnhanced;
static cvar_t *r_flaresOcclusion;
static qboolean occlusionEnabled = qtrue;

/*
===================
R_InitFlaresEnhanced
===================
*/
void R_InitFlaresEnhanced(void)
{
	int i;
	
	Com_Memset(flaresEnhanced, 0, sizeof(flaresEnhanced));
	numActiveFlares = 0;
	nextFreeFlare = 0;
	
	for (i = 0; i < MAX_ENHANCED_FLARES; i++) {
		flaresEnhanced[i].shader = 0;
		flaresEnhanced[i].active = qfalse;
		flaresEnhanced[i].scriptId = -1;
		flaresEnhanced[i].numElements = 0;
		flaresEnhanced[i].occlusionSamples = 4;
		flaresEnhanced[i].fadeInTime = 0.0f;
		flaresEnhanced[i].fadeOutTime = 0.0f;
		flaresEnhanced[i].currentFade = 1.0f;
		flaresEnhanced[i].pulseSpeed = 0.0f;
		flaresEnhanced[i].pulsePhase = 0.0f;
		flaresEnhanced[i].pulseMin = 0.5f;
		flaresEnhanced[i].pulseMax = 1.0f;
	}
	
	r_flaresEnhanced = ri.Cvar_Get("r_flaresEnhanced", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_flaresEnhanced, "Enable enhanced lens flare system");
	}
	
	r_flaresOcclusion = ri.Cvar_Get("r_flaresOcclusion", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_flaresOcclusion, "Enable enhanced flare occlusion testing");
	}
	
	occlusionEnabled = qtrue;
	
	ri.Printf(PRINT_ALL, "Enhanced lens flare system initialized: %d flares\n", MAX_ENHANCED_FLARES);
}

/*
===================
R_ShutdownFlaresEnhanced
===================
*/
void R_ShutdownFlaresEnhanced(void)
{
	R_ClearFlaresEnhanced();
}

/*
===================
R_ClearFlaresEnhanced
===================
*/
void R_ClearFlaresEnhanced(void)
{
	Com_Memset(flaresEnhanced, 0, sizeof(flaresEnhanced));
	numActiveFlares = 0;
	nextFreeFlare = 0;
}

/*
===================
R_FindFreeFlareSlot
===================
*/
static int R_FindFreeFlareSlot(void)
{
	int i;
	
	for (i = 0; i < MAX_ENHANCED_FLARES; i++) {
		int idx = (nextFreeFlare + i) % MAX_ENHANCED_FLARES;
		if (!flaresEnhanced[idx].active) {
			nextFreeFlare = (idx + 1) % MAX_ENHANCED_FLARES;
			return idx;
		}
	}
	
	return -1;
}

/*
===================
R_CreateFlareEnhanced
===================
*/
int R_CreateFlareEnhanced(const vec3_t origin, const vec3_t color, float intensity, qhandle_t shader)
{
	int slot;
	flareEnhanced_t *flare;
	
	slot = R_FindFreeFlareSlot();
	if (slot < 0) {
		return -1;
	}
	
	flare = &flaresEnhanced[slot];
	Com_Memset(flare, 0, sizeof(flareEnhanced_t));
	
	VectorCopy(origin, flare->origin);
	VectorCopy(color, flare->color);
	flare->intensity = intensity;
	flare->shader = shader;
	flare->active = qtrue;
	flare->addedFrame = -1;
	flare->portalView = PV_NONE;
	flare->frameSceneNum = -1;
	flare->fogNum = 0;
	flare->occluded = qfalse;
	flare->occlusionFactor = 1.0f;
	flare->lastOcclusionTest = 0.0f;
	flare->occlusionSamples = 4;
	flare->numElements = 0;
	flare->pulseSpeed = 0.0f;
	flare->pulsePhase = 0.0f;
	flare->pulseMin = 0.5f;
	flare->pulseMax = 1.0f;
	flare->fadeInTime = 0.0f;
	flare->fadeOutTime = 0.0f;
	flare->currentFade = 1.0f;
	flare->scriptId = -1;
	
	// Add default core element
	R_AddFlareElement(slot, FLARE_ELEMENT_CORE, 1.0f, 0.0f, 0.0f, color, 1.0f);
	
	numActiveFlares++;
	return slot;
}

/*
===================
R_DestroyFlareEnhanced
===================
*/
void R_DestroyFlareEnhanced(int flareId)
{
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	if (flaresEnhanced[flareId].active) {
		flaresEnhanced[flareId].active = qfalse;
		flaresEnhanced[flareId].numElements = 0;
		numActiveFlares--;
	}
}

/*
===================
R_SetFlareEnhancedActive
===================
*/
void R_SetFlareEnhancedActive(int flareId, qboolean active)
{
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flaresEnhanced[flareId].active = active;
}

/*
===================
R_SetFlareEnhancedColor
===================
*/
void R_SetFlareEnhancedColor(int flareId, const vec3_t color)
{
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	VectorCopy(color, flaresEnhanced[flareId].color);
}

/*
===================
R_SetFlareEnhancedIntensity
===================
*/
void R_SetFlareEnhancedIntensity(int flareId, float intensity)
{
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flaresEnhanced[flareId].intensity = intensity;
}

/*
===================
R_AddFlareElement
===================
*/
void R_AddFlareElement(int flareId, flareElementType_t type, float size, float distance, float angle, const vec3_t color, float intensity)
{
	flareEnhanced_t *flare;
	flareElement_t *element;
	
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flare = &flaresEnhanced[flareId];
	
	if (flare->numElements >= MAX_FLARE_ELEMENTS) {
		return;
	}
	
	element = &flare->elements[flare->numElements];
	Com_Memset(element, 0, sizeof(flareElement_t));
	
	element->type = type;
	element->size = size;
	element->distance = distance;
	element->angle = angle;
	VectorCopy(color, element->color);
	element->intensity = intensity;
	element->shader = -1;
	element->rotationSpeed = 0.0f;
	element->rotation = 0.0f;
	element->occluded = qfalse;
	
	flare->numElements++;
}

/*
===================
R_RemoveFlareElement
===================
*/
void R_RemoveFlareElement(int flareId, int elementIndex)
{
	flareEnhanced_t *flare;
	int i;
	
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flare = &flaresEnhanced[flareId];
	
	if (elementIndex < 0 || elementIndex >= flare->numElements) {
		return;
	}
	
	// Shift elements
	for (i = elementIndex; i < flare->numElements - 1; i++) {
		flare->elements[i] = flare->elements[i + 1];
	}
	
	flare->numElements--;
}

/*
===================
R_SetFlareElementShader
===================
*/
void R_SetFlareElementShader(int flareId, int elementIndex, qhandle_t shader)
{
	flareEnhanced_t *flare;
	
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flare = &flaresEnhanced[flareId];
	
	if (elementIndex < 0 || elementIndex >= flare->numElements) {
		return;
	}
	
	flare->elements[elementIndex].shader = shader;
}

/*
===================
R_SetFlareElementRotation
===================
*/
void R_SetFlareElementRotation(int flareId, int elementIndex, float speed)
{
	flareEnhanced_t *flare;
	
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flare = &flaresEnhanced[flareId];
	
	if (elementIndex < 0 || elementIndex >= flare->numElements) {
		return;
	}
	
	flare->elements[elementIndex].rotationSpeed = speed;
}

/*
===================
R_SetFlarePulse
===================
*/
void R_SetFlarePulse(int flareId, float speed, float min, float max)
{
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flaresEnhanced[flareId].pulseSpeed = speed;
	flaresEnhanced[flareId].pulseMin = min;
	flaresEnhanced[flareId].pulseMax = max;
}

/*
===================
R_SetFlareFade
===================
*/
void R_SetFlareFade(int flareId, float fadeIn, float fadeOut)
{
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flaresEnhanced[flareId].fadeInTime = fadeIn;
	flaresEnhanced[flareId].fadeOutTime = fadeOut;
}

/*
===================
R_SetFlareOcclusionSamples
===================
*/
void R_SetFlareOcclusionSamples(int flareId, int samples)
{
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return;
	}
	
	flaresEnhanced[flareId].occlusionSamples = (samples < 1) ? 1 : ((samples > 16) ? 16 : samples);
}

/*
===================
R_TestFlareOcclusion
===================
*/
qboolean R_TestFlareOcclusion(int flareId)
{
	flareEnhanced_t *flare;
	vec3_t dir;
	float dist;
	
	if (flareId < 0 || flareId >= MAX_ENHANCED_FLARES) {
		return qfalse;
	}
	
	if (!occlusionEnabled || !r_flaresOcclusion || r_flaresOcclusion->integer == 0) {
		return qtrue;
	}
	
	flare = &flaresEnhanced[flareId];
	
	if (!flare->active) {
		return qfalse;
	}
	
	// Calculate direction from viewer to flare
	VectorSubtract(flare->origin, backEnd.viewParms.or.origin, dir);
	dist = VectorNormalize(dir);
	
	if (dist < 1.0f) {
		flare->occlusionFactor = 1.0f;
		return qtrue;
	}
	
	// TODO: Implement actual occlusion testing using depth buffer
	// For now, use simple distance-based approximation
	flare->occlusionFactor = 1.0f;
	
	flare->occluded = (flare->occlusionFactor < 0.1f);
	return !flare->occluded;
}

/*
===================
R_AddFlareEnhanced
===================
*/
void R_AddFlareEnhanced(void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal)
{
	(void)surface; // Unused parameter - kept for API compatibility
	(void)normal; // Unused parameter - kept for API compatibility
	
	flareEnhanced_t *flare;
	vec3_t local;
	float d = 1.0f;
	vec4_t eye, clip, normalized, window;
	int i;
	
	if (!r_flaresEnhanced || r_flaresEnhanced->integer == 0) {
		return;
	}
	
	backEnd.pc.c_flareAdds++;
	
	// Check normal facing
	if (normal && (normal[0] || normal[1] || normal[2])) {
		VectorSubtract(backEnd.viewParms.or.origin, point, local);
		VectorNormalizeFast(local);
		d = DotProduct(local, normal);
		
		if (d < 0) {
			return;
		}
	}
	
	// Transform to screen space
	R_TransformModelToClip(point, backEnd.or.modelMatrix, 
		backEnd.viewParms.projectionMatrix, eye, clip);
	
	// Check if off screen
	for (i = 0; i < 3; i++) {
		if (clip[i] >= clip[3] || clip[i] <= -clip[3]) {
			return;
		}
	}
	
	R_TransformClipToWindow(clip, &backEnd.viewParms, normalized, window);
	
	if (window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth ||
		window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight) {
		return;
	}
	
	// Find or create flare
	flare = NULL;
	for (i = 0; i < MAX_ENHANCED_FLARES; i++) {
		if (flaresEnhanced[i].active &&
			flaresEnhanced[i].frameSceneNum == backEnd.viewParms.frameSceneNum &&
			flaresEnhanced[i].portalView == backEnd.viewParms.portalView) {
			// Check if same position (within tolerance)
			vec3_t diff;
			VectorSubtract(flaresEnhanced[i].origin, point, diff);
			if (VectorLengthSquared(diff) < 100.0f) { // 10 units tolerance
				flare = &flaresEnhanced[i];
				break;
			}
		}
	}
	
	if (!flare) {
		int slot = R_FindFreeFlareSlot();
		if (slot < 0) {
			return;
		}
		flare = &flaresEnhanced[slot];
		Com_Memset(flare, 0, sizeof(flareEnhanced_t));
		flare->active = qtrue;
		flare->frameSceneNum = backEnd.viewParms.frameSceneNum;
		flare->portalView = backEnd.viewParms.portalView;
		flare->addedFrame = -1;
		flare->numElements = 0;
		flare->currentFade = 0.0f;
		numActiveFlares++;
	}
	
	if (flare->addedFrame != backEnd.viewParms.frameCount - 1) {
		flare->currentFade = 0.0f;
	}
	
	flare->addedFrame = backEnd.viewParms.frameCount;
	flare->fogNum = fogNum;
	
	VectorCopy(point, flare->origin);
	VectorScale(color, d, flare->color);
	
	flare->windowX = backEnd.viewParms.viewportX + (int)window[0];
	flare->windowY = backEnd.viewParms.viewportY + (int)window[1];
	flare->eyeZ = eye[2];
	
	if (backEnd.viewParms.portalView != PV_NONE) {
		flare->drawZ = (clip[2] + clip[3] - 1.5f) / (2.0f * clip[3]);
	} else {
		flare->drawZ = (clip[2] + clip[3] - 0.5f) / (2.0f * clip[3]);
	}
}

/*
===================
R_UpdateFlaresEnhanced
===================
*/
static int lastFlareUpdateTime = 0;

void R_UpdateFlaresEnhanced(void)
{
	flareEnhanced_t *flare;
	flareElement_t *element;
	int i, j;
	float deltaTime;
	float currentTime;
	
	if (!r_flaresEnhanced || r_flaresEnhanced->integer == 0) {
		return;
	}
	
	// Calculate delta time
	if (lastFlareUpdateTime > 0) {
		deltaTime = (tr.refdef.time - lastFlareUpdateTime) * 0.001f;
	} else {
		deltaTime = 0.0f;
	}
	lastFlareUpdateTime = tr.refdef.time;
	currentTime = tr.refdef.time / 1000.0f;
	
	for (i = 0; i < MAX_ENHANCED_FLARES; i++) {
		flare = &flaresEnhanced[i];
		
		if (!flare->active) {
			continue;
		}
		
		// Update pulse
		if (flare->pulseSpeed > 0.0f) {
			flare->pulsePhase += flare->pulseSpeed * deltaTime;
			if (flare->pulsePhase >= 360.0f) {
				flare->pulsePhase -= 360.0f;
			}
			float pulse = flare->pulseMin + (flare->pulseMax - flare->pulseMin) * 
				(0.5f + 0.5f * sin(flare->pulsePhase * M_PI / 180.0f));
			flare->intensity = pulse;
		}
		
		// Update fade
		if (flare->fadeInTime > 0.0f || flare->fadeOutTime > 0.0f) {
			// TODO: Implement fade timing
		}
		
		// Update element rotations
		for (j = 0; j < flare->numElements; j++) {
			element = &flare->elements[j];
			
			if (element->rotationSpeed != 0.0f) {
				element->rotation += element->rotationSpeed * deltaTime;
				if (element->rotation >= 360.0f) {
					element->rotation -= 360.0f;
				}
			}
		}
		
		// Test occlusion periodically
		if (currentTime - flare->lastOcclusionTest > 0.1f) {
			R_TestFlareOcclusion(i);
			flare->lastOcclusionTest = currentTime;
		}
	}
}

/*
===================
R_RenderFlareElement
===================
*/
static void R_RenderFlareElement(flareEnhanced_t *flare, flareElement_t *element, int windowX, int windowY, float baseSize, float baseIntensity)
{
	float size, distance, angle;
	vec3_t color;
	float intensity;
	qhandle_t shader;
	color4ub_t c;
	float cos_a, sin_a;
	float offsetX, offsetY;
	
	// Calculate element size
	size = baseSize * element->size;
	
	// Calculate distance offset
	distance = element->distance * baseSize;
	
	// Calculate angle
	angle = element->angle + element->rotation;
	cos_a = cos(angle * M_PI / 180.0f);
	sin_a = sin(angle * M_PI / 180.0f);
	
	offsetX = distance * cos_a;
	offsetY = distance * sin_a;
	
	// Calculate color and intensity
	VectorScale(element->color, element->intensity * baseIntensity * flare->occlusionFactor, color);
	intensity = element->intensity * baseIntensity * flare->occlusionFactor;
	
	if (intensity < 0.01f) {
		return; // Too dim to render
	}
	
	// Get shader
	shader = (element->shader >= 0) ? element->shader : flare->shader;
	if (shader <= 0) {
		// Use default flare shader from tr.flareShader
		if (tr.flareShader) {
			shader = tr.flareShader->index;
		} else if (tr.whiteShader) {
			shader = tr.whiteShader->index;
		} else {
			return; // No valid shader
		}
	}
	
	// Set color
	c.rgba[0] = (byte)(color[0] * 255.0f);
	c.rgba[1] = (byte)(color[1] * 255.0f);
	c.rgba[2] = (byte)(color[2] * 255.0f);
	c.rgba[3] = 255;
	
	// Render based on element type
	switch (element->type) {
		case FLARE_ELEMENT_CORE:
		case FLARE_ELEMENT_GHOST:
		case FLARE_ELEMENT_HALO:
			// Render as quad
			RB_BeginSurface(R_GetShaderByHandle(shader), flare->fogNum);
			RB_AddQuadStamp2(windowX + offsetX - size, windowY + offsetY - size,
				size * 2, size * 2, 0, 0, 1, 1, c);
			RB_EndSurface();
			break;
			
		case FLARE_ELEMENT_STREAK:
			// Render as elongated quad
			RB_BeginSurface(R_GetShaderByHandle(shader), flare->fogNum);
			RB_AddQuadStamp2(windowX + offsetX - size * 0.5f, windowY + offsetY - size * 2.0f,
				size, size * 4.0f, 0, 0, 1, 1, c);
			RB_EndSurface();
			break;
			
		case FLARE_ELEMENT_SPARKLE:
			// Render as small quad
			RB_BeginSurface(R_GetShaderByHandle(shader), flare->fogNum);
			RB_AddQuadStamp2(windowX + offsetX - size * 0.5f, windowY + offsetY - size * 0.5f,
				size, size, 0, 0, 1, 1, c);
			RB_EndSurface();
			break;
	}
}

/*
===================
R_RenderFlaresEnhanced
===================
*/
void R_RenderFlaresEnhanced(void)
{
	flareEnhanced_t *flare;
	flareElement_t *element;
	int i, j;
	float distance, size, intensity, factor;
	vec3_t scaledColor;
	
	if (!r_flaresEnhanced || r_flaresEnhanced->integer == 0) {
		return;
	}
	
	if (numActiveFlares == 0) {
		return;
	}
	
	for (i = 0; i < MAX_ENHANCED_FLARES; i++) {
		flare = &flaresEnhanced[i];
		
		if (!flare->active || flare->numElements == 0) {
			continue;
		}
		
		// Check if flare should be rendered this frame
		if (flare->addedFrame < backEnd.viewParms.frameCount - 1) {
			continue;
		}
		
		if (flare->frameSceneNum != backEnd.viewParms.frameSceneNum ||
			flare->portalView != backEnd.viewParms.portalView) {
			continue;
		}
		
		// Calculate base size and intensity
		if (flare->eyeZ > -1.0f) {
			distance = 1.0f;
		} else {
			distance = -flare->eyeZ;
		}
		
		size = backEnd.viewParms.viewportWidth * (r_flareSize->value / 640.0f + 8.0f / distance);
		factor = distance + size * sqrt(r_flareCoeff->value);
		intensity = r_flareCoeff->value * size * size / (factor * factor);
		intensity *= flare->intensity * flare->occlusionFactor * flare->currentFade;
		
		VectorScale(flare->color, intensity, scaledColor);
		
		// Render all elements
		for (j = 0; j < flare->numElements; j++) {
			element = &flare->elements[j];
			
			if (!element->occluded) {
				R_RenderFlareElement(flare, element, flare->windowX, flare->windowY, size, intensity);
			}
		}
	}
}

/*
===================
R_GetActiveFlareCount
===================
*/
int R_GetActiveFlareCount(void)
{
	return numActiveFlares;
}

/*
===================
R_SetFlareOcclusionEnabled
===================
*/
void R_SetFlareOcclusionEnabled(qboolean enabled)
{
	occlusionEnabled = enabled;
}

