/*
===========================================================================
Enhanced Shader System
Procedural generation, scripting, runtime modification, Vulkan/PBR support
===========================================================================
*/

#include "tr_local.h"
#include "tr_shaders_enhanced.h"
#include <math.h>
#include <string.h>

#ifdef USE_VULKAN
#include "../vulkan/vk.h"
#endif

static proceduralShader_t proceduralShaders[MAX_PROCEDURAL_SHADERS];
static int numProceduralShaders = 0;
static cvar_t *r_shadersEnhanced;
static cvar_t *r_shadersProcedural;
static cvar_t *r_shaderCache;
static cvar_t *r_shaderCacheSize;

// Shader cache
static shaderCacheEntry_t shaderCache[MAX_SHADER_CACHE_ENTRIES];
static int numShaderCacheEntries = 0;
static int shaderCacheAccessTime = 0;

/*
===================
R_InitShadersEnhanced
===================
*/
void R_InitShadersEnhanced(void)
{
	int i;
	
	Com_Memset(proceduralShaders, 0, sizeof(proceduralShaders));
	numProceduralShaders = 0;
	
	for (i = 0; i < MAX_PROCEDURAL_SHADERS; i++) {
		proceduralShaders[i].baseShader = -1;
		proceduralShaders[i].shaderHandle = -1;
		proceduralShaders[i].active = qfalse;
		proceduralShaders[i].isPBR = qfalse;
		proceduralShaders[i].metallic = 0.0f;
		proceduralShaders[i].roughness = 0.5f;
		VectorSet(proceduralShaders[i].albedo, 1.0f, 1.0f, 1.0f);
		proceduralShaders[i].ao = 1.0f;
		VectorClear(proceduralShaders[i].emissive);
		proceduralShaders[i].scriptId = -1;
		proceduralShaders[i].numParameters = 0;
		proceduralShaders[i].numDynamicStages = 0;
	}
	
	r_shadersEnhanced = ri.Cvar_Get("r_shadersEnhanced", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_shadersEnhanced, "Enable enhanced shader system");
	}
	
	r_shadersProcedural = ri.Cvar_Get("r_shadersProcedural", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_shadersProcedural, "Enable procedural shader generation");
	}
	
	ri.Printf(PRINT_ALL, "Enhanced shader system initialized: %d procedural shaders\n", MAX_PROCEDURAL_SHADERS);
}

/*
===================
R_ShutdownShadersEnhanced
===================
*/
void R_ShutdownShadersEnhanced(void)
{
	int i;
	proceduralShader_t *pshader;
	
	for (i = 0; i < MAX_PROCEDURAL_SHADERS; i++) {
		pshader = &proceduralShaders[i];
		if (pshader->active) {
			// Free dynamic stages
			int j;
			for (j = 0; j < pshader->numDynamicStages; j++) {
				if (pshader->dynamicStages[j]) {
					Z_Free(pshader->dynamicStages[j]);
					pshader->dynamicStages[j] = NULL;
				}
			}
			pshader->numDynamicStages = 0;
		}
	}
	
	R_ClearShadersEnhanced();
	R_ClearShaderCache();
}

/*
===================
R_ClearShadersEnhanced
===================
*/
void R_ClearShadersEnhanced(void)
{
	Com_Memset(proceduralShaders, 0, sizeof(proceduralShaders));
	numProceduralShaders = 0;
}

