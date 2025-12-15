#if defined(__APPLE__) && !defined(__ANDROID__)

#import "metal.h"
#import "../qcommon/qcommon.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#ifdef TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

metalContext_t metal;

/*
================
Metal_GetErrorString
================
*/
const char* Metal_GetErrorString(int error) {
	// Metal uses NSError, but we'll provide basic error strings
	switch (error) {
		case 1:
			return "Device creation failed";
		case 2:
			return "Command queue creation failed";
		case 3:
			return "Swap chain creation failed";
		case 4:
			return "Render pipeline creation failed";
		case 5:
			return "Shader compilation failed";
		case 6:
			return "Buffer creation failed";
		case 7:
			return "Texture creation failed";
		default:
			return "Unknown error";
	}
}

/*
================
Metal_CheckFeatureSupport
================
*/
qboolean Metal_CheckFeatureSupport(void) {
	if (!metal.device) {
		return qfalse;
	}
	
	// Check for argument buffers support (Metal 2.0+)
	if (@available(macOS 10.13, iOS 11.0, *)) {
		metal.supportsArgumentBuffers = [metal.device supportsFamily:MTLGPUFamilyApple1] ||
									   [metal.device supportsFamily:MTLGPUFamilyMac1] ||
									   [metal.device supportsFamily:MTLGPUFamilyMac2];
	}
	
	// Check for indirect command buffers (Metal 2.0+)
	if (@available(macOS 10.14, iOS 12.0, *)) {
		metal.supportsIndirectCommandBuffers = [metal.device supportsFamily:MTLGPUFamilyApple1] ||
											   [metal.device supportsFamily:MTLGPUFamilyMac1] ||
											   [metal.device supportsFamily:MTLGPUFamilyMac2];
	}
	
	// Check for ray tracing (Metal 3.0+)
	if (@available(macOS 13.0, iOS 16.0, *)) {
		metal.supportsRayTracing = [metal.device supportsRayTracing];
	} else {
		metal.supportsRayTracing = qfalse;
	}
	
	return qtrue;
}

/*
================
Metal_CreateDevice
================
*/
qboolean Metal_CreateDevice(void) {
	// Get default Metal device
	metal.device = MTLCreateSystemDefaultDevice();
	if (!metal.device) {
		Com_Printf("Metal: Failed to create Metal device\n");
		return qfalse;
	}
	
	Com_Printf("Metal: Device created: %s\n", [[metal.device name] UTF8String]);
	
	// Create command queue
	metal.commandQueue = [metal.device newCommandQueue];
	if (!metal.commandQueue) {
		Com_Printf("Metal: Failed to create command queue\n");
		return qfalse;
	}
	
	// Set max frames in flight (triple buffering)
	metal.maxFramesInFlight = 3;
	metal.currentFrame = 0;
	metal.frameSemaphore = dispatch_semaphore_create(metal.maxFramesInFlight);
	
	// Check feature support
	Metal_CheckFeatureSupport();
	
	return qtrue;
}

/*
================
Metal_CreateSwapChain
================
*/
qboolean Metal_CreateSwapChain(void *windowOrView, int width, int height) {
	if (!metal.device) {
		return qfalse;
	}
	
#ifdef TARGET_OS_IPHONE
	UIView *view = (__bridge UIView *)windowOrView;
	if (!view) {
		return qfalse;
	}
	
	// Create Metal layer
	metal.metalLayer = [CAMetalLayer layer];
	if (!metal.metalLayer) {
		return qfalse;
	}
	
	metal.metalLayer.device = metal.device;
	metal.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	metal.metalLayer.framebufferOnly = YES;
	metal.metalLayer.frame = view.bounds;
	
	// Add layer to view
	[view.layer addSublayer:metal.metalLayer];
	metal.view = (__bridge void *)view;
#else
	NSWindow *window = (__bridge NSWindow *)windowOrView;
	if (!window) {
		return qfalse;
	}
	
	// Create Metal layer
	metal.metalLayer = [CAMetalLayer layer];
	if (!metal.metalLayer) {
		return qfalse;
	}
	
	metal.metalLayer.device = metal.device;
	metal.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	metal.metalLayer.framebufferOnly = YES;
	
	// Set layer as content layer for window
	window.contentView.layer = metal.metalLayer;
	window.contentView.wantsLayer = YES;
	metal.window = (__bridge void *)window;
#endif
	
	metal.width = width;
	metal.height = height;
	
	// Set drawable size
	CGSize drawableSize = CGSizeMake(width, height);
	metal.metalLayer.drawableSize = drawableSize;
	
	// Initialize 2D projection matrix
	Metal_Update2DProjection();
	
	return qtrue;
}

