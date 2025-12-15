#ifndef __TR_SHADERS_ENHANCED_H__
#define __TR_SHADERS_ENHANCED_H__

#include "tr_local.h"

#define MAX_PROCEDURAL_SHADERS		256
#define MAX_SHADER_PARAMETERS		32
#define MAX_SHADER_ANIMATIONS		16
#define MAX_SHADER_STAGES_DYNAMIC	8
#define MAX_SHADER_CACHE_ENTRIES		512
#define MAX_SHADER_CACHE_NAME		64

// Shader parameter types
typedef enum {
	SHADER_PARAM_FLOAT,
	SHADER_PARAM_VEC2,
	SHADER_PARAM_VEC3,
	SHADER_PARAM_VEC4,
	SHADER_PARAM_INT,
	SHADER_PARAM_BOOL,
	SHADER_PARAM_TEXTURE,
	SHADER_PARAM_COLOR
} shaderParamType_t;

// Shader animation types
typedef enum {
	SHADER_ANIM_LINEAR,
	SHADER_ANIM_SINE,
	SHADER_ANIM_COSINE,
	SHADER_ANIM_TRIANGLE,
	SHADER_ANIM_SAWTOOTH,
	SHADER_ANIM_INVERSE_SAWTOOTH,
	SHADER_ANIM_NOISE,
	SHADER_ANIM_CUSTOM		// Lua script function
} shaderAnimType_t;

// Shader parameter structure
typedef struct {
	char				name[MAX_QPATH];
	shaderParamType_t	type;
	union {
		float			fValue;
		vec2_t			v2Value;
		vec3_t			v3Value;
		vec4_t			v4Value;
		int				iValue;
		qboolean		bValue;
		qhandle_t		tValue;		// Texture handle
	} value;
	
	// Animation
	qboolean			animated;
	shaderAnimType_t	animType;
	float				animSpeed;
	float				animPhase;
	float				animMin;
	float				animMax;
	int					scriptId;	// Lua script ID for custom animation
	
	// PBR-specific parameters
	qboolean			isPBRParam;	// Is this a PBR parameter?
	char				pbrProperty[32];	// metallic, roughness, albedo, etc.
} shaderParameter_t;

// Procedural shader structure
typedef struct {
	char				name[MAX_QPATH];
	qhandle_t			baseShader;		// Base shader to modify (-1 = create new)
	qhandle_t			shaderHandle;	// Generated shader handle
	
	// Dynamic stages
	shaderStage_t		*dynamicStages[MAX_SHADER_STAGES_DYNAMIC];
	int					numDynamicStages;
	
	// Parameters
	shaderParameter_t	parameters[MAX_SHADER_PARAMETERS];
	int					numParameters;
	
	// PBR properties
	qboolean			isPBR;
	float				metallic;
	float				roughness;
	vec3_t				albedo;
	float				ao;				// Ambient occlusion
	vec3_t				emissive;
	
	// State
	qboolean			active;
	qboolean			dirty;			// Needs regeneration
	int					lastUpdateTime;
	
	// Scripting
	int					scriptId;		// Lua script ID
	
	// Caching
	unsigned int		contentHash;	// Hash of shader content for caching
	qboolean			cached;			// Is this shader cached?
	
	// Vulkan
#ifdef USE_VULKAN
	qboolean			isVulkanShader;	// Is this a Vulkan shader?
	void				*vulkanModule;	// VkShaderModule (cast as void*)
	char				vertexSource[8192];	// GLSL vertex source
	char				fragmentSource[8192];	// GLSL fragment source
	qboolean			needsVulkanCompile;	// Needs Vulkan compilation
#endif
} proceduralShader_t;

// Shader cache entry
typedef struct {
	char				name[MAX_SHADER_CACHE_NAME];
	unsigned int		contentHash;
	qhandle_t			shaderHandle;
	int					shaderId;		// Procedural shader ID (-1 if not procedural)
	int					lastAccessTime;
	qboolean			inUse;
} shaderCacheEntry_t;

// Enhanced shader system functions
void	R_InitShadersEnhanced(void);
void	R_ShutdownShadersEnhanced(void);
void	R_ClearShadersEnhanced(void);

// Procedural shader management
int		R_CreateProceduralShader(const char *name, qhandle_t baseShader);
void	R_DestroyProceduralShader(int shaderId);
qhandle_t R_GetProceduralShaderHandle(int shaderId);
void	R_RegenerateProceduralShader(int shaderId);