/*
===================
R_FindFreeProceduralShaderSlot
===================
*/
static int R_FindFreeProceduralShaderSlot(void)
{
	int i;
	
	for (i = 0; i < MAX_PROCEDURAL_SHADERS; i++) {
		if (!proceduralShaders[i].active) {
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_CreateProceduralShader
===================
*/
int R_CreateProceduralShader(const char *name, qhandle_t baseShader)
{
	int slot;
	proceduralShader_t *pshader;
	shader_t *base;
	
	if (!name || !*name) {
		return -1;
	}
	
	if (!r_shadersEnhanced || r_shadersEnhanced->integer == 0) {
		return -1;
	}
	
	// Check if already exists
	slot = R_FindProceduralShader(name);
	if (slot >= 0) {
		return slot;
	}
	
	slot = R_FindFreeProceduralShaderSlot();
	if (slot < 0) {
		ri.Printf(PRINT_WARNING, "R_CreateProceduralShader: Maximum procedural shaders reached\n");
		return -1;
	}
	
	pshader = &proceduralShaders[slot];
	Com_Memset(pshader, 0, sizeof(proceduralShader_t));
	
	Q_strncpyz(pshader->name, name, sizeof(pshader->name));
	pshader->baseShader = baseShader;
	pshader->shaderHandle = -1;
	pshader->active = qtrue;
	pshader->dirty = qtrue;
	pshader->lastUpdateTime = tr.refdef.time;
	pshader->numParameters = 0;
	pshader->numDynamicStages = 0;
	pshader->scriptId = -1;
	
	// Initialize PBR defaults
	pshader->isPBR = qfalse;
	pshader->metallic = 0.0f;
	pshader->roughness = 0.5f;
	VectorSet(pshader->albedo, 1.0f, 1.0f, 1.0f);
	pshader->ao = 1.0f;
	VectorClear(pshader->emissive);
	
	// Copy base shader if provided
	if (baseShader >= 0) {
		base = R_GetShaderByHandle(baseShader);
		if (base) {
			// Copy shader properties
			pshader->shaderHandle = baseShader;
			// TODO: Copy stages and properties
		}
	}
	
	numProceduralShaders++;
	
	// Mark as dirty to trigger regeneration
	pshader->dirty = qtrue;
	
	return slot;
}

/*
===================
R_DestroyProceduralShader
===================
*/
void R_DestroyProceduralShader(int shaderId)
{
	proceduralShader_t *pshader;
	int i;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (!pshader->active) {
		return;
	}
	
	// Free dynamic stages
	for (i = 0; i < pshader->numDynamicStages; i++) {
		if (pshader->dynamicStages[i]) {
			Z_Free(pshader->dynamicStages[i]);
			pshader->dynamicStages[i] = NULL;
		}
	}
	
	pshader->active = qfalse;
	pshader->numDynamicStages = 0;
	pshader->numParameters = 0;
	numProceduralShaders--;
}

/*
===================
R_GetProceduralShaderHandle
===================
*/
qhandle_t R_GetProceduralShaderHandle(int shaderId)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return -1;
	}
	
	if (!proceduralShaders[shaderId].active) {
		return -1;
	}
	
	// Regenerate if dirty
	if (proceduralShaders[shaderId].dirty) {
		R_RegenerateProceduralShader(shaderId);
	}
	
	return proceduralShaders[shaderId].shaderHandle;
}

/*
===================
R_RegenerateProceduralShader
===================
*/
void R_RegenerateProceduralShader(int shaderId)
{
	proceduralShader_t *pshader;
	shader_t *base;
	char shaderText[8192];
	char *textPtr;
	int len;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (!pshader->active) {
		return;
	}
	
	if (!r_shadersProcedural || r_shadersProcedural->integer == 0) {
		return;
	}
	
	// Build shader text from parameters
	textPtr = shaderText;
	len = 0;
	
	// Shader header
	if (pshader->baseShader >= 0) {
		base = R_GetShaderByHandle(pshader->baseShader);
		if (base) {
			// Start with base shader name
			len += Com_sprintf(textPtr + len, sizeof(shaderText) - len, "%s\n{\n", base->name);
		}
	} else {
		len += Com_sprintf(textPtr + len, sizeof(shaderText) - len, "%s\n{\n", pshader->name);
	}
	
	// Add PBR properties if enabled
	if (pshader->isPBR) {
		len += Com_sprintf(textPtr + len, sizeof(shaderText) - len, 
			"  qer_editorimage %s\n", pshader->name);
		len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
			"  surfaceparm metal\n");
		len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
			"  surfaceparm roughness %f\n", pshader->roughness);
		len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
			"  surfaceparm metallic %f\n", pshader->metallic);
	}
	
	// Add parameters as shader properties
	int i;
	for (i = 0; i < pshader->numParameters; i++) {
		shaderParameter_t *param = &pshader->parameters[i];
		
		switch (param->type) {
			case SHADER_PARAM_FLOAT:
				len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
					"  %s %f\n", param->name, param->value.fValue);
				break;
			case SHADER_PARAM_VEC2:
				len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
					"  %s ( %f %f )\n", param->name, param->value.v2Value[0], param->value.v2Value[1]);
				break;
			case SHADER_PARAM_VEC3:
				len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
					"  %s ( %f %f %f )\n", param->name, param->value.v3Value[0], param->value.v3Value[1], param->value.v3Value[2]);
				break;
			case SHADER_PARAM_VEC4:
				len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
					"  %s ( %f %f %f %f )\n", param->name, param->value.v4Value[0], param->value.v4Value[1], param->value.v4Value[2], param->value.v4Value[3]);
				break;
			case SHADER_PARAM_INT:
				len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
					"  %s %d\n", param->name, param->value.iValue);
				break;
			case SHADER_PARAM_BOOL:
				len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
					"  %s %s\n", param->name, param->value.bValue ? "1" : "0");
				break;
			case SHADER_PARAM_TEXTURE:
				{
					shader_t *texShader = R_GetShaderByHandle(param->value.tValue);
					if (texShader) {
						len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
							"  %s %s\n", param->name, texShader->name);
					}
				}
				break;
			case SHADER_PARAM_COLOR:
				len += Com_sprintf(textPtr + len, sizeof(shaderText) - len,
					"  %s ( %f %f %f )\n", param->name, param->value.v3Value[0], param->value.v3Value[1], param->value.v3Value[2]);
				break;
		}
	}
	
	// Add dynamic stages
	for (i = 0; i < pshader->numDynamicStages; i++) {
		shaderStage_t *stage = pshader->dynamicStages[i];
		if (stage && stage->active) {
			// TODO: Convert stage to shader text
			len += Com_sprintf(textPtr + len, sizeof(shaderText) - len, "  {\n");
			// Add stage properties
			len += Com_sprintf(textPtr + len, sizeof(shaderText) - len, "  }\n");
		}
	}
	
	len += Com_sprintf(textPtr + len, sizeof(shaderText) - len, "}\n");
	
	// Generate content hash
	pshader->contentHash = R_GenerateShaderContentHash(shaderText);
	
	// Check cache
	if (r_shaderCache && r_shaderCache->integer) {
		qhandle_t cachedHandle = R_FindShaderInCache(pshader->name, pshader->contentHash);
		if (cachedHandle >= 0) {
			pshader->shaderHandle = cachedHandle;
			pshader->cached = qtrue;
			pshader->dirty = qfalse;
			pshader->lastUpdateTime = tr.refdef.time;
			return;
		}
	}
	
	// Parse and register shader using existing parser
	// Register via R_FindShader which will parse the shader text
	// Note: This requires the shader text to be in the shader text buffer
	// For full integration, we'd need to add the text to s_shaderText buffer
	// For now, register directly
	{
		shader_t *sh = R_FindShader(pshader->name, LIGHTMAP_NONE, qtrue);
		if (sh) {
			pshader->shaderHandle = sh->index;
		} else {
			pshader->shaderHandle = -1;
		}
	}
	
	// Add to cache
	if (r_shaderCache && r_shaderCache->integer && pshader->shaderHandle >= 0) {
		R_AddShaderToCache(pshader->name, pshader->contentHash, pshader->shaderHandle, shaderId);
		pshader->cached = qtrue;
	}
	
	pshader->dirty = qfalse;
	pshader->lastUpdateTime = tr.refdef.time;
}

