#ifdef USE_VPX

#include "client.h"
#include "cl_cin_codec.h"
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

// External access to cinematic table
extern cin_cache cinTable[MAX_VIDEO_HANDLES];
extern int currentHandle;

// VPX decoder state
typedef struct {
	vpx_codec_ctx_t decoder;
	vpx_codec_iface_t *iface;
	vpx_codec_dec_cfg_t cfg;
	
	vpx_codec_stream_info_t si;
	
	qboolean initialized;
	qboolean header_parsed;
	
	// Frame timing
	int frame_count;
	double fps;
	int last_frame_time;
	
	// File reading
	byte *file_buffer;
	int file_buffer_size;
	int file_buffer_pos;
	int file_size;
} vpx_data_t;

// Forward declarations
qboolean VPX_Init(int handle);
void VPX_Shutdown(int handle);
e_status VPX_Run(int handle);
void VPX_Reset(int handle);

// Helper to read WebM/VPX file data
static int VPX_ReadData(int handle, byte *buffer, int size) {
	vpx_data_t *data = (vpx_data_t *)cinTable[handle].codecData;
	int bytes_read = 0;
	int to_read;
	
	if (!data) return 0;
	
	// If we haven't loaded the file yet, load it all
	if (!data->file_buffer && cinTable[handle].ROQSize > 0) {
		data->file_size = cinTable[handle].ROQSize;
		data->file_buffer = (byte *)Z_Malloc(data->file_size, TAG_TEMP, qfalse);
		if (data->file_buffer) {
			FS_Seek(cinTable[handle].iFile, 0, FS_SEEK_SET);
			FS_Read(data->file_buffer, data->file_size, cinTable[handle].iFile);
			data->file_buffer_pos = 0;
		}
	}
	
	if (!data->file_buffer) return 0;
	
	to_read = size;
	if (data->file_buffer_pos + to_read > data->file_size) {
		to_read = data->file_size - data->file_buffer_pos;
	}
	
	if (to_read > 0) {
		Com_Memcpy(buffer, data->file_buffer + data->file_buffer_pos, to_read);
		data->file_buffer_pos += to_read;
		bytes_read = to_read;
	}
	
	return bytes_read;
}

// Convert YUV to RGB (similar to Theora)
static void VPX_YUVtoRGB(vpx_image_t *img, byte *rgb, int width, int height) {
	int x, y;
	int Y, Cb, Cr;
	int R, G, B;
	byte *y_plane = img->planes[0];
	byte *u_plane = img->planes[1];
	byte *v_plane = img->planes[2];
	int y_stride = img->stride[0];
	int u_stride = img->stride[1];
	int v_stride = img->stride[2];
	
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			Y = y_plane[y * y_stride + x];
			Cb = u_plane[(y / 2) * u_stride + (x / 2)] - 128;
			Cr = v_plane[(y / 2) * v_stride + (x / 2)] - 128;
			
			// YUV to RGB conversion
			R = Y + (int)(1.402f * Cr);
			G = Y - (int)(0.344f * Cb + 0.714f * Cr);
			B = Y + (int)(1.772f * Cb);
			
			// Clamp values
			if (R < 0) R = 0; else if (R > 255) R = 255;
			if (G < 0) G = 0; else if (G > 255) G = 255;
			if (B < 0) B = 0; else if (B > 255) B = 255;
			
			// Write RGBA (little-endian)
			rgb[(y * width + x) * 4 + 0] = R;
			rgb[(y * width + x) * 4 + 1] = G;
			rgb[(y * width + x) * 4 + 2] = B;
			rgb[(y * width + x) * 4 + 3] = 255;
		}
	}
}

