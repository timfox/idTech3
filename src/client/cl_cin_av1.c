#ifdef USE_DAV1D

#include "client.h"
#include "cl_cin_codec.h"
#include <dav1d/dav1d.h>
#include <string.h>
#include <errno.h>

// AV1 decoder state
typedef struct {
	Dav1dContext *ctx;
	Dav1dSettings settings;
	
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
	
	// Current frame
	Dav1dPicture pic;
	qboolean frame_ready;
} av1_data_t;

// Forward declarations
qboolean AV1_Init(int handle);
void AV1_Shutdown(int handle);
e_status AV1_Run(int handle);
void AV1_Reset(int handle);

// Helper to read AV1 file data
static int AV1_ReadData(int handle, byte *buffer, int size) {
	av1_data_t *data = (av1_data_t *)cinTable[handle].codecData;
	int bytes_read = 0;
	int to_read;
	
	if (!data) return 0;
	
	// If we haven't loaded the file yet, load it all
	if (!data->file_buffer && cinTable[handle].ROQSize > 0) {
		data->file_size = cinTable[handle].ROQSize;
		data->file_buffer = (byte *)Z_Malloc(data->file_size);
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

// Convert YUV to RGB (similar to VPX/Theora)
static void AV1_YUVtoRGB(Dav1dPicture *pic, byte *rgb, int width, int height) {
	int x, y;
	int Y, Cb, Cr;
	int R, G, B;
	
	// Get plane pointers and strides
	const uint8_t *y_plane = pic->data[0];
	const uint8_t *u_plane = pic->data[1];
	const uint8_t *v_plane = pic->data[2];
	int y_stride = pic->stride[0];
	int u_stride = pic->stride[1];
	int v_stride = pic->stride[2];
	
	// AV1 uses 4:2:0 chroma subsampling
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			Y = y_plane[y * y_stride + x];
			Cb = u_plane[(y / 2) * u_stride + (x / 2)] - 128;
			Cr = v_plane[(y / 2) * v_stride + (x / 2)] - 128;
			
			// YUV to RGB conversion (ITU-R BT.601)
			R = Y + (int)(1.402f * Cr);
			G = Y - (int)(0.344f * Cb + 0.714f * Cr);
			B = Y + (int)(1.772f * Cb);
			
			// Clamp values
			if (R < 0) R = 0; else if (R > 255) R = 255;
			if (G < 0) G = 0; else if (G > 255) G = 255;
			if (B < 0) B = 0; else if (B > 255) B = 255;
			
			// Write RGBA
			rgb[(y * width + x) * 4 + 0] = R;
			rgb[(y * width + x) * 4 + 1] = G;
			rgb[(y * width + x) * 4 + 2] = B;
			rgb[(y * width + x) * 4 + 3] = 255;
		}
	}
}