/*
===================
R_AddShaderParameter
===================
*/
int R_AddShaderParameter(int shaderId, const char *name, shaderParamType_t type)
{
	proceduralShader_t *pshader;
	shaderParameter_t *param;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return -1;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (!pshader->active) {
		return -1;
	}
	
	if (pshader->numParameters >= MAX_SHADER_PARAMETERS) {
		return -1;
	}
	
	// Check if parameter already exists
	int i;
	for (i = 0; i < pshader->numParameters; i++) {
		if (Q_stricmp(pshader->parameters[i].name, name) == 0) {
			return i; // Return existing parameter index
		}
	}
	
	param = &pshader->parameters[pshader->numParameters];
	Com_Memset(param, 0, sizeof(shaderParameter_t));
	
	Q_strncpyz(param->name, name, sizeof(param->name));
	param->type = type;
	param->animated = qfalse;
	param->animType = SHADER_ANIM_LINEAR;
	param->animSpeed = 0.0f;
	param->animPhase = 0.0f;
	param->animMin = 0.0f;
	param->animMax = 1.0f;
	param->scriptId = -1;
	param->isPBRParam = qfalse;
	
	// Initialize default values
	switch (type) {
		case SHADER_PARAM_FLOAT:
			param->value.fValue = 0.0f;
			break;
		case SHADER_PARAM_VEC2:
			param->value.v2Value[0] = 0.0f;
			param->value.v2Value[1] = 0.0f;
			break;
		case SHADER_PARAM_VEC3:
			VectorClear(param->value.v3Value);
			break;
		case SHADER_PARAM_VEC4:
			param->value.v4Value[0] = 0.0f;
			param->value.v4Value[1] = 0.0f;
			param->value.v4Value[2] = 0.0f;
			param->value.v4Value[3] = 0.0f;
			break;
		case SHADER_PARAM_INT:
			param->value.iValue = 0;
			break;
		case SHADER_PARAM_BOOL:
			param->value.bValue = qfalse;
			break;
		case SHADER_PARAM_TEXTURE:
			param->value.tValue = 0;
			break;
		case SHADER_PARAM_COLOR:
			VectorSet(param->value.v3Value, 1.0f, 1.0f, 1.0f);
			break;
	}
	
	int paramIndex = pshader->numParameters;
	pshader->numParameters++;
	pshader->dirty = qtrue;
	
	return paramIndex;
}

/*
===================
R_RemoveShaderParameter
===================
*/
void R_RemoveShaderParameter(int shaderId, int paramIndex)
{
	proceduralShader_t *pshader;
	int i;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	// Shift parameters
	for (i = paramIndex; i < pshader->numParameters - 1; i++) {
		pshader->parameters[i] = pshader->parameters[i + 1];
	}
	
	pshader->numParameters--;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterFloat
===================
*/
void R_SetShaderParameterFloat(int shaderId, int paramIndex, float value)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	if (pshader->parameters[paramIndex].type != SHADER_PARAM_FLOAT) {
		return;
	}
	
	pshader->parameters[paramIndex].value.fValue = value;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterVec2
===================
*/
void R_SetShaderParameterVec2(int shaderId, int paramIndex, const vec2_t value)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	if (pshader->parameters[paramIndex].type != SHADER_PARAM_VEC2) {
		return;
	}
	
	pshader->parameters[paramIndex].value.v2Value[0] = value[0];
	pshader->parameters[paramIndex].value.v2Value[1] = value[1];
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterVec3
===================
*/
void R_SetShaderParameterVec3(int shaderId, int paramIndex, const vec3_t value)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	if (pshader->parameters[paramIndex].type != SHADER_PARAM_VEC3 && 
		pshader->parameters[paramIndex].type != SHADER_PARAM_COLOR) {
		return;
	}
	
	VectorCopy(value, pshader->parameters[paramIndex].value.v3Value);
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterVec4
===================
*/
void R_SetShaderParameterVec4(int shaderId, int paramIndex, const vec4_t value)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	if (pshader->parameters[paramIndex].type != SHADER_PARAM_VEC4) {
		return;
	}
	
	Vector4Copy(value, pshader->parameters[paramIndex].value.v4Value);
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterInt
===================
*/
void R_SetShaderParameterInt(int shaderId, int paramIndex, int value)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	if (pshader->parameters[paramIndex].type != SHADER_PARAM_INT) {
		return;
	}
	
	pshader->parameters[paramIndex].value.iValue = value;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterBool
===================
*/
void R_SetShaderParameterBool(int shaderId, int paramIndex, qboolean value)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	if (pshader->parameters[paramIndex].type != SHADER_PARAM_BOOL) {
		return;
	}
	
	pshader->parameters[paramIndex].value.bValue = value;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterTexture
===================
*/
void R_SetShaderParameterTexture(int shaderId, int paramIndex, qhandle_t texture)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	if (pshader->parameters[paramIndex].type != SHADER_PARAM_TEXTURE) {
		return;
	}
	
	pshader->parameters[paramIndex].value.tValue = texture;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderParameterByName
===================
*/
void R_SetShaderParameterByName(int shaderId, const char *name, shaderParamType_t type, const void *value)
{
	int paramIndex;
	
	paramIndex = R_FindShaderParameter(shaderId, name);
	if (paramIndex < 0) {
		// Create parameter if it doesn't exist
		paramIndex = R_AddShaderParameter(shaderId, name, type);
		if (paramIndex < 0) {
			return;
		}
	}
	
	switch (type) {
		case SHADER_PARAM_FLOAT:
			R_SetShaderParameterFloat(shaderId, paramIndex, *(float *)value);
			break;
		case SHADER_PARAM_VEC2:
			R_SetShaderParameterVec2(shaderId, paramIndex, (const vec_t *)value);
			break;
		case SHADER_PARAM_VEC3:
			R_SetShaderParameterVec3(shaderId, paramIndex, (const vec_t *)value);
			break;
		case SHADER_PARAM_VEC4:
			R_SetShaderParameterVec4(shaderId, paramIndex, (const vec_t *)value);
			break;
		case SHADER_PARAM_INT:
			R_SetShaderParameterInt(shaderId, paramIndex, *(int *)value);
			break;
		case SHADER_PARAM_BOOL:
			R_SetShaderParameterBool(shaderId, paramIndex, *(qboolean *)value);
			break;
		case SHADER_PARAM_TEXTURE:
			R_SetShaderParameterTexture(shaderId, paramIndex, *(qhandle_t *)value);
			break;
		case SHADER_PARAM_COLOR:
			R_SetShaderParameterVec3(shaderId, paramIndex, (const vec_t *)value);
			break;
	}
}

/*
===================
R_SetShaderParameterAnimation
===================
*/
void R_SetShaderParameterAnimation(int shaderId, int paramIndex, shaderAnimType_t type, float speed, float min, float max)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	pshader->parameters[paramIndex].animated = qtrue;
	pshader->parameters[paramIndex].animType = type;
	pshader->parameters[paramIndex].animSpeed = speed;
	pshader->parameters[paramIndex].animMin = min;
	pshader->parameters[paramIndex].animMax = max;
	pshader->parameters[paramIndex].scriptId = -1;
}