/*
================
Metal_CreateRenderTargets
================
*/
qboolean Metal_CreateRenderTargets(int width, int height) {
	if (!metal.device || !metal.metalLayer) {
		return qfalse;
	}
	
	// Depth texture descriptor
	MTLTextureDescriptor *depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
																						   width:width
																						  height:height
																					   mipmapped:NO];
	depthDesc.usage = MTLTextureUsageRenderTarget;
	depthDesc.storageMode = MTLStorageModePrivate;
	
	metal.depthTexture = [metal.device newTextureWithDescriptor:depthDesc];
	if (!metal.depthTexture) {
		Com_Printf("Metal: Failed to create depth texture\n");
		return qfalse;
	}
	
	metal.width = width;
	metal.height = height;
	
	return qtrue;
}

/*
================
Metal_LoadShaders
================
*/
qboolean Metal_LoadShaders(void) {
	if (!metal.device) {
		return qfalse;
	}
	
	// Try to load default library first
	NSError *error = nil;
	metal.shaderLibrary = [metal.device newDefaultLibrary];
	
	if (!metal.shaderLibrary) {
		Com_Printf("Metal: Failed to load default shader library\n");
		return qfalse;
	}
	
	Com_Printf("Metal: Shader library loaded\n");
	return qtrue;
}

/*
================
Metal_GetFunction
================
*/
id<MTLFunction> Metal_GetFunction(const char *name) {
	if (!metal.shaderLibrary) {
		return nil;
	}
	
	NSString *functionName = [NSString stringWithUTF8String:name];
	return [metal.shaderLibrary newFunctionWithName:functionName];
}

/*
================
Metal_CreateRenderPipeline
================
*/
qboolean Metal_CreateRenderPipeline(void) {
	if (!metal.device || !metal.shaderLibrary) {
		return qfalse;
	}
	
	// Get vertex and fragment shaders
	id<MTLFunction> vertexFunction = Metal_GetFunction("vertex_main");
	id<MTLFunction> fragmentFunction = Metal_GetFunction("fragment_main");
	
	if (!vertexFunction || !fragmentFunction) {
		Com_Printf("Metal: Failed to get shader functions\n");
		return qfalse;
	}
	
	// Create render pipeline descriptor
	MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	pipelineDesc.vertexFunction = vertexFunction;
	pipelineDesc.fragmentFunction = fragmentFunction;
	pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
	
	// Enable blending
	pipelineDesc.colorAttachments[0].blendingEnabled = YES;
	pipelineDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
	pipelineDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
	pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
	pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	
	NSError *error = nil;
	metal.renderPipelineState = [metal.device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
	
	if (!metal.renderPipelineState) {
		Com_Printf("Metal: Failed to create render pipeline: %s\n", [[error localizedDescription] UTF8String]);
		return qfalse;
	}
	
	Com_Printf("Metal: Render pipeline created\n");
	return qtrue;
}

/*
================
Metal_CreateUIPipeline
================
Create render pipeline state for UI/2D rendering
*/
qboolean Metal_CreateUIPipeline(void) {
	if (!metal.device || !metal.shaderLibrary) {
		return qfalse;
	}
	
	// Get UI shader functions
	id<MTLFunction> uiVertexFunction = Metal_GetFunction("ui_vertex");
	id<MTLFunction> uiFragmentFunction = Metal_GetFunction("ui_fragment");
	
	if (!uiVertexFunction || !uiFragmentFunction) {
		Com_Printf("Metal: Failed to get UI shader functions\n");
		return qfalse;
	}
	
	// Create UI render pipeline descriptor
	MTLRenderPipelineDescriptor *uiPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	uiPipelineDesc.vertexFunction = uiVertexFunction;
	uiPipelineDesc.fragmentFunction = uiFragmentFunction;
	uiPipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	uiPipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
	
	// Enable blending for UI
	uiPipelineDesc.colorAttachments[0].blendingEnabled = YES;
	uiPipelineDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
	uiPipelineDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
	uiPipelineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	uiPipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
	uiPipelineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	uiPipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	
	// Vertex descriptor for UI (position, texCoord, color)
	MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];
	vertexDesc.attributes[0].format = MTLVertexFormatFloat2; // position
	vertexDesc.attributes[0].offset = 0;
	vertexDesc.attributes[0].bufferIndex = 0;
	vertexDesc.attributes[1].format = MTLVertexFormatFloat2; // texCoord
	vertexDesc.attributes[1].offset = 8;
	vertexDesc.attributes[1].bufferIndex = 0;
	vertexDesc.attributes[2].format = MTLVertexFormatFloat4; // color
	vertexDesc.attributes[2].offset = 16;
	vertexDesc.attributes[2].bufferIndex = 0;
	vertexDesc.layouts[0].stride = 32; // sizeof(UIVertex)
	vertexDesc.layouts[0].stepRate = 1;
	vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
	
	uiPipelineDesc.vertexDescriptor = vertexDesc;
	
	NSError *error = nil;
	metal.uiPipelineState = [metal.device newRenderPipelineStateWithDescriptor:uiPipelineDesc error:&error];
	
	if (!metal.uiPipelineState) {
		Com_Printf("Metal: Failed to create UI render pipeline: %s\n", [[error localizedDescription] UTF8String]);
		return qfalse;
	}
	
	Com_Printf("Metal: UI render pipeline created\n");
	return qtrue;
}