// Shader parameter management
int		R_AddShaderParameter(int shaderId, const char *name, shaderParamType_t type);
void	R_RemoveShaderParameter(int shaderId, int paramIndex);
void	R_SetShaderParameterFloat(int shaderId, int paramIndex, float value);
void	R_SetShaderParameterVec2(int shaderId, int paramIndex, const vec2_t value);
void	R_SetShaderParameterVec3(int shaderId, int paramIndex, const vec3_t value);
void	R_SetShaderParameterVec4(int shaderId, int paramIndex, const vec4_t value);
void	R_SetShaderParameterInt(int shaderId, int paramIndex, int value);
void	R_SetShaderParameterBool(int shaderId, int paramIndex, qboolean value);
void	R_SetShaderParameterTexture(int shaderId, int paramIndex, qhandle_t texture);
void	R_SetShaderParameterByName(int shaderId, const char *name, shaderParamType_t type, const void *value);

// Shader animation
void	R_SetShaderParameterAnimation(int shaderId, int paramIndex, shaderAnimType_t type, float speed, float min, float max);
void	R_SetShaderParameterAnimationScript(int shaderId, int paramIndex, int scriptId);
void	R_UpdateShaderAnimations(float deltaTime);

// PBR shader functions
void	R_SetShaderPBR(int shaderId, qboolean enabled);
void	R_SetShaderMetallic(int shaderId, float metallic);
void	R_SetShaderRoughness(int shaderId, float roughness);
void	R_SetShaderAlbedo(int shaderId, const vec3_t albedo);
void	R_SetShaderAO(int shaderId, float ao);
void	R_SetShaderEmissive(int shaderId, const vec3_t emissive);
void	R_SetShaderPBRTexture(int shaderId, qhandle_t ormTexture);	// ORM texture (Occlusion/Roughness/Metallic)

// Runtime shader modification
void	R_ModifyShaderStage(int shaderId, int stageIndex, const shaderStage_t *stage);
void	R_AddShaderStage(int shaderId, const shaderStage_t *stage);
void	R_RemoveShaderStage(int shaderId, int stageIndex);
void	R_SetShaderCullType(int shaderId, cullType_t cullType);
void	R_SetShaderSort(int shaderId, float sort);
void	R_SetShaderPortalRange(int shaderId, float range);

// Vulkan-specific functions
#ifdef USE_VULKAN
void	R_GenerateVulkanShader(int shaderId, const char *vertexSource, const char *fragmentSource);
void	R_UpdateVulkanShaderParameters(int shaderId);
qboolean R_IsVulkanShader(int shaderId);
qboolean R_CompileVulkanShader(int shaderId);
void	R_CompileVulkanShaderFromText(int shaderId, const char *vertexGLSL, const char *fragmentGLSL);
#endif

// Shader caching
void	R_ClearShaderCache(void);
qhandle_t R_FindShaderInCache(const char *name, unsigned int contentHash);
void	R_AddShaderToCache(const char *name, unsigned int contentHash, qhandle_t shaderHandle, int shaderId);
void	R_UpdateShaderCache(void);
int		R_GetShaderCacheSize(void);

// Advanced shader stage manipulation
void	R_CopyShaderStage(int shaderId, int srcStageIndex, int dstStageIndex);
void	R_SwapShaderStages(int shaderId, int stageIndex1, int stageIndex2);
void	R_InsertShaderStage(int shaderId, int stageIndex, const shaderStage_t *stage);
void	R_DuplicateShaderStage(int shaderId, int stageIndex);
void	R_SetShaderStageBlendMode(int shaderId, int stageIndex, int srcBlend, int dstBlend);
void	R_SetShaderStageTexture(int shaderId, int stageIndex, int bundleIndex, qhandle_t texture);
void	R_SetShaderStageRGBGen(int shaderId, int stageIndex, colorGen_t rgbGen);
void	R_SetShaderStageAlphaGen(int shaderId, int stageIndex, alphaGen_t alphaGen);
void	R_SetShaderStageWaveform(int shaderId, int stageIndex, qboolean isRGB, waveForm_t *waveform);
void	R_SetShaderStageTexMod(int shaderId, int stageIndex, int bundleIndex, int modIndex, texMod_t type, float *params);

// Shader parser integration
qboolean R_ParseProceduralShaderText(int shaderId, const char *shaderText);
qhandle_t R_RegisterProceduralShader(const char *name, const char *shaderText);
unsigned int R_GenerateShaderContentHash(const char *shaderText);

// Utility
int		R_FindProceduralShader(const char *name);
int		R_FindShaderParameter(int shaderId, const char *name);
void	R_UpdateProceduralShaders(float deltaTime);
int		R_GetActiveProceduralShaderCount(void);

#endif // __TR_SHADERS_ENHANCED_H__