/*
==================
AV1_Init

Initialize AV1 decoder
==================
*/
qboolean AV1_Init(int handle) {
	av1_data_t *data;
	byte header[64];
	int bytes_read;
	int res;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return qfalse;
	
	// Allocate codec data
	data = (av1_data_t *)Z_Malloc(sizeof(av1_data_t));
	if (!data) {
		Com_Printf("AV1_Init: failed to allocate memory\n");
		return qfalse;
	}
	
	Com_Memset(data, 0, sizeof(av1_data_t));
	cinTable[handle].codecData = data;
	
	// Initialize dav1d settings
	dav1d_default_settings(&data->settings);
	data->settings.n_threads = 1; // Single-threaded for now
	data->settings.max_frame_delay = 1;
	
	// Create decoder context
	res = dav1d_open(&data->ctx, &data->settings);
	if (res != 0) {
		Com_Printf("AV1_Init: failed to create decoder context: %s\n", strerror(-res));
		AV1_Shutdown(handle);
		return qfalse;
	}
	
	// Reset file position
	FS_Seek(cinTable[handle].iFile, 0, FS_SEEK_SET);
	
	// Read header (for detection, actual parsing happens in AV1_Run)
	bytes_read = AV1_ReadData(handle, header, sizeof(header));
	if (bytes_read < 16) {
		Com_Printf("AV1_Init: failed to read header\n");
		AV1_Shutdown(handle);
		return qfalse;
	}
	
	// Use default dimensions initially - will be updated on first frame decode
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
		cinTable[handle].buf = Z_Malloc(cinTable[handle].CIN_WIDTH * cinTable[handle].CIN_HEIGHT * 4);
		if (!cinTable[handle].buf) {
			Com_Printf("AV1_Init: failed to allocate frame buffer\n");
			AV1_Shutdown(handle);
			return qfalse;
		}
	}
	
	data->initialized = qtrue;
	data->frame_count = 0;
	data->fps = 30.0; // Default, will be updated if available
	data->last_frame_time = CL_ScaledMilliseconds();
	data->frame_ready = qfalse;
	
	cinTable[handle].startTime = cinTable[handle].lastTime = CL_ScaledMilliseconds();
	
	Com_DPrintf("AV1_Init: initialized AV1 decoder\n");
	
	return qtrue;
}

