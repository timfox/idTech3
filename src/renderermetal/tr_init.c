#if defined(__APPLE__) && !defined(__ANDROID__)

#import "tr_local.h"
#import "../qcommon/qcommon.h"

#ifdef TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

metalRenderer_t tr;

/*
================
Metal_Init
================
High-level renderer initialization
*/
void Metal_Init(void) {
	Com_Memset(&tr, 0, sizeof(tr));
	
	// Initialize low-level Metal API
	extern qboolean MetalAPI_Init(void);
	if (!MetalAPI_Init()) {
		Com_Error(ERR_FATAL, "Metal_Init failed");
	}
	
	tr.initialized = qtrue;
	Com_Printf("Metal renderer initialized\n");
}

/*
================
Metal_Shutdown
================
High-level renderer shutdown
*/
void Metal_Shutdown(void) {
	if (!tr.initialized) {
		return;
	}
	
	// Shutdown low-level Metal API
	extern void MetalAPI_Shutdown(void);
	MetalAPI_Shutdown();
	
	tr.initialized = qfalse;
	Com_Printf("Metal renderer shut down\n");
}

/*
================
Metal_InitWindow
================
*/
qboolean Metal_InitWindow(void *windowOrView) {
	if (!tr.initialized) {
		return qfalse;
	}
	
#ifdef TARGET_OS_IPHONE
	UIView *view = (__bridge UIView *)windowOrView;
	if (!view) {
		return qfalse;
	}
	
	CGSize size = view.bounds.size;
	tr.width = (int)size.width;
	tr.height = (int)size.height;
	tr.view = windowOrView;
#else
	NSWindow *window = (__bridge NSWindow *)windowOrView;
	if (!window) {
		return qfalse;
	}
	
	NSRect frame = [window.contentView bounds];
	tr.width = (int)frame.size.width;
	tr.height = (int)frame.size.height;
	tr.window = windowOrView;
#endif
	
	// Create swap chain
	if (!Metal_CreateSwapChain(windowOrView, tr.width, tr.height)) {
		return qfalse;
	}
	
	// Create render targets
	if (!Metal_CreateRenderTargets(tr.width, tr.height)) {
		return qfalse;
	}
	
	// Load shaders
	if (!Metal_LoadShaders()) {
		Com_Printf("Metal: Warning - Shader loading failed\n");
	}
	
	// Create render pipelines
	if (!Metal_CreateRenderPipeline()) {
		Com_Printf("Metal: Warning - Render pipeline creation failed\n");
	}
	
	// Create UI render pipeline
	extern qboolean Metal_CreateUIPipeline(void);
	if (!Metal_CreateUIPipeline()) {
		Com_Printf("Metal: Warning - UI render pipeline creation failed\n");
	}
	
	// Create depth stencil states
	if (!Metal_CreateDepthStencilState()) {
		Com_Printf("Metal: Warning - Depth stencil state creation failed\n");
	}
	
	tr.active = qtrue;
	return qtrue;
}

/*
================
Metal_ShutdownWindow
================
*/
void Metal_ShutdownWindow(void) {
	if (!tr.active) {
		return;
	}
	
	tr.active = qfalse;
}

/*
================
Metal_BeginFrame
================
*/
void Metal_BeginFrame(void) {
	if (!tr.active) {
		return;
	}
	
	// Wait for frame semaphore (if using triple buffering)
	if (metal.frameSemaphore) {
		dispatch_semaphore_wait(metal.frameSemaphore, DISPATCH_TIME_FOREVER);
	}
	
	// Get command buffer
	metal.currentCommandBuffer = [metal.commandQueue commandBuffer];
	if (!metal.currentCommandBuffer) {
		return;
	}
	
	// Get drawable for this frame
	metal.currentDrawable = [metal.metalLayer nextDrawable];
	if (!metal.currentDrawable) {
		return;
	}
	
	// Create render pass descriptor
	MTLRenderPassDescriptor *renderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	if (!renderPassDesc) {
		return;
	}
	
	renderPassDesc.colorAttachments[0].texture = metal.currentDrawable.texture;
	renderPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
	renderPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
	renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	
	renderPassDesc.depthAttachment.texture = metal.depthTexture;
	renderPassDesc.depthAttachment.loadAction = MTLLoadActionClear;
	renderPassDesc.depthAttachment.clearDepth = 1.0;
	renderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
	
	// Create render command encoder
	metal.currentRenderEncoder = [metal.currentCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
	if (!metal.currentRenderEncoder) {
		return;
	}
	
	// Set viewport
	metal.viewport.originX = 0.0;
	metal.viewport.originY = 0.0;
	metal.viewport.width = (double)tr.width;
	metal.viewport.height = (double)tr.height;
	metal.viewport.znear = 0.0;
	metal.viewport.zfar = 1.0;
	
	[metal.currentRenderEncoder setViewport:metal.viewport];
	
	// Set render pipeline state
	if (metal.renderPipelineState) {
		[metal.currentRenderEncoder setRenderPipelineState:metal.renderPipelineState];
	}
	
	// Set depth stencil state
	if (metal.depthStencilState) {
		[metal.currentRenderEncoder setDepthStencilState:metal.depthStencilState];
	}
	
	// Reset statistics
	tr.drawCalls = 0;
	tr.triangles = 0;
	tr.vertices = 0;
}

/*
================
Metal_EndFrame
================
*/
void Metal_EndFrame(void) {
	if (!tr.active) {
		return;
	}
	
	// End encoding
	if (metal.currentRenderEncoder) {
		[metal.currentRenderEncoder endEncoding];
		metal.currentRenderEncoder = nil;
	}
}

/*
================
Metal_Present
================
Present the current frame
*/
void Metal_Present(void) {
	if (!tr.active) {
		return;
	}
	
	// Present via low-level Metal API
	extern void MetalAPI_Present(void);
	MetalAPI_Present();
}

/*
================
Metal_Resize
================
*/
void Metal_Resize(int width, int height) {
	if (!tr.active) {
		return;
	}
	
	if (tr.width == width && tr.height == height) {
		return;
	}
	
	Metal_ResizeSwapChain(width, height);
	tr.width = width;
	tr.height = height;
}

#endif // __APPLE__ && !__ANDROID__