/*
==================
VPX_Init

Initialize VP8/VP9 decoder
==================
*/
qboolean VPX_Init(int handle) {
	vpx_data_t *data;
	vpx_codec_err_t res;
	byte header[32];
	int bytes_read;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return qfalse;
	
	// Determine codec (VP8 or VP9)
	video_codec_t codec = cinTable[handle].codec;
	vpx_codec_iface_t *iface = NULL;
	
	if (codec == CODEC_VP8) {
		iface = vpx_codec_vp8_dx();
	} else if (codec == CODEC_VP9) {
		iface = vpx_codec_vp9_dx();
	}
	
	if (!iface) {
		Com_Printf("VPX_Init: unsupported codec\n");
		return qfalse;
	}
	
	// Allocate codec data
	data = (vpx_data_t *)Z_Malloc(sizeof(vpx_data_t), TAG_TEMP, qfalse);
	if (!data) {
		Com_Printf("VPX_Init: failed to allocate memory\n");
		return qfalse;
	}
	
	Com_Memset(data, 0, sizeof(vpx_data_t));
	cinTable[handle].codecData = data;
	data->iface = iface;
	
	// Reset file position
	FS_Seek(cinTable[handle].iFile, 0, FS_SEEK_SET);
	
	// Read header to determine dimensions (simplified - real WebM parsing is more complex)
	bytes_read = VPX_ReadData(handle, header, sizeof(header));
	if (bytes_read < 16) {
		Com_Printf("VPX_Init: failed to read header\n");
		VPX_Shutdown(handle);
		return qfalse;
	}
	
	// Initialize decoder config
	data->cfg.threads = 1;
	data->cfg.w = 0;  // Will be set from first frame
	data->cfg.h = 0;
	
	// Initialize decoder
	res = vpx_codec_dec_init(&data->decoder, iface, &data->cfg, 0);
	if (res != VPX_CODEC_OK) {
		Com_Printf("VPX_Init: failed to initialize decoder: %s\n", vpx_codec_error(&data->decoder));
		VPX_Shutdown(handle);
		return qfalse;
	}
	
	// Try to decode first frame to get dimensions
	// For now, use default dimensions - will be updated on first frame decode
	cinTable[handle].CIN_WIDTH = DEFAULT_CIN_WIDTH;
	cinTable[handle].CIN_HEIGHT = DEFAULT_CIN_HEIGHT;
	cinTable[handle].drawX = cinTable[handle].CIN_WIDTH;
	cinTable[handle].drawY = cinTable[handle].CIN_HEIGHT;
	
	// Initialize buffer
	cinTable[handle].samplesPerPixel = 4; // RGBA
	cinTable[handle].samplesPerLine = cinTable[handle].CIN_WIDTH * 4;
	cinTable[handle].screenDelta = cinTable[handle].CIN_HEIGHT * cinTable[handle].samplesPerLine;
	
	// Allocate frame buffer if needed
	if (!cinTable[handle].buf) {
		cinTable[handle].buf = Z_Malloc(cinTable[handle].CIN_WIDTH * cinTable[handle].CIN_HEIGHT * 4, TAG_TEMP, qfalse);
		if (!cinTable[handle].buf) {
			Com_Printf("VPX_Init: failed to allocate frame buffer\n");
			VPX_Shutdown(handle);
			return qfalse;
		}
	}
	
	data->initialized = qtrue;
	data->frame_count = 0;
	data->fps = 30.0; // Default, will be updated if available
	data->last_frame_time = CL_ScaledMilliseconds();
	
	cinTable[handle].startTime = cinTable[handle].lastTime = CL_ScaledMilliseconds();
	
	Com_DPrintf("VPX_Init: initialized %s decoder\n", (codec == CODEC_VP8) ? "VP8" : "VP9");
	
	return qtrue;
}

/*
==================
VPX_Shutdown

Clean up VPX decoder
==================
*/
void VPX_Shutdown(int handle) {
	vpx_data_t *data;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return;
	
	data = (vpx_data_t *)cinTable[handle].codecData;
	if (!data) return;
	
	if (data->initialized) {
		vpx_codec_destroy(&data->decoder);
		data->initialized = qfalse;
	}
	
	if (data->file_buffer) {
		Z_Free(data->file_buffer);
		data->file_buffer = NULL;
	}
	
	// Note: Frame buffer (cinTable[handle].buf) is managed by main cinematic system
	// and will be freed when the handle is closed
	
	Z_Free(data);
	cinTable[handle].codecData = NULL;
}

