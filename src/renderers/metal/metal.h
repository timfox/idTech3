#ifndef __METAL_H__
#define __METAL_H__

#if defined(__APPLE__) && !defined(__ANDROID__)

#include "../common/q_shared.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#else
// C-compatible forward declarations
typedef void* id;
typedef void* MTLDevice;
typedef void* MTLCommandQueue;
typedef void* MTLCommandBuffer;
typedef void* MTLRenderCommandEncoder;
typedef void* MTLLibrary;
typedef void* MTLFunction;
typedef void* MTLRenderPipelineState;
typedef void* MTLDepthStencilState;
typedef void* MTLTexture;
typedef void* MTLBuffer;
typedef void* CAMetalLayer;
typedef void* NSWindow;
typedef void* UIView;
#endif

// Metal context structure
#ifdef __OBJC__
typedef struct {
	qboolean initialized;
	
	// Metal device and command queue
	id<MTLDevice> device;
	id<MTLCommandQueue> commandQueue;
	
	// Metal layer (for macOS/iOS)
	CAMetalLayer *metalLayer;
	
	// Current command buffer
	id<MTLCommandBuffer> currentCommandBuffer;
	id<MTLRenderCommandEncoder> currentRenderEncoder;
	id<CAMetalDrawable> currentDrawable; // Current frame's drawable
	
	// Render pipeline states
	id<MTLRenderPipelineState> renderPipelineState; // Default 3D pipeline
	id<MTLRenderPipelineState> uiPipelineState; // UI/2D pipeline
	id<MTLDepthStencilState> depthStencilState;
	id<MTLDepthStencilState> uiDepthStencilState; // No depth testing for 2D
	
	// Shader library
	id<MTLLibrary> shaderLibrary;
	
	// Buffers
	id<MTLBuffer> vertexBuffer;
	id<MTLBuffer> indexBuffer;
	size_t vertexBufferSize;
	size_t indexBufferSize;
	
	// Textures
	id<MTLTexture> depthTexture;
	id<MTLTexture> colorTexture;
	
	// Viewport
	MTLViewport viewport;
	
	// 2D projection matrix (orthographic)
	float projection2D[16];
	
	// Window/view handle
	#ifdef __APPLE__
	#ifdef TARGET_OS_IPHONE
	UIView *view;
	#else
	NSWindow *window;
	#endif
	#endif
	
	int width;
	int height;
	
	// Frame synchronization
	dispatch_semaphore_t frameSemaphore;
	int maxFramesInFlight;
	int currentFrame;
	
	// Feature support
	qboolean supportsArgumentBuffers;
	qboolean supportsIndirectCommandBuffers;
	qboolean supportsRayTracing; // Metal 3.0+
} metalContext_t;
#else
// C-compatible structure (pointers only)
typedef struct {
	qboolean initialized;
	void *device;
	void *commandQueue;
	void *metalLayer;
	void *currentCommandBuffer;
	void *currentRenderEncoder;
	void *currentDrawable;
	void *renderPipelineState;
	void *uiPipelineState;
	void *depthStencilState;
	void *uiDepthStencilState;
	void *shaderLibrary;
	void *vertexBuffer;
	void *indexBuffer;
	size_t vertexBufferSize;
	size_t indexBufferSize;
	void *depthTexture;
	void *colorTexture;
	// Viewport would need separate struct in C
	#ifdef TARGET_OS_IPHONE
	void *view;
	#else
	void *window;
	#endif
	int width;
	int height;
	void *frameSemaphore;
	int maxFramesInFlight;
	int currentFrame;
	qboolean supportsArgumentBuffers;
	qboolean supportsIndirectCommandBuffers;
	qboolean supportsRayTracing;
	float projection2D[16];
} metalContext_t;
#endif

extern metalContext_t metal;

// Function prototypes
// Low-level Metal API functions (internal use)
qboolean MetalAPI_Init(void);
void MetalAPI_Shutdown(void);
void MetalAPI_Present(void);

// High-level functions (called from renderer interface)
qboolean Metal_CreateDevice(void);
qboolean Metal_CreateSwapChain(void *windowOrView, int width, int height);
qboolean Metal_CreateRenderTargets(int width, int height);
qboolean Metal_CreateRenderPipeline(void);
qboolean Metal_CreateUIPipeline(void);
qboolean Metal_CreateDepthStencilState(void);
void Metal_Update2DProjection(void);
void Metal_WaitForGPU(void);
void Metal_Present(void);
void Metal_ResizeSwapChain(int width, int height);

// Helper functions
const char* Metal_GetErrorString(int error);
qboolean Metal_CheckFeatureSupport(void);

// Shader compilation
qboolean Metal_LoadShaders(void);
id<MTLFunction> Metal_GetFunction(const char *name);

#endif // __APPLE__ && !__ANDROID__

#endif // __METAL_H__