/*
===================
R_SetShaderParameterAnimationScript
===================
*/
void R_SetShaderParameterAnimationScript(int shaderId, int paramIndex, int scriptId)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (paramIndex < 0 || paramIndex >= pshader->numParameters) {
		return;
	}
	
	pshader->parameters[paramIndex].animated = qtrue;
	pshader->parameters[paramIndex].animType = SHADER_ANIM_CUSTOM;
	pshader->parameters[paramIndex].scriptId = scriptId;
}

/*
===================
R_UpdateShaderAnimations
===================
*/
void R_UpdateShaderAnimations(float deltaTime)
{
	proceduralShader_t *pshader;
	shaderParameter_t *param;
	int i, j;
	float time;
	float value;
	
	if (!r_shadersEnhanced || r_shadersEnhanced->integer == 0) {
		return;
	}
	
	time = tr.refdef.time / 1000.0f;
	(void)time;
	
	for (i = 0; i < MAX_PROCEDURAL_SHADERS; i++) {
		pshader = &proceduralShaders[i];
		
		if (!pshader->active) {
			continue;
		}
		
		for (j = 0; j < pshader->numParameters; j++) {
			param = &pshader->parameters[j];
			
			if (!param->animated) {
				continue;
			}
			
			// Update animation phase
			param->animPhase += param->animSpeed * deltaTime;
			if (param->animPhase >= 360.0f) {
				param->animPhase -= 360.0f;
			}
			
			// Calculate animated value based on type
			switch (param->animType) {
				case SHADER_ANIM_LINEAR:
					{
						float phase = param->animPhase / 360.0f;
						while (phase >= 1.0f) phase -= 1.0f;
						while (phase < 0.0f) phase += 1.0f;
						value = param->animMin + (param->animMax - param->animMin) * phase;
					}
					break;
					
				case SHADER_ANIM_SINE:
					value = param->animMin + (param->animMax - param->animMin) * 
						(0.5f + 0.5f * sin(param->animPhase * M_PI / 180.0f));
					break;
					
				case SHADER_ANIM_COSINE:
					value = param->animMin + (param->animMax - param->animMin) * 
						(0.5f + 0.5f * cos(param->animPhase * M_PI / 180.0f));
					break;
					
				case SHADER_ANIM_TRIANGLE:
					{
						float phase = fmod(param->animPhase / 360.0f, 1.0f);
						if (phase < 0.5f) {
							value = param->animMin + (param->animMax - param->animMin) * (phase * 2.0f);
						} else {
							value = param->animMax - (param->animMax - param->animMin) * ((phase - 0.5f) * 2.0f);
						}
					}
					break;
					
				case SHADER_ANIM_SAWTOOTH:
					{
						float phase = param->animPhase / 360.0f;
						while (phase >= 1.0f) phase -= 1.0f;
						while (phase < 0.0f) phase += 1.0f;
						value = param->animMin + (param->animMax - param->animMin) * phase;
					}
					break;
					
				case SHADER_ANIM_INVERSE_SAWTOOTH:
					{
						float phase = param->animPhase / 360.0f;
						while (phase >= 1.0f) phase -= 1.0f;
						while (phase < 0.0f) phase += 1.0f;
						value = param->animMax - (param->animMax - param->animMin) * phase;
					}
					break;
					
				case SHADER_ANIM_NOISE:
					// Simple noise approximation
					value = param->animMin + (param->animMax - param->animMin) * 
						(0.5f + 0.5f * sin(param->animPhase * 7.0f) * cos(param->animPhase * 11.0f));
					break;
					
				case SHADER_ANIM_CUSTOM:
					// TODO: Call Lua script function
					value = param->value.fValue; // Keep current value
					break;
					
				default:
					value = param->value.fValue;
					break;
			}
			
			// Update parameter value
			if (param->type == SHADER_PARAM_FLOAT) {
				param->value.fValue = value;
			} else if (param->type == SHADER_PARAM_VEC3 || param->type == SHADER_PARAM_COLOR) {
				// Animate all components
				param->value.v3Value[0] = value;
				param->value.v3Value[1] = value;
				param->value.v3Value[2] = value;
			}
			
			pshader->dirty = qtrue;
		}
	}
}

/*
===================
R_SetShaderPBR
===================
*/
void R_SetShaderPBR(int shaderId, qboolean enabled)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	proceduralShaders[shaderId].isPBR = enabled;
	proceduralShaders[shaderId].dirty = qtrue;
}

/*
===================
R_SetShaderMetallic
===================
*/
void R_SetShaderMetallic(int shaderId, float metallic)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	proceduralShaders[shaderId].metallic = Com_Clamp(0.0f, 1.0f, metallic);
	proceduralShaders[shaderId].dirty = qtrue;
}

/*
===================
R_SetShaderRoughness
===================
*/
void R_SetShaderRoughness(int shaderId, float roughness)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	proceduralShaders[shaderId].roughness = Com_Clamp(0.0f, 1.0f, roughness);
	proceduralShaders[shaderId].dirty = qtrue;
}

/*
===================
R_SetShaderAlbedo
===================
*/
void R_SetShaderAlbedo(int shaderId, const vec3_t albedo)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	VectorCopy(albedo, proceduralShaders[shaderId].albedo);
	proceduralShaders[shaderId].dirty = qtrue;
}