/*
==================
VPX_Run

Decode and display next frame
==================
*/
e_status VPX_Run(int handle) {
	vpx_data_t *data;
	vpx_codec_iter_t iter = NULL;
	vpx_image_t *img;
	byte frame_data[65536];
	int frame_size;
	int current_time;
	int frame_delay;
	vpx_codec_err_t res;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) {
		return FMV_EOF;
	}
	
	data = (vpx_data_t *)cinTable[handle].codecData;
	if (!data || !data->initialized) {
		return FMV_EOF;
	}
	
	// Check timing
	current_time = CL_ScaledMilliseconds();
	frame_delay = (int)(1000.0 / data->fps);
	
	if (current_time - data->last_frame_time < frame_delay) {
		return cinTable[handle].status; // Not time for next frame yet
	}
	
	// Read frame data (simplified - real WebM parsing needs proper container format handling)
	// For now, try to read a chunk and decode it
	frame_size = VPX_ReadData(handle, frame_data, sizeof(frame_data));
	if (frame_size <= 0) {
		// End of file
		if (cinTable[handle].looping) {
			VPX_Reset(handle);
			return cinTable[handle].status;
		} else {
			cinTable[handle].status = FMV_EOF;
			return FMV_EOF;
		}
	}
	
	// Decode frame
	res = vpx_codec_decode(&data->decoder, frame_data, frame_size, NULL, 0);
	if (res != VPX_CODEC_OK) {
		// Try to continue - might be partial frame or need more data
		return cinTable[handle].status;
	}
	
	// Get decoded frame
	img = vpx_codec_get_frame(&data->decoder, &iter);
	if (img) {
		// Update dimensions if this is first frame
		if (cinTable[handle].CIN_WIDTH != (int)img->d_w || cinTable[handle].CIN_HEIGHT != (int)img->d_h) {
			cinTable[handle].CIN_WIDTH = img->d_w;
			cinTable[handle].CIN_HEIGHT = img->d_h;
			cinTable[handle].drawX = cinTable[handle].CIN_WIDTH;
			cinTable[handle].drawY = cinTable[handle].CIN_HEIGHT;
			cinTable[handle].samplesPerLine = cinTable[handle].CIN_WIDTH * 4;
			cinTable[handle].screenDelta = cinTable[handle].CIN_HEIGHT * cinTable[handle].samplesPerLine;
			
			// Reallocate buffer if needed
			if (cinTable[handle].buf) {
				Z_Free(cinTable[handle].buf);
			}
			cinTable[handle].buf = Z_Malloc(cinTable[handle].CIN_WIDTH * cinTable[handle].CIN_HEIGHT * 4, TAG_TEMP, qfalse);
		}
		
		// Convert YUV to RGB
		if (cinTable[handle].buf) {
			VPX_YUVtoRGB(img, cinTable[handle].buf, 
				cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT);
			cinTable[handle].dirty = qtrue;
		}
		
		data->frame_count++;
		data->last_frame_time = current_time;
	}
	
	return cinTable[handle].status;
}

/*
==================
VPX_Reset

Reset decoder to beginning
==================
*/
void VPX_Reset(int handle) {
	vpx_data_t *data;
	vpx_codec_err_t res;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return;
	
	data = (vpx_data_t *)cinTable[handle].codecData;
	if (!data) return;
	
	// Reset file position
	if (data->file_buffer) {
		data->file_buffer_pos = 0;
	} else {
		if (cinTable[handle].iFile != FS_INVALID_HANDLE) {
			FS_Seek(cinTable[handle].iFile, 0, FS_SEEK_SET);
		}
	}
	
	// Destroy and recreate decoder
	if (data->initialized) {
		vpx_codec_destroy(&data->decoder);
		data->initialized = qfalse;
	}
	
	// Reinitialize decoder
	res = vpx_codec_dec_init(&data->decoder, data->iface, &data->cfg, 0);
	if (res != VPX_CODEC_OK) {
		Com_Printf("VPX_Reset: failed to reinitialize decoder: %s\n", vpx_codec_error(&data->decoder));
		cinTable[handle].status = FMV_EOF;
		return;
	}
	
	data->initialized = qtrue;
	data->frame_count = 0;
	data->last_frame_time = CL_ScaledMilliseconds();
	cinTable[handle].startTime = cinTable[handle].lastTime = CL_ScaledMilliseconds();
	
	cinTable[handle].status = FMV_LOOPED;
}

#endif // USE_VPX

