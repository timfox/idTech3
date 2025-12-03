#ifndef TR_LOCAL_H
#define TR_LOCAL_H

#if defined(__APPLE__) && !defined(__ANDROID__)

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "tr_common.h"
#include "metal.h"

#ifdef USE_CIMGUI
typedef struct ImDrawData ImDrawData;
#endif

// Metal constants (similar to GL constants)
typedef enum {
	METAL_NEAREST,
	METAL_LINEAR,
	METAL_NEAREST_MIPMAP_NEAREST,
	METAL_LINEAR_MIPMAP_NEAREST,
	METAL_NEAREST_MIPMAP_LINEAR,
	METAL_LINEAR_MIPMAP_LINEAR,
	METAL_MODULATE,
	METAL_ADD,
	METAL_ADD_NONIDENTITY,
	METAL_BLEND_MODULATE,
	METAL_BLEND_ADD,
	METAL_BLEND_ALPHA,
	METAL_BLEND_ONE_MINUS_ALPHA,
	METAL_BLEND_MIX_ALPHA,
	METAL_BLEND_MIX_ONE_MINUS_ALPHA,
	METAL_BLEND_DST_COLOR_SRC_ALPHA,
	METAL_DECAL,
	METAL_BACK_LEFT,
	METAL_BACK_RIGHT
} metalCompat;

#define METAL_INDEX_TYPE		uint32_t
#define METALint				int
#define METALuint				unsigned int
#define METALboolean			qboolean

// Maximum values
#define MAX_METAL_TEXTURES		32
#define MAX_METAL_PIPELINES		1024
#define MAX_METAL_SAMPLERS		32

// Buffer sizes
#define VERTEX_BUFFER_SIZE		(4 * 1024 * 1024)
#define INDEX_BUFFER_SIZE		(2 * 1024 * 1024)
#define STAGING_BUFFER_SIZE		(2 * 1024 * 1024)

// Swap chain
#define MAX_SWAPCHAIN_BUFFERS	3

// Forward declarations
typedef struct image_s image_t;
typedef struct shader_s shader_t;
typedef struct model_s model_t;

// Metal-specific image extension
#ifdef __OBJC__
struct image_s {
	char imgName[MAX_QPATH];
	int width, height;
	int uploadWidth, uploadHeight;
	imgFlags_t flags;
	int frameUsed;
	void *metalTexture; // id<MTLTexture> (bridged)
	void *metalSampler; // id<MTLSamplerState> (bridged)
};
#else
// C-compatible forward declaration
struct image_s {
	char imgName[MAX_QPATH];
	int width, height;
	int uploadWidth, uploadHeight;
	imgFlags_t flags;
	int frameUsed;
	void *metalTexture;
	void *metalSampler;
};
#endif

// Metal renderer state
typedef struct {
	qboolean initialized;
	qboolean active;
	
	// Window/view
	#ifdef TARGET_OS_IPHONE
	void *view;
	#else
	void *window;
	#endif
	int width;
	int height;
	
	// Metal context
	metalContext_t metal;
	
	// Current state
	image_t *currentImage;
	shader_t *currentShader;
	float currentColor[4]; // Current color for 2D rendering
	
	// Statistics
	int drawCalls;
	int triangles;
	int vertices;
	
	// Performance counters
	int frameTime;
	int gpuTime;
} metalRenderer_t;

extern metalRenderer_t tr;

// Function prototypes
void Metal_Init(void);
void Metal_Shutdown(void);
qboolean Metal_InitWindow(void *windowOrView);
void Metal_ShutdownWindow(void);
void Metal_BeginFrame(void);
void Metal_EndFrame(void);
void Metal_Present(void);
void Metal_Resize(int width, int height);

// Texture management
void Metal_BindTexture(image_t *image);
image_t *Metal_CreateImage(const char *name, const byte *pic, int width, int height, imgFlags_t flags);

// 2D rendering
void Metal_Setup2DProjection(void);
void Metal_AddQuad2D(float x, float y, float w, float h, 
					float s1, float t1, float s2, float t2,
					const float *color);

#endif // __APPLE__ && !__ANDROID__

#endif // TR_LOCAL_H