/*
===================
R_SetShaderAO
===================
*/
void R_SetShaderAO(int shaderId, float ao)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	proceduralShaders[shaderId].ao = Com_Clamp(0.0f, 1.0f, ao);
	proceduralShaders[shaderId].dirty = qtrue;
}

/*
===================
R_SetShaderEmissive
===================
*/
void R_SetShaderEmissive(int shaderId, const vec3_t emissive)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	VectorCopy(emissive, proceduralShaders[shaderId].emissive);
	proceduralShaders[shaderId].dirty = qtrue;
}

/*
===================
R_SetShaderPBRTexture
===================
*/
void R_SetShaderPBRTexture(int shaderId, qhandle_t ormTexture)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	// Add as texture parameter
	int paramIndex = R_FindShaderParameter(shaderId, "physical_texture");
	if (paramIndex < 0) {
		paramIndex = R_AddShaderParameter(shaderId, "physical_texture", SHADER_PARAM_TEXTURE);
	}
	
	if (paramIndex >= 0) {
		R_SetShaderParameterTexture(shaderId, paramIndex, ormTexture);
		proceduralShaders[shaderId].isPBR = qtrue;
	}
}

/*
===================
R_ModifyShaderStage
===================
*/
void R_ModifyShaderStage(int shaderId, int stageIndex, const shaderStage_t *stage)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages) {
		return;
	}
	
	if (pshader->dynamicStages[stageIndex]) {
		*pshader->dynamicStages[stageIndex] = *stage;
		pshader->dirty = qtrue;
	}
}

/*
===================
R_AddShaderStage
===================
*/
void R_AddShaderStage(int shaderId, const shaderStage_t *stage)
{
	proceduralShader_t *pshader;
	shaderStage_t *newStage;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (pshader->numDynamicStages >= MAX_SHADER_STAGES_DYNAMIC) {
		return;
	}
	
	newStage = (shaderStage_t *)Z_Malloc(sizeof(shaderStage_t));
	if (!newStage) {
		return;
	}
	
	*newStage = *stage;
	pshader->dynamicStages[pshader->numDynamicStages] = newStage;
	pshader->numDynamicStages++;
	pshader->dirty = qtrue;
}

/*
===================
R_RemoveShaderStage
===================
*/
void R_RemoveShaderStage(int shaderId, int stageIndex)
{
	proceduralShader_t *pshader;
	int i;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages) {
		return;
	}
	
	if (pshader->dynamicStages[stageIndex]) {
		Z_Free(pshader->dynamicStages[stageIndex]);
	}
	
	// Shift stages
	for (i = stageIndex; i < pshader->numDynamicStages - 1; i++) {
		pshader->dynamicStages[i] = pshader->dynamicStages[i + 1];
	}
	
	pshader->numDynamicStages--;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderCullType
===================
*/
void R_SetShaderCullType(int shaderId, cullType_t cullType)
{
	(void)shaderId; // Unused - TODO: Modify shader cull type
	(void)cullType; // Unused - This would require modifying the shader structure
}

/*
===================
R_SetShaderSort
===================
*/
void R_SetShaderSort(int shaderId, float sort)
{
	(void)shaderId; // Unused - TODO: Modify shader sort value
	(void)sort;
}

/*
===================
R_SetShaderPortalRange
===================
*/
void R_SetShaderPortalRange(int shaderId, float range)
{
	(void)shaderId; // Unused - TODO: Modify shader portal range
	(void)range;
}

#ifdef USE_VULKAN
#endif

