#if defined(__APPLE__) && !defined(__ANDROID__)

#include "tr_local.h"
#include "../qcommon/qcommon.h"
#include "../renderer/tr_common.h" // For imgFlags_t
#import "metal.h"
#import <Metal/Metal.h>

// Forward declarations
extern metalContext_t metal;
extern metalRenderer_t tr;

/*
================
Metal_UploadTexture
================
Upload texture data to Metal
*/
static qboolean Metal_UploadTexture(image_t *image, byte *data, int width, int height, imgFlags_t flags) {
	if (!metal.device || !data || !image) {
		return qfalse;
	}
	
	// Determine pixel format
	MTLPixelFormat pixelFormat = MTLPixelFormatRGBA8Unorm;
	if (flags & IMGFLAG_RGB) {
		// RGB format (no alpha)
		pixelFormat = MTLPixelFormatRGB8Unorm;
	}
	
	// Create texture descriptor
	MTLTextureDescriptor *textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
																						   width:width
																						  height:height
																					   mipmapped:(flags & IMGFLAG_MIPMAP) ? YES : NO];
	textureDesc.usage = MTLTextureUsageShaderRead;
	textureDesc.storageMode = MTLStorageModeManaged;
	
	// Create texture
	id<MTLTexture> texture = [metal.device newTextureWithDescriptor:textureDesc];
	if (!texture) {
		Com_Printf("Metal: Failed to create texture\n");
		return qfalse;
	}
	
	// Calculate bytes per row
	int bytesPerRow = width * 4; // RGBA
	if (flags & IMGFLAG_RGB) {
		bytesPerRow = width * 3; // RGB
	}
	
	// Upload texture data
	MTLRegion region = MTLRegionMake2D(0, 0, width, height);
	[texture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
	
	// Create sampler state
	MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
	if (flags & IMGFLAG_CLAMPTOEDGE) {
		samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
	} else {
		samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
		samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;
	}
	
	if (flags & IMGFLAG_MIPMAP) {
		samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.mipFilter = MTLSamplerMipFilterLinear;
	} else {
		samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
	}
	
	id<MTLSamplerState> sampler = [metal.device newSamplerStateWithDescriptor:samplerDesc];
	if (!sampler) {
		Com_Printf("Metal: Failed to create sampler state\n");
		return qfalse;
	}
	
	// Store texture and sampler in image structure
	image->metalTexture = (__bridge_retained void*)texture;
	image->metalSampler = (__bridge_retained void*)sampler;
	image->width = width;
	image->height = height;
	image->uploadWidth = width;
	image->uploadHeight = height;
	image->flags = flags;
	
	return qtrue;
}

/*
================
Metal_BindTexture
================
Bind texture for rendering
*/
void Metal_BindTexture(image_t *image) {
	if (!metal.currentRenderEncoder || !image) {
		return;
	}
	
	id<MTLTexture> texture = (__bridge id<MTLTexture>)image->metalTexture;
	id<MTLSamplerState> sampler = (__bridge id<MTLSamplerState>)image->metalSampler;
	
	if (texture) {
		[metal.currentRenderEncoder setFragmentTexture:texture atIndex:0];
		if (sampler) {
			[metal.currentRenderEncoder setFragmentSamplerState:sampler atIndex:0];
		}
		tr.currentImage = image;
		image->frameUsed = tr.frameTime; // Track usage
	}
}

/*
================
Metal_CreateImage
================
Create image from data (called from renderercommon)
Note: This is a helper function. The actual R_CreateImage is in renderercommon
and will call the renderer-specific upload function.
*/
image_t *Metal_CreateImage(const char *name, const byte *pic, int width, int height, imgFlags_t flags) {
	image_t *image;
	
	if (!name || !pic) {
		return NULL;
	}
	
	// Allocate image structure (this should match the structure from renderercommon)
	// For now, we'll use a simplified version
	image = (image_t*)ri.Hunk_Alloc(sizeof(image_t), h_low);
	if (!image) {
		return NULL;
	}
	
	Com_Memset(image, 0, sizeof(image_t));
	Q_strncpyz(image->imgName, name, sizeof(image->imgName));
	image->flags = flags;
	
	// Upload texture data
	if (!Metal_UploadTexture(image, (byte*)pic, width, height, flags)) {
		return NULL;
	}
	
	Com_Printf("Metal: Created image %s (%dx%d)\n", name, width, height);
	return image;
}

#endif // __APPLE__ && !__ANDROID__

