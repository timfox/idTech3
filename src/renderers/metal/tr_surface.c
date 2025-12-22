#if defined(__APPLE__) && !defined(__ANDROID__)

#include "tr_local.h"
#include "../common/qcommon.h"
#import "metal.h"
#import <Metal/Metal.h>

// Forward declarations
extern metalContext_t metal;
extern metalRenderer_t tr;

// Vertex structure for 2D UI rendering
typedef struct {
	float position[2];
	float texCoord[2];
	float color[4];
} UIVertex;

/*
================
Metal_AddQuad2D
================
Add a 2D quad to the render queue
*/
static void Metal_AddQuad2D(float x, float y, float w, float h, 
							float s1, float t1, float s2, float t2,
							const float *color) {
	if (!metal.currentRenderEncoder) {
		return;
	}
	
	// Create quad vertices
	UIVertex vertices[4];
	
	// Bottom-left
	vertices[0].position[0] = x;
	vertices[0].position[1] = y + h;
	vertices[0].texCoord[0] = s1;
	vertices[0].texCoord[1] = t2;
	vertices[0].color[0] = color[0];
	vertices[0].color[1] = color[1];
	vertices[0].color[2] = color[2];
	vertices[0].color[3] = color[3];
	
	// Bottom-right
	vertices[1].position[0] = x + w;
	vertices[1].position[1] = y + h;
	vertices[1].texCoord[0] = s2;
	vertices[1].texCoord[1] = t2;
	vertices[1].color[0] = color[0];
	vertices[1].color[1] = color[1];
	vertices[1].color[2] = color[2];
	vertices[1].color[3] = color[3];
	
	// Top-right
	vertices[2].position[0] = x + w;
	vertices[2].position[1] = y;
	vertices[2].texCoord[0] = s2;
	vertices[2].texCoord[1] = t1;
	vertices[2].color[0] = color[0];
	vertices[2].color[1] = color[1];
	vertices[2].color[2] = color[2];
	vertices[2].color[3] = color[3];
	
	// Top-left
	vertices[3].position[0] = x;
	vertices[3].position[1] = y;
	vertices[3].texCoord[0] = s1;
	vertices[3].texCoord[1] = t1;
	vertices[3].color[0] = color[0];
	vertices[3].color[1] = color[1];
	vertices[3].color[2] = color[2];
	vertices[3].color[3] = color[3];
	
	// Create index buffer for quad (two triangles)
	uint16_t indices[6] = {
		0, 1, 2,  // First triangle
		0, 2, 3   // Second triangle
	};
	
	// Allocate vertex buffer
	size_t vertexBufferSize = sizeof(vertices);
	id<MTLBuffer> vertexBuffer = [metal.device newBufferWithBytes:vertices
														   length:vertexBufferSize
														  options:MTLResourceOptionCPUCacheModeDefault];
	
	if (!vertexBuffer) {
		Com_Printf("Metal: Failed to create vertex buffer for quad\n");
		return;
	}
	
	// Allocate index buffer
	size_t indexBufferSize = sizeof(indices);
	id<MTLBuffer> indexBuffer = [metal.device newBufferWithBytes:indices
														  length:indexBufferSize
														 options:MTLResourceOptionCPUCacheModeDefault];
	
	if (!indexBuffer) {
		Com_Printf("Metal: Failed to create index buffer for quad\n");
		return;
	}
	
	// Set vertex buffer
	[metal.currentRenderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
	
	// Draw quad
	[metal.currentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
										   indexCount:6
											indexType:MTLIndexTypeUInt16
										  indexBuffer:indexBuffer
									indexBufferOffset:0];
	
	// Update statistics
	tr.drawCalls++;
	tr.triangles += 2;
	tr.vertices += 4;
}

/*
================
Metal_Setup2DProjection
================
Setup 2D orthographic projection for UI rendering
*/
void Metal_Setup2DProjection(void) {
	if (!metal.currentRenderEncoder) {
		return;
	}
	
	// Set UI render pipeline state
	if (metal.uiPipelineState) {
		[metal.currentRenderEncoder setRenderPipelineState:metal.uiPipelineState];
	}
	
	// Set UI depth stencil state (no depth testing)
	if (metal.uiDepthStencilState) {
		[metal.currentRenderEncoder setDepthStencilState:metal.uiDepthStencilState];
	}
	
	// Set viewport for 2D rendering (screen coordinates)
	MTLViewport viewport;
	viewport.originX = 0.0;
	viewport.originY = 0.0;
	viewport.width = (double)tr.width;
	viewport.height = (double)tr.height;
	viewport.znear = 0.0;
	viewport.zfar = 1.0;
	
	[metal.currentRenderEncoder setViewport:viewport];
	
	// Create orthographic projection matrix for 2D
	// Converts screen coordinates (0,0 to width,height) to NDC (-1,-1 to 1,1)
	float projection[16] = {
		2.0f / tr.width,  0.0f,             0.0f, 0.0f,
		0.0f,            -2.0f / tr.height, 0.0f, 0.0f,
		0.0f,             0.0f,             1.0f, 0.0f,
		-1.0f,            1.0f,             0.0f, 1.0f
	};
	
	// Upload projection matrix as uniform buffer
	id<MTLBuffer> uniformBuffer = [metal.device newBufferWithBytes:projection
															length:sizeof(projection)
														   options:MTLResourceOptionCPUCacheModeDefault];
	if (uniformBuffer) {
		[metal.currentRenderEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
	}
}

#endif // __APPLE__ && !__ANDROID__