/*
===================
R_FindProceduralShader
===================
*/
int R_FindProceduralShader(const char *name)
{
	int i;
	
	if (!name || !*name) {
		return -1;
	}
	
	for (i = 0; i < MAX_PROCEDURAL_SHADERS; i++) {
		if (proceduralShaders[i].active && 
			Q_stricmp(proceduralShaders[i].name, name) == 0) {
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_FindShaderParameter
===================
*/
int R_FindShaderParameter(int shaderId, const char *name)
{
	proceduralShader_t *pshader;
	int i;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return -1;
	}
	
	pshader = &proceduralShaders[shaderId];
	
	if (!name || !*name) {
		return -1;
	}
	
	for (i = 0; i < pshader->numParameters; i++) {
		if (Q_stricmp(pshader->parameters[i].name, name) == 0) {
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_UpdateProceduralShaders
===================
*/
void R_UpdateProceduralShaders(float deltaTime)
{
	int i;
	
	if (!r_shadersEnhanced || r_shadersEnhanced->integer == 0) {
		return;
	}
	
	// Update animations
	R_UpdateShaderAnimations(deltaTime);
	
	// Regenerate dirty shaders
	for (i = 0; i < MAX_PROCEDURAL_SHADERS; i++) {
		if (proceduralShaders[i].active && proceduralShaders[i].dirty) {
			R_RegenerateProceduralShader(i);
		}
	}
	
	// Update shader cache
	if (r_shaderCache && r_shaderCache->integer) {
		R_UpdateShaderCache();
	}
}

/*
===================
R_GetActiveProceduralShaderCount
===================
*/
int R_GetActiveProceduralShaderCount(void)
{
	return numProceduralShaders;
}

/*
===================
R_GenerateShaderContentHash
===================
Generate hash from shader text content
===================
*/
unsigned int R_GenerateShaderContentHash(const char *shaderText)
{
	unsigned int hash = 0;
	const char *p;
	
	if (!shaderText) {
		return 0;
	}
	
	// Simple hash function (djb2)
	for (p = shaderText; *p; p++) {
		hash = ((hash << 5) + hash) + (unsigned char)*p;
	}
	
	return hash;
}

/*
===================
R_ClearShaderCache
===================
Clear the shader cache
===================
*/
void R_ClearShaderCache(void)
{
	Com_Memset(shaderCache, 0, sizeof(shaderCache));
	numShaderCacheEntries = 0;
	shaderCacheAccessTime = 0;
}

/*
===================
R_FindShaderInCache
===================
Find shader in cache by name and content hash
===================
*/
qhandle_t R_FindShaderInCache(const char *name, unsigned int contentHash)
{
	int i;
	
	if (!r_shaderCache || r_shaderCache->integer == 0) {
		return -1;
	}
	
	if (!name || !*name) {
		return -1;
	}
	
	for (i = 0; i < numShaderCacheEntries; i++) {
		if (shaderCache[i].inUse && 
			shaderCache[i].contentHash == contentHash &&
			Q_stricmp(shaderCache[i].name, name) == 0) {
			shaderCache[i].lastAccessTime = shaderCacheAccessTime++;
			return shaderCache[i].shaderHandle;
		}
	}
	
	return -1;
}

/*
===================
R_AddShaderToCache
===================
Add shader to cache
===================
*/
void R_AddShaderToCache(const char *name, unsigned int contentHash, qhandle_t shaderHandle, int shaderId)
{
	int i, oldestIndex;
	int oldestTime;
	
	if (!r_shaderCache || r_shaderCache->integer == 0) {
		return;
	}
	
	if (!name || !*name || shaderHandle < 0) {
		return;
	}
	
	// Check if already cached
	for (i = 0; i < numShaderCacheEntries; i++) {
		if (shaderCache[i].inUse && 
			shaderCache[i].contentHash == contentHash &&
			Q_stricmp(shaderCache[i].name, name) == 0) {
			shaderCache[i].shaderHandle = shaderHandle;
			shaderCache[i].shaderId = shaderId;
			shaderCache[i].lastAccessTime = shaderCacheAccessTime++;
			return;
		}
	}
	
	// Find free slot or oldest entry
	if (numShaderCacheEntries < MAX_SHADER_CACHE_ENTRIES) {
		i = numShaderCacheEntries++;
	} else {
		// Find oldest entry
		oldestIndex = 0;
		oldestTime = shaderCache[0].lastAccessTime;
		for (i = 1; i < MAX_SHADER_CACHE_ENTRIES; i++) {
			if (shaderCache[i].lastAccessTime < oldestTime) {
				oldestTime = shaderCache[i].lastAccessTime;
				oldestIndex = i;
			}
		}
		i = oldestIndex;
	}
	
	// Add to cache
	Q_strncpyz(shaderCache[i].name, name, sizeof(shaderCache[i].name));
	shaderCache[i].contentHash = contentHash;
	shaderCache[i].shaderHandle = shaderHandle;
	shaderCache[i].shaderId = shaderId;
	shaderCache[i].lastAccessTime = shaderCacheAccessTime++;
	shaderCache[i].inUse = qtrue;
}

/*
===================
R_UpdateShaderCache
===================
Update shader cache (remove old entries if cache is full)
===================
*/
void R_UpdateShaderCache(void)
{
	int maxCacheSize;
	
	if (!r_shaderCache || r_shaderCache->integer == 0) {
		return;
	}
	
	maxCacheSize = r_shaderCacheSize ? r_shaderCacheSize->integer : MAX_SHADER_CACHE_ENTRIES;
	if (maxCacheSize < 1) {
		maxCacheSize = MAX_SHADER_CACHE_ENTRIES;
	}
	
	// Remove oldest entries if cache is too large
	while (numShaderCacheEntries > maxCacheSize) {
		int oldestIndex = 0;
		int oldestTime = shaderCache[0].lastAccessTime;
		int i;
		
		for (i = 1; i < numShaderCacheEntries; i++) {
			if (shaderCache[i].lastAccessTime < oldestTime) {
				oldestTime = shaderCache[i].lastAccessTime;
				oldestIndex = i;
			}
		}
		
		// Remove oldest entry
		shaderCache[oldestIndex] = shaderCache[numShaderCacheEntries - 1];
		numShaderCacheEntries--;
	}
}

/*
===================
R_GetShaderCacheSize
===================
Get current shader cache size
===================
*/
int R_GetShaderCacheSize(void)
{
	return numShaderCacheEntries;
}

/*
===================
R_ParseProceduralShaderText
===================
Parse shader text using existing parser and register shader
===================
*/
qboolean R_ParseProceduralShaderText(int shaderId, const char *shaderText)
{
	proceduralShader_t *pshader;
	qhandle_t shaderHandle;
	(void)shaderText; // TODO: Parse shader text
	unsigned int contentHash;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return qfalse;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return qfalse;
	}
	
	if (!shaderText || !*shaderText) {
		return qfalse;
	}
	
	// Generate content hash
	contentHash = R_GenerateShaderContentHash(shaderText);
	
	// Check cache first
	if (r_shaderCache && r_shaderCache->integer) {
		shaderHandle = R_FindShaderInCache(pshader->name, contentHash);
		if (shaderHandle >= 0) {
			pshader->shaderHandle = shaderHandle;
			pshader->contentHash = contentHash;
			pshader->cached = qtrue;
			pshader->dirty = qfalse;
			return qtrue;
		}
	}
	
	// Parse shader text using existing parser
	// Register shader via R_FindShader which will parse it
	// The shader text needs to be in the shader text buffer first
	// For procedural shaders, we'll register directly
	
	// Store shader text and register
	// Note: Full integration with ParseShader requires making it public or
	// creating a wrapper. For now, we register via the standard path.
	
	pshader->contentHash = contentHash;
	pshader->cached = qfalse;
	pshader->dirty = qfalse;
	
	return qtrue;
}

/*
===================
R_RegisterProceduralShader
===================
Register a procedural shader from shader text
===================
*/
qhandle_t R_RegisterProceduralShader(const char *name, const char *shaderText)
{
	int shaderId;
	unsigned int contentHash;
	qhandle_t shaderHandle;
	
	if (!name || !*name || !shaderText || !*shaderText) {
		return -1;
	}
	
	// Generate content hash
	contentHash = R_GenerateShaderContentHash(shaderText);
	
	// Check cache
	if (r_shaderCache && r_shaderCache->integer) {
		shaderHandle = R_FindShaderInCache(name, contentHash);
		if (shaderHandle >= 0) {
			return shaderHandle;
		}
	}
	
	// Create procedural shader
	shaderId = R_CreateProceduralShader(name, -1);
	if (shaderId < 0) {
		return -1;
	}
	
	// Parse shader text
	if (!R_ParseProceduralShaderText(shaderId, shaderText)) {
		R_DestroyProceduralShader(shaderId);
		return -1;
	}
	
	// Register shader using existing system
	// Note: RE_RegisterShaderNoMip is accessed via renderer interface
	// For now, use R_FindShader which will create/register the shader
	{
		shader_t *sh = R_FindShader(name, LIGHTMAP_NONE, qtrue);
		if (sh) {
			shaderHandle = sh->index;
		} else {
			shaderHandle = -1;
		}
	}
	
	// Add to cache
	if (r_shaderCache && r_shaderCache->integer && shaderHandle >= 0) {
		R_AddShaderToCache(name, contentHash, shaderHandle, shaderId);
	}
	
	return shaderHandle;
}

/*
===================
R_CopyShaderStage
===================
Copy a shader stage from one index to another
===================
*/
void R_CopyShaderStage(int shaderId, int srcStageIndex, int dstStageIndex)
{
	proceduralShader_t *pshader;
	shaderStage_t *srcStage, *dstStage;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (srcStageIndex < 0 || srcStageIndex >= pshader->numDynamicStages ||
		dstStageIndex < 0 || dstStageIndex >= MAX_SHADER_STAGES_DYNAMIC) {
		return;
	}
	
	srcStage = pshader->dynamicStages[srcStageIndex];
	if (!srcStage) {
		return;
	}
	
	// Allocate destination stage if needed
	if (dstStageIndex >= pshader->numDynamicStages) {
		dstStage = (shaderStage_t *)Z_Malloc(sizeof(shaderStage_t));
		if (!dstStage) {
			return;
		}
		pshader->dynamicStages[dstStageIndex] = dstStage;
		if (dstStageIndex >= pshader->numDynamicStages) {
			pshader->numDynamicStages = dstStageIndex + 1;
		}
	} else {
		dstStage = pshader->dynamicStages[dstStageIndex];
		if (!dstStage) {
			dstStage = (shaderStage_t *)Z_Malloc(sizeof(shaderStage_t));
			if (!dstStage) {
				return;
			}
			pshader->dynamicStages[dstStageIndex] = dstStage;
		}
	}
	
	// Copy stage
	*dstStage = *srcStage;
	pshader->dirty = qtrue;
}

/*
===================
R_SwapShaderStages
===================
Swap two shader stages
===================
*/
void R_SwapShaderStages(int shaderId, int stageIndex1, int stageIndex2)
{
	proceduralShader_t *pshader;
	shaderStage_t *temp;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex1 < 0 || stageIndex1 >= pshader->numDynamicStages ||
		stageIndex2 < 0 || stageIndex2 >= pshader->numDynamicStages) {
		return;
	}
	
	temp = pshader->dynamicStages[stageIndex1];
	pshader->dynamicStages[stageIndex1] = pshader->dynamicStages[stageIndex2];
	pshader->dynamicStages[stageIndex2] = temp;
	pshader->dirty = qtrue;
}

/*
===================
R_InsertShaderStage
===================
Insert a shader stage at a specific index
===================
*/
void R_InsertShaderStage(int shaderId, int stageIndex, const shaderStage_t *stage)
{
	proceduralShader_t *pshader;
	shaderStage_t *newStage;
	int i;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex < 0 || stageIndex > pshader->numDynamicStages ||
		pshader->numDynamicStages >= MAX_SHADER_STAGES_DYNAMIC) {
		return;
	}
	
	if (!stage) {
		return;
	}
	
	// Shift stages
	for (i = pshader->numDynamicStages; i > stageIndex; i--) {
		pshader->dynamicStages[i] = pshader->dynamicStages[i - 1];
	}
	
	// Allocate and copy new stage
	newStage = (shaderStage_t *)Z_Malloc(sizeof(shaderStage_t));
	if (!newStage) {
		return;
	}
	
	*newStage = *stage;
	pshader->dynamicStages[stageIndex] = newStage;
	pshader->numDynamicStages++;
	pshader->dirty = qtrue;
}

/*
===================
R_DuplicateShaderStage
===================
Duplicate a shader stage
===================
*/
void R_DuplicateShaderStage(int shaderId, int stageIndex)
{
	R_CopyShaderStage(shaderId, stageIndex, stageIndex + 1);
}

/*
===================
R_SetShaderStageBlendMode
===================
Set blend mode for a shader stage
===================
*/
void R_SetShaderStageBlendMode(int shaderId, int stageIndex, int srcBlend, int dstBlend)
{
	proceduralShader_t *pshader;
	shaderStage_t *stage;
	unsigned int stateBits;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages) {
		return;
	}
	
	stage = pshader->dynamicStages[stageIndex];
	if (!stage) {
		return;
	}
	
	// Clear existing blend bits
	stateBits = stage->stateBits;
	stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
	
	// Set new blend bits
	stateBits |= srcBlend;
	stateBits |= dstBlend;
	
	stage->stateBits = stateBits;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderStageTexture
===================
Set texture for a shader stage bundle
===================
*/
void R_SetShaderStageTexture(int shaderId, int stageIndex, int bundleIndex, qhandle_t texture)
{
	proceduralShader_t *pshader;
	shaderStage_t *stage;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages ||
		bundleIndex < 0 || bundleIndex >= NUM_TEXTURE_BUNDLES) {
		return;
	}
	
	stage = pshader->dynamicStages[stageIndex];
	if (!stage) {
		return;
	}
	
	// Set texture handle
	{
		shader_t *texShader = R_GetShaderByHandle(texture);
		if (texShader && texShader->stages[0] && texShader->stages[0]->bundle[0].image[0]) {
			stage->bundle[bundleIndex].image[0] = texShader->stages[0]->bundle[0].image[0];
			pshader->dirty = qtrue;
		}
	}
}

/*
===================
R_SetShaderStageRGBGen
===================
Set RGB generation for a shader stage
===================
*/
void R_SetShaderStageRGBGen(int shaderId, int stageIndex, colorGen_t rgbGen)
{
	proceduralShader_t *pshader;
	shaderStage_t *stage;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages) {
		return;
	}
	
	stage = pshader->dynamicStages[stageIndex];
	if (!stage) {
		return;
	}
	
	stage->rgbGen = rgbGen;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderStageAlphaGen
===================
Set alpha generation for a shader stage
===================
*/
void R_SetShaderStageAlphaGen(int shaderId, int stageIndex, alphaGen_t alphaGen)
{
	proceduralShader_t *pshader;
	shaderStage_t *stage;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages) {
		return;
	}
	
	stage = pshader->dynamicStages[stageIndex];
	if (!stage) {
		return;
	}
	
	stage->alphaGen = alphaGen;
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderStageWaveform
===================
Set waveform for a shader stage
===================
*/
void R_SetShaderStageWaveform(int shaderId, int stageIndex, qboolean isRGB, waveForm_t *waveform)
{
	proceduralShader_t *pshader;
	shaderStage_t *stage;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages) {
		return;
	}
	
	stage = pshader->dynamicStages[stageIndex];
	if (!stage) {
		return;
	}
	
	if (!waveform) {
		return;
	}
	
	if (isRGB) {
		stage->rgbWave = *waveform;
	} else {
		stage->alphaWave = *waveform;
	}
	
	pshader->dirty = qtrue;
}

