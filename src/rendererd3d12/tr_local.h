#ifndef TR_LOCAL_H
#define TR_LOCAL_H

#ifdef _WIN32

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "tr_common.h"
#include "d3d12.h"

#ifdef USE_CIMGUI
typedef struct ImDrawData ImDrawData;
#endif

// D3D12 constants (similar to GL constants)
typedef enum {
	D3D12_NEAREST,
	D3D12_LINEAR,
	D3D12_NEAREST_MIPMAP_NEAREST,
	D3D12_LINEAR_MIPMAP_NEAREST,
	D3D12_NEAREST_MIPMAP_LINEAR,
	D3D12_LINEAR_MIPMAP_LINEAR,
	D3D12_MODULATE,
	D3D12_ADD,
	D3D12_ADD_NONIDENTITY,
	D3D12_BLEND_MODULATE,
	D3D12_BLEND_ADD,
	D3D12_BLEND_ALPHA,
	D3D12_BLEND_ONE_MINUS_ALPHA,
	D3D12_BLEND_MIX_ALPHA,
	D3D12_BLEND_MIX_ONE_MINUS_ALPHA,
	D3D12_BLEND_DST_COLOR_SRC_ALPHA,
	D3D12_DECAL,
	D3D12_BACK_LEFT,
	D3D12_BACK_RIGHT
} d3d12Compat;

#define D3D12_INDEX_TYPE		uint32_t
#define D3D12int				int
#define D3D12uint				unsigned int
#define D3D12boolean			BOOL

// Maximum values
#define MAX_D3D12_TEXTURES		32
#define MAX_D3D12_PIPELINES		1024
#define MAX_D3D12_SAMPLERS		32

// Buffer sizes
#define VERTEX_BUFFER_SIZE		(4 * 1024 * 1024)
#define INDEX_BUFFER_SIZE		(2 * 1024 * 1024)
#define STAGING_BUFFER_SIZE		(2 * 1024 * 1024)

// Swap chain
#define MAX_SWAPCHAIN_BUFFERS	3

// Descriptor heap sizes
#define RTV_HEAP_SIZE			MAX_SWAPCHAIN_BUFFERS
#define DSV_HEAP_SIZE			1
#define SRV_HEAP_SIZE			1024
#define SAMPLER_HEAP_SIZE		32

// Forward declarations
typedef struct image_s image_t;
typedef struct shader_s shader_t;
typedef struct model_s model_t;

// D3D12 renderer state
typedef struct {
	qboolean initialized;
	qboolean active;
	
	// Window
	HWND hwnd;
	UINT width;
	UINT height;
	
	// D3D12 context
	d3d12Context_t d3d12;
	
	// Current state
	image_t *currentImage;
	shader_t *currentShader;
	
	// Statistics
	int drawCalls;
	int triangles;
	int vertices;
	
	// Performance counters
	int frameTime;
	int gpuTime;
} d3d12Renderer_t;

extern d3d12Renderer_t tr;

// Function prototypes (to be implemented)
void D3D12_Init(void);
void D3D12_Shutdown(void);
qboolean D3D12_InitWindow(HWND hwnd);
void D3D12_ShutdownWindow(void);
void D3D12_BeginFrame(void);
void D3D12_EndFrame(void);
void D3D12_Present(void);
void D3D12_Resize(UINT width, UINT height);

#endif // _WIN32

#endif // TR_LOCAL_H

