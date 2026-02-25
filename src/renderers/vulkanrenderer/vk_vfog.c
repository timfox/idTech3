/*
===========================================================================
Copyright (C) 2026 id Tech 3 Contributors

Volumetric fog implementation for Vulkan renderer.
Provides analytical height fog, ray-marched volumetric fog,
and froxel-based volumetric lighting.
===========================================================================
*/

#include "tr_local.h"
#include "vk_vfog.h"

static qboolean vfogInitialized = qfalse;
static vfogParams_t vfogParams;

static cvar_t *r_vfog;
static cvar_t *r_vfog_density;
static cvar_t *r_vfog_heightFalloff;
static cvar_t *r_vfog_heightOffset;
static cvar_t *r_vfog_maxDistance;
static cvar_t *r_vfog_scatter;
static cvar_t *r_vfog_anisotropy;
static cvar_t *r_vfog_absorption;
static cvar_t *r_vfog_noiseScale;
static cvar_t *r_vfog_noiseSpeed;
static cvar_t *r_vfog_windX;
static cvar_t *r_vfog_windY;
static cvar_t *r_vfog_windZ;
static cvar_t *r_vfog_colorR;
static cvar_t *r_vfog_colorG;
static cvar_t *r_vfog_colorB;
static cvar_t *r_vfog_ambient;
static cvar_t *r_vfog_temporal;
static cvar_t *r_vfog_phase;

/*
===============
VFog_RegisterCvars
===============
*/
void VFog_RegisterCvars(void) {
	r_vfog              = ri.Cvar_Get("r_vfog",              "0",    CVAR_ARCHIVE);
	r_vfog_density      = ri.Cvar_Get("r_vfog_density",      "0.02", CVAR_ARCHIVE);
	r_vfog_heightFalloff= ri.Cvar_Get("r_vfog_heightFalloff","0.04", CVAR_ARCHIVE);
	r_vfog_heightOffset = ri.Cvar_Get("r_vfog_heightOffset", "0.0",  CVAR_ARCHIVE);
	r_vfog_maxDistance   = ri.Cvar_Get("r_vfog_maxDistance",  "4000", CVAR_ARCHIVE);
	r_vfog_scatter      = ri.Cvar_Get("r_vfog_scatter",      "1.0",  CVAR_ARCHIVE);
	r_vfog_anisotropy   = ri.Cvar_Get("r_vfog_anisotropy",   "0.6",  CVAR_ARCHIVE);
	r_vfog_absorption   = ri.Cvar_Get("r_vfog_absorption",   "0.01", CVAR_ARCHIVE);
	r_vfog_noiseScale   = ri.Cvar_Get("r_vfog_noiseScale",   "0.003",CVAR_ARCHIVE);
	r_vfog_noiseSpeed   = ri.Cvar_Get("r_vfog_noiseSpeed",   "0.5",  CVAR_ARCHIVE);
	r_vfog_windX        = ri.Cvar_Get("r_vfog_windX",        "1.0",  CVAR_ARCHIVE);
	r_vfog_windY        = ri.Cvar_Get("r_vfog_windY",        "0.0",  CVAR_ARCHIVE);
	r_vfog_windZ        = ri.Cvar_Get("r_vfog_windZ",        "0.2",  CVAR_ARCHIVE);
	r_vfog_colorR       = ri.Cvar_Get("r_vfog_colorR",       "0.5",  CVAR_ARCHIVE);
	r_vfog_colorG       = ri.Cvar_Get("r_vfog_colorG",       "0.55", CVAR_ARCHIVE);
	r_vfog_colorB       = ri.Cvar_Get("r_vfog_colorB",       "0.65", CVAR_ARCHIVE);
	r_vfog_ambient      = ri.Cvar_Get("r_vfog_ambient",      "0.3",  CVAR_ARCHIVE);
	r_vfog_temporal     = ri.Cvar_Get("r_vfog_temporal",      "0.9",  CVAR_ARCHIVE);
	r_vfog_phase        = ri.Cvar_Get("r_vfog_phase",        "0.8",  CVAR_ARCHIVE);

	ri.Printf(PRINT_ALL, "Volumetric fog: cvars registered (r_vfog %s)\n",
		r_vfog->integer ? "enabled" : "disabled");
}

/*
===============
VFog_Init
===============
*/
void VFog_Init(void) {
	if (vfogInitialized) {
		return;
	}

	VFog_RegisterCvars();

	Com_Memset(&vfogParams, 0, sizeof(vfogParams));
	VFog_UpdateParams(&vfogParams);

	vfogInitialized = qtrue;
	ri.Printf(PRINT_ALL, "Volumetric fog system initialized\n");
}