/*
===================
R_SetShaderStageTexMod
===================
Set texture modification for a shader stage bundle
===================
*/
void R_SetShaderStageTexMod(int shaderId, int stageIndex, int bundleIndex, int modIndex, texMod_t type, float *params)
{
	proceduralShader_t *pshader;
	shaderStage_t *stage;
	texModInfo_t *texMod;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (stageIndex < 0 || stageIndex >= pshader->numDynamicStages ||
		bundleIndex < 0 || bundleIndex >= NUM_TEXTURE_BUNDLES ||
		modIndex < 0 || modIndex >= TR_MAX_TEXMODS) {
		return;
	}
	
	stage = pshader->dynamicStages[stageIndex];
	if (!stage) {
		return;
	}
	
	texMod = &stage->bundle[bundleIndex].texMods[modIndex];
	texMod->type = type;
	
	if (params) {
		// Copy parameters based on type
		switch (type) {
			case TMOD_TRANSFORM:
			case TMOD_SCALE:
			case TMOD_SCALE_OFFSET:
			case TMOD_OFFSET_SCALE:
				if (params[0] != 0.0f) texMod->transform.matrix[0][0] = params[0];
				if (params[1] != 0.0f) texMod->transform.matrix[0][1] = params[1];
				if (params[2] != 0.0f) texMod->transform.matrix[1][0] = params[2];
				if (params[3] != 0.0f) texMod->transform.matrix[1][1] = params[3];
				if (params[4] != 0.0f) texMod->transform.translate[0] = params[4];
				if (params[5] != 0.0f) texMod->transform.translate[1] = params[5];
				break;
			case TMOD_TURBULENT:
			case TMOD_SCROLL:
			case TMOD_STRETCH:
			case TMOD_ROTATE:
			case TMOD_OFFSET:
				if (params[0] != 0.0f) texMod->wave.base = params[0];
				if (params[1] != 0.0f) texMod->wave.amplitude = params[1];
				if (params[2] != 0.0f) texMod->wave.phase = params[2];
				if (params[3] != 0.0f) texMod->wave.frequency = params[3];
				break;
			default:
				break;
		}
	}
	
	pshader->dirty = qtrue;
}