/*
==================
AV1_Shutdown

Clean up AV1 decoder
==================
*/
void AV1_Shutdown(int handle) {
	av1_data_t *data;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return;
	
	data = (av1_data_t *)cinTable[handle].codecData;
	if (!data) return;
	
	if (data->initialized) {
		// Unref any pending picture
		if (data->frame_ready) {
			dav1d_picture_unref(&data->pic);
			data->frame_ready = qfalse;
		}
		
		// Close decoder context
		if (data->ctx) {
			dav1d_close(&data->ctx);
			data->ctx = NULL;
		}
		
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
AV1_Run

Decode and display next frame
==================
*/
e_status AV1_Run(int handle) {
	av1_data_t *data;
	Dav1dData dav1d_data;
	byte frame_data[65536];
	int frame_size;
	int current_time;
	int frame_delay;
	int res;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) {
		return FMV_EOF;
	}
	
	data = (av1_data_t *)cinTable[handle].codecData;
	if (!data || !data->initialized || !data->ctx) {
		return FMV_EOF;
	}
	
	// Check timing
	current_time = CL_ScaledMilliseconds();
	frame_delay = (int)(1000.0 / data->fps);
	
	if (current_time - data->last_frame_time < frame_delay) {
		return cinTable[handle].status; // Not time for next frame yet
	}
	
	// If we have a frame ready, use it
	if (data->frame_ready) {
		// Update dimensions if this is first frame
		if (cinTable[handle].CIN_WIDTH != (int)data->pic.p.w || 
		    cinTable[handle].CIN_HEIGHT != (int)data->pic.p.h) {
			cinTable[handle].CIN_WIDTH = data->pic.p.w;
			cinTable[handle].CIN_HEIGHT = data->pic.p.h;
			cinTable[handle].drawX = cinTable[handle].CIN_WIDTH;
			cinTable[handle].drawY = cinTable[handle].CIN_HEIGHT;
			cinTable[handle].samplesPerLine = cinTable[handle].CIN_WIDTH * 4;
			cinTable[handle].screenDelta = cinTable[handle].CIN_HEIGHT * cinTable[handle].samplesPerLine;
			
			// Reallocate buffer if needed
			if (cinTable[handle].buf) {
				Z_Free(cinTable[handle].buf);
			}
			cinTable[handle].buf = Z_Malloc(cinTable[handle].CIN_WIDTH * cinTable[handle].CIN_HEIGHT * 4);
		}
		
		// Convert YUV to RGB
		if (cinTable[handle].buf) {
			AV1_YUVtoRGB(&data->pic, cinTable[handle].buf, 
				cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT);
			cinTable[handle].dirty = qtrue;
		}
		
		// Unref the picture
		dav1d_picture_unref(&data->pic);
		data->frame_ready = qfalse;
		
		data->frame_count++;
		data->last_frame_time = current_time;
	}
	
	// Try to read and decode next frame
	// For AV1 in WebM, we need to parse the container format properly
	// For now, try to read a chunk and feed it to dav1d
	frame_size = AV1_ReadData(handle, frame_data, sizeof(frame_data));
	if (frame_size <= 0) {
		// End of file - try to flush decoder
		res = dav1d_send_data(data->ctx, NULL);
		if (res == 0) {
			// Try to get any remaining frames
			res = dav1d_get_picture(data->ctx, &data->pic);
			if (res == 0) {
				data->frame_ready = qtrue;
				return cinTable[handle].status;
			}
		}
		
		// No more frames
		if (cinTable[handle].looping) {
			AV1_Reset(handle);
			return cinTable[handle].status;
		} else {
			cinTable[handle].status = FMV_EOF;
			return FMV_EOF;
		}
	}
	
	// Allocate dav1d data buffer
	// Note: For proper WebM/AV1 support, we'd need to parse the WebM container
	// to extract AV1 OBU frames. This is a simplified implementation.
	res = dav1d_data_wrap(&dav1d_data, frame_data, frame_size, NULL, NULL);
	if (res != 0) {
		return cinTable[handle].status;
	}
	
	// Send data to decoder
	// dav1d_send_data takes ownership of the data, so we don't need to unref it
	res = dav1d_send_data(data->ctx, &dav1d_data);
	if (res < 0 && res != -EAGAIN) {
		// Error decoding - unref the data since send_data failed
		dav1d_data_unref(&dav1d_data);
		return cinTable[handle].status;
	}
	
	// Try to get decoded frame
	// Loop to handle multiple frames from one data packet
	do {
		res = dav1d_get_picture(data->ctx, &data->pic);
		if (res == 0) {
			data->frame_ready = qtrue;
			// Process this frame on next call
			break;
		} else if (res == -EAGAIN) {
			// Need more data, continue reading
			break;
		} else {
			// Error
			break;
		}
	} while (0);
	
	return cinTable[handle].status;
}

/*
==================
AV1_Reset

Reset decoder to beginning
==================
*/
void AV1_Reset(int handle) {
	av1_data_t *data;
	int res;
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return;
	
	data = (av1_data_t *)cinTable[handle].codecData;
	if (!data) return;
	
	// Reset file position
	if (data->file_buffer) {
		data->file_buffer_pos = 0;
	} else {
		if (cinTable[handle].iFile != FS_INVALID_HANDLE) {
			FS_Seek(cinTable[handle].iFile, 0, FS_SEEK_SET);
		}
	}
	
	// Unref any pending picture
	if (data->frame_ready) {
		dav1d_picture_unref(&data->pic);
		data->frame_ready = qfalse;
	}
	
	// Close and recreate decoder context
	if (data->ctx) {
		dav1d_close(&data->ctx);
		data->ctx = NULL;
	}
	
	// Recreate decoder
	res = dav1d_open(&data->ctx, &data->settings);
	if (res != 0) {
		Com_Printf("AV1_Reset: failed to recreate decoder: %s\n", strerror(-res));
		cinTable[handle].status = FMV_EOF;
		return;
	}
	
	data->initialized = qtrue;
	data->frame_count = 0;
	data->last_frame_time = CL_ScaledMilliseconds();
	cinTable[handle].startTime = cinTable[handle].lastTime = CL_ScaledMilliseconds();
	
	cinTable[handle].status = FMV_LOOPED;
}

/*
==================
AV1_RegisterCodec

Register AV1 codec with the system
==================
*/
void AV1_RegisterCodec(void) {
	// Registration is handled in cl_cin_codec.c
	Com_DPrintf("AV1 codec registered\n");
}

#endif // USE_DAV1D