/*
================
Metal_CreateDepthStencilState
================
*/
qboolean Metal_CreateDepthStencilState(void) {
	if (!metal.device) {
		return qfalse;
	}
	
	// 3D depth stencil state (with depth testing)
	MTLDepthStencilDescriptor *depthDesc = [[MTLDepthStencilDescriptor alloc] init];
	depthDesc.depthCompareFunction = MTLCompareFunctionLess;
	depthDesc.depthWriteEnabled = YES;
	
	metal.depthStencilState = [metal.device newDepthStencilStateWithDescriptor:depthDesc];
	if (!metal.depthStencilState) {
		Com_Printf("Metal: Failed to create depth stencil state\n");
		return qfalse;
	}
	
	// UI depth stencil state (no depth testing)
	MTLDepthStencilDescriptor *uiDepthDesc = [[MTLDepthStencilDescriptor alloc] init];
	uiDepthDesc.depthCompareFunction = MTLCompareFunctionAlways;
	uiDepthDesc.depthWriteEnabled = NO;
	
	metal.uiDepthStencilState = [metal.device newDepthStencilStateWithDescriptor:uiDepthDesc];
	if (!metal.uiDepthStencilState) {
		Com_Printf("Metal: Failed to create UI depth stencil state\n");
		return qfalse;
	}
	
	return qtrue;
}

/*
================
MetalAPI_Init
================
Low-level Metal API initialization
*/
/*
================
Metal_Update2DProjection
================
Update 2D projection matrix when screen size changes
*/
void Metal_Update2DProjection(void) {
	if (metal.width == 0 || metal.height == 0) {
		return;
	}
	
	// Create orthographic projection matrix for 2D
	// Converts screen coordinates (0,0 to width,height) to NDC (-1,-1 to 1,1)
	float projection[16] = {
		2.0f / metal.width,  0.0f,               0.0f, 0.0f,
		0.0f,              -2.0f / metal.height, 0.0f, 0.0f,
		0.0f,               0.0f,               1.0f, 0.0f,
		-1.0f,              1.0f,               0.0f, 1.0f
	};
	
	Com_Memcpy(metal.projection2D, projection, sizeof(projection));
}

/*
================
MetalAPI_Init
================
Low-level Metal API initialization
*/
qboolean MetalAPI_Init(void) {
	Com_Memset(&metal, 0, sizeof(metal));
	
	if (!Metal_CreateDevice()) {
		return qfalse;
	}
	
	metal.initialized = qtrue;
	return qtrue;
}

/*
================
MetalAPI_Shutdown
================
Low-level Metal API shutdown
*/
void MetalAPI_Shutdown(void) {
	if (!metal.initialized) {
		return;
	}
	
	// Wait for GPU to finish
	Metal_WaitForGPU();
	
	// Release resources
	metal.renderPipelineState = nil;
	metal.uiPipelineState = nil;
	metal.depthStencilState = nil;
	metal.uiDepthStencilState = nil;
	metal.shaderLibrary = nil;
	metal.vertexBuffer = nil;
	metal.indexBuffer = nil;
	metal.depthTexture = nil;
	metal.colorTexture = nil;
	metal.commandQueue = nil;
	metal.device = nil;
	metal.metalLayer = nil;
	
	if (metal.frameSemaphore) {
		dispatch_release(metal.frameSemaphore);
		metal.frameSemaphore = nil;
	}
	
	metal.initialized = qfalse;
}

/*
================
Metal_WaitForGPU
================
*/
void Metal_WaitForGPU(void) {
	if (metal.currentCommandBuffer) {
		[metal.currentCommandBuffer waitUntilCompleted];
		metal.currentCommandBuffer = nil;
	}
}

/*
================
MetalAPI_Present
================
Low-level Metal API present
*/
void MetalAPI_Present(void) {
	if (!metal.currentCommandBuffer || !metal.currentDrawable) {
		return;
	}
	
	// Present the drawable that was obtained in BeginFrame
	[metal.currentCommandBuffer presentDrawable:metal.currentDrawable];
	[metal.currentCommandBuffer commit];
	
	// Signal semaphore (if using triple buffering)
	if (metal.frameSemaphore) {
		metal.currentFrame = (metal.currentFrame + 1) % metal.maxFramesInFlight;
		dispatch_semaphore_signal(metal.frameSemaphore);
	}
	
	// Clear current frame state
	metal.currentCommandBuffer = nil;
	metal.currentDrawable = nil;
}

/*
================
Metal_ResizeSwapChain
================
*/
void Metal_ResizeSwapChain(int width, int height) {
	if (!metal.metalLayer) {
		return;
	}
	
	metal.width = width;
	metal.height = height;
	
	CGSize drawableSize = CGSizeMake(width, height);
	metal.metalLayer.drawableSize = drawableSize;
	
	// Recreate render targets
	Metal_CreateRenderTargets(width, height);
}

#endif // __APPLE__ && !__ANDROID__