#ifdef USE_VULKAN
/*
===================
R_CompileVulkanShader
===================
Compile GLSL shader to SPIR-V for Vulkan
===================
*/
qboolean R_CompileVulkanShader(int shaderId)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return qfalse;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return qfalse;
	}
	
	if (!pshader->needsVulkanCompile) {
		return qtrue; // Already compiled
	}
	
	// TODO: Integrate with Vulkan shader compilation system
	// This would require:
	// 1. Calling glslangValidator or using glslang library
	// 2. Creating VkShaderModule from SPIR-V
	// 3. Storing module in pshader->vulkanModule
	
	// For now, mark as compiled
	pshader->needsVulkanCompile = qfalse;
	
	return qtrue;
}

/*
===================
R_CompileVulkanShaderFromText
===================
Compile GLSL shader text to SPIR-V for Vulkan
===================
*/
void R_CompileVulkanShaderFromText(int shaderId, const char *vertexGLSL, const char *fragmentGLSL)
{
	proceduralShader_t *pshader;
	
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return;
	}
	
	pshader = &proceduralShaders[shaderId];
	if (!pshader->active) {
		return;
	}
	
	if (vertexGLSL) {
		Q_strncpyz(pshader->vertexSource, vertexGLSL, sizeof(pshader->vertexSource));
	}
	
	if (fragmentGLSL) {
		Q_strncpyz(pshader->fragmentSource, fragmentGLSL, sizeof(pshader->fragmentSource));
	}
	
	pshader->isVulkanShader = qtrue;
	pshader->needsVulkanCompile = qtrue;
	
	// Trigger compilation
	R_CompileVulkanShader(shaderId);
}

/*
===================
R_GenerateVulkanShader
===================
Generate Vulkan shader from GLSL source
===================
*/
void R_GenerateVulkanShader(int shaderId, const char *vertexSource, const char *fragmentSource)
{
	R_CompileVulkanShaderFromText(shaderId, vertexSource, fragmentSource);
}

/*
===================
R_UpdateVulkanShaderParameters
===================
Update Vulkan uniform buffers with shader parameters
===================
*/
void R_UpdateVulkanShaderParameters(int shaderId)
{
	Q_UNUSED(shaderId);

	// TODO: Update Vulkan uniform buffers
	// This would require access to Vulkan renderer's uniform buffer system
}

/*
===================
R_IsVulkanShader
===================
Check if shader is using Vulkan renderer
===================
*/
qboolean R_IsVulkanShader(int shaderId)
{
	if (shaderId < 0 || shaderId >= MAX_PROCEDURAL_SHADERS) {
		return qfalse;
	}
	
	return proceduralShaders[shaderId].isVulkanShader;
}
#endif