/*
===============
VFog_Shutdown
===============
*/
void VFog_Shutdown(void) {
	if (!vfogInitialized) {
		return;
	}

	vfogInitialized = qfalse;
	ri.Printf(PRINT_ALL, "Volumetric fog system shut down\n");
}

/*
===============
VFog_UpdateParams

Pull latest cvar values into the vfogParams structure.
===============
*/
void VFog_UpdateParams(vfogParams_t *params) {
	if (!params) return;

	params->enabled        = r_vfog ? (float)r_vfog->integer : 0.0f;
	params->density        = r_vfog_density ? r_vfog_density->value : 0.02f;
	params->heightFalloff  = r_vfog_heightFalloff ? r_vfog_heightFalloff->value : 0.04f;
	params->heightOffset   = r_vfog_heightOffset ? r_vfog_heightOffset->value : 0.0f;
	params->maxDistance    = r_vfog_maxDistance ? r_vfog_maxDistance->value : 4000.0f;
	params->scatterIntensity = r_vfog_scatter ? r_vfog_scatter->value : 1.0f;
	params->scatterAnisotropy = r_vfog_anisotropy ? r_vfog_anisotropy->value : 0.6f;
	params->absorptionCoeff = r_vfog_absorption ? r_vfog_absorption->value : 0.01f;
	params->noiseScale     = r_vfog_noiseScale ? r_vfog_noiseScale->value : 0.003f;
	params->noiseSpeed     = r_vfog_noiseSpeed ? r_vfog_noiseSpeed->value : 0.5f;
	params->windDirX       = r_vfog_windX ? r_vfog_windX->value : 1.0f;
	params->windDirY       = r_vfog_windY ? r_vfog_windY->value : 0.0f;
	params->windDirZ       = r_vfog_windZ ? r_vfog_windZ->value : 0.2f;
	params->fogColorR      = r_vfog_colorR ? r_vfog_colorR->value : 0.5f;
	params->fogColorG      = r_vfog_colorG ? r_vfog_colorG->value : 0.55f;
	params->fogColorB      = r_vfog_colorB ? r_vfog_colorB->value : 0.65f;
	params->ambientIntensity = r_vfog_ambient ? r_vfog_ambient->value : 0.3f;
	params->temporalBlend  = r_vfog_temporal ? r_vfog_temporal->value : 0.9f;
	params->phaseG         = r_vfog_phase ? r_vfog_phase->value : 0.8f;
}

/*
===============
VFog_IsEnabled
===============
*/
qboolean VFog_IsEnabled(void) {
	return r_vfog && r_vfog->integer > 0;
}

/*
===============
VFog_RenderRayMarch

Phase 2: Ray-marched volumetric fog as a fullscreen post-process pass.
Marches rays from camera through the scene, sampling depth and noise
to accumulate fog density and in-scattering.

This function is called between the main scene render and the bloom pass.
===============
*/
void VFog_RenderRayMarch(void) {
	if (!VFog_IsEnabled() || !vfogInitialized) {
		return;
	}

	VFog_UpdateParams(&vfogParams);

	/* placeholder: full GPU implementation requires:
	   1. volumetric_fog.frag shader compiled and bound
	   2. depth buffer bound as input texture
	   3. push constants with camera + fog parameters
	   4. fullscreen triangle draw

	   The shader performs VFOG_RAY_STEPS steps from camera to scene depth,
	   accumulating:
	     - exponential height fog density
	     - 3D noise perturbation for turbulence
	     - Henyey-Greenstein phase function for anisotropic scattering
	     - beer-lambert absorption
	*/
}

/*
===============
VFog_ComputeFroxels

Phase 3: Compute shader froxel scatter.
Dispatches compute shaders to scatter light into a 3D froxel grid,
then integrates front-to-back for volumetric lighting.
===============
*/
void VFog_ComputeFroxels(void) {
	if (!VFog_IsEnabled() || !vfogInitialized) {
		return;
	}

	if (!r_vfog || r_vfog->integer < 3) {
		return;
	}

	/* placeholder: compute pipeline implementation requires:
	   1. 3D image (VFOG_FROXEL_WIDTH x VFOG_FROXEL_HEIGHT x VFOG_FROXEL_DEPTH)
	   2. scatter compute shader to inject light
	   3. integration compute shader (front-to-back accumulation)
	   4. sampling in the main rendering pass
	*/
}

/*
===============
VFog_ApplyFog

Apply volumetric fog to the final image.
Called during the post-processing pipeline.
===============
*/
void VFog_ApplyFog(void) {
	if (!VFog_IsEnabled() || !vfogInitialized) {
		return;
	}

	if (r_vfog->integer >= 2) {
		VFog_RenderRayMarch();
	}

	if (r_vfog->integer >= 3) {
		VFog_ComputeFroxels();
	}
}
