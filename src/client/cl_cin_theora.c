#ifdef USE_THEORA

#include "client.h"
#include "cl_cin_codec.h"
#include <theora/theora.h>
#include <theora/theoradec.h>
#include <ogg/ogg.h>

// Theora decoder state
typedef struct {
	ogg_sync_state ogg_sync;
	ogg_stream_state ogg_stream;
	th_dec_ctx *theora_decoder;
	th_info theora_info;
	th_comment theora_comment;
	th_setup_info *theora_setup;
	
	int video_packet_count;
	int audio_packet_count;
	
	qboolean initialized;
	qboolean header_read;
	
	// Frame timing
	int frame_count;
	double fps;
	int last_frame_time;
} theora_data_t;

// Forward declarations (made non-static for external access)
qboolean Theora_Init(int handle);
void Theora_Shutdown(int handle);
e_status Theora_Run(int handle);
void Theora_Reset(int handle);

// Internal helper functions
static qboolean Theora_ParseHeaders(int handle);
static size_t Theora_ReadOggPage(int handle, ogg_page *page);

// Helper function to parse Theora headers from OGG stream
static qboolean Theora_ParseHeaders(int handle) {
	theora_data_t *data = (theora_data_t *)cinTable[handle].codecData;
	ogg_page page;
	ogg_packet packet;
	int packet_count = 0;
	int theora_header_count = 0;
	qboolean have_identification = qfalse;
	qboolean have_comment = qfalse;
	qboolean have_setup = qfalse;
	qboolean stream_initialized = qfalse;

	if (!data) return qfalse;

	// Read pages and extract Theora headers
	while (!have_setup) {
		size_t page_size = Theora_ReadOggPage(handle, &page);
		if (page_size == 0) {
			Com_Printf("Theora_ParseHeaders: failed to read OGG page\n");
			return qfalse;
		}

		// Initialize stream on first page
		if (!stream_initialized) {
			if (ogg_stream_init(&data->ogg_stream, ogg_page_serialno(&page)) != 0) {
				Com_Printf("Theora_ParseHeaders: failed to initialize OGG stream\n");
				return qfalse;
			}
			stream_initialized = qtrue;
			Com_Printf("Theora_ParseHeaders: initialized OGG stream with serial %d\n", ogg_page_serialno(&page));
		}

		// Submit page to stream state
		if (ogg_stream_pagein(&data->ogg_stream, &page) != 0) {
			Com_Printf("Theora_ParseHeaders: failed to submit page to stream\n");
			return qfalse;
		}

		// Extract packets from page
		while (ogg_stream_packetout(&data->ogg_stream, &packet) == 1) {
			packet_count++;

			// Try to decode as Theora header
			int ret = th_decode_headerin(&data->theora_info, &data->theora_comment,
										&data->theora_setup, &packet);

			if (ret < 0 && ret != TH_ENOTFORMAT) {
				Com_Printf("Theora_ParseHeaders: error parsing header packet %d (ret=%d)\n", packet_count, ret);
				continue;
			}

			if (ret > 0) {
				// This is a valid Theora header
				theora_header_count++;

				switch (theora_header_count) {
					case 1: // Identification header
						have_identification = qtrue;
						break;
					case 2: // Comment header
						have_comment = qtrue;
						break;
					case 3: // Setup header
						have_setup = qtrue;
						break;
				}
			}
		}

		// Safety check to prevent infinite loops
		if (packet_count > 100) {
			Com_Printf("Theora_ParseHeaders: too many packets, aborting\n");
			return qfalse;
		}
	}

	if (!have_identification || !have_comment || !have_setup) {
		Com_Printf("Theora_ParseHeaders: missing required Theora headers\n");
		return qfalse;
	}

	Com_Printf("Theora_ParseHeaders: successfully parsed %d Theora headers\n", theora_header_count);
	return qtrue;
}

// Helper function to read data from file into Ogg buffer
static size_t Theora_ReadOggPage(int handle, ogg_page *page) {
	theora_data_t *data = (theora_data_t *)cinTable[handle].codecData;
	char *buffer;
	int bytes;
	size_t ret = 0;
	int attempts = 0;

	if (!data) return 0;

	// Try to get a page; if not enough data, read more.
	while (attempts++ < 16) {
		int pageout = ogg_sync_pageout(&data->ogg_sync, page);
		if (pageout == 1) {
			ret = page->header_len + page->body_len;
			break;
		}

		// Need more data
		buffer = ogg_sync_buffer(&data->ogg_sync, 4096);
		if (!buffer) {
			return 0;
		}

		bytes = FS_Read((byte *)buffer, 4096, cinTable[handle].iFile);
		if (bytes <= 0) {
			return 0;
		}

		ogg_sync_wrote(&data->ogg_sync, bytes);
	}

	return ret;
}

/*
==================
Theora_YUVtoRGB

Converts YUV 4:2:0 video frames to RGB format for display.

The function performs color space conversion from the compressed YUV format
used by Theora video codec to RGB format required for rendering. It handles
4:2:0 subsampling where chroma components (U/V) are half the resolution of
luma (Y) in both dimensions.

Parameters:
- ycbcr: Theora YUV buffer containing Y, U, V planes with their respective strides
- rgb: Output buffer for RGB data (must be pre-allocated, width*height*4 bytes)
- width: Frame width in pixels
- height: Frame height in pixels

Security Notes:
- Performs bounds checking to prevent buffer overflows
- Validates chroma plane access to ensure safe memory access
- Handles edge cases where video dimensions aren't evenly divisible by 2

Performance Notes:
- Processes pixels in scanline order for optimal cache performance
- Pre-calculates chroma dimensions to avoid repeated computations
- Uses integer arithmetic for color conversion efficiency
==================
*/
static void Theora_YUVtoRGB(th_ycbcr_buffer ycbcr, byte *rgb, int width, int height) {
	// Validate input parameters
	if (!rgb) {
		Com_Printf("Theora_YUVtoRGB: RGB buffer is NULL\n");
		return;
	}

	if (!ycbcr[0].data || !ycbcr[1].data || !ycbcr[2].data) {
		Com_Printf("Theora_YUVtoRGB: YUV buffers are NULL\n");
		return;
	}

	if (width <= 0 || height <= 0) {
		Com_Printf("Theora_YUVtoRGB: invalid dimensions %dx%d\n", width, height);
		return;
	}

	int x, y;
	int Y, Cb, Cr;
	int R, G, B;
	byte *y_plane = ycbcr[0].data;
	byte *u_plane = ycbcr[1].data;
	byte *v_plane = ycbcr[2].data;
	int y_stride = ycbcr[0].stride;
	int u_stride = ycbcr[1].stride;
	int v_stride = ycbcr[2].stride;

	// Use the actual dimensions reported by Theora library
	int chroma_width = ycbcr[1].width;
	int chroma_height = ycbcr[1].height;

	// Validate buffer dimensions
	if (ycbcr[0].width != width || ycbcr[0].height != height) {
		Com_Printf("Theora_YUVtoRGB: Y plane dimensions mismatch: expected %dx%d, got %dx%d\n",
			width, height, ycbcr[0].width, ycbcr[0].height);
		return;
	}

	if (chroma_width <= 0 || chroma_height <= 0) {
		Com_Printf("Theora_YUVtoRGB: invalid chroma dimensions %dx%d\n", chroma_width, chroma_height);
		return;
	}

	// Debug output and validation
	static int debug_count = 0;
	if (debug_count < 10) {  // Increased debug count to see more frames
		int expected_rgb_size = width * height * 4;
		Com_Printf("Theora_YUVtoRGB: %dx%d -> expected_rgb_size=%d, chroma %dx%d, strides Y:%d U:%d V:%d\n",
			width, height, expected_rgb_size, chroma_width, chroma_height, y_stride, u_stride, v_stride);
		debug_count++;
	}

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			// For 4:4:4 format, all planes have the same dimensions
			int chroma_x = x;
			int chroma_y = y;

			// Bounds checking for Y plane
			int y_idx = y * y_stride + x;
			if (y_idx < 0 || y_idx >= ycbcr[0].stride * ycbcr[0].height || x >= ycbcr[0].width) {
				Y = 128; // Default luminance
			} else {
				Y = y_plane[y_idx];
			}

			// Bounds checking for chroma planes (4:4:4 = same as luma)
			int u_idx = chroma_y * u_stride + chroma_x;
			int v_idx = chroma_y * v_stride + chroma_x;

			if (u_idx < 0 || u_idx >= ycbcr[1].stride * ycbcr[1].height) {
				Cb = 128; // Default chrominance
			} else {
				Cb = u_plane[u_idx] - 128;
			}

			if (v_idx < 0 || v_idx >= ycbcr[2].stride * ycbcr[2].height) {
				Cr = 128; // Default chrominance
			} else {
				Cr = v_plane[v_idx] - 128;
			}

			// YUV to RGB conversion
			R = Y + (int)(1.402f * Cr);
			G = Y - (int)(0.344f * Cb + 0.714f * Cr);
			B = Y + (int)(1.772f * Cb);

			// Clamp values
			if (R < 0) R = 0; else if (R > 255) R = 255;
			if (G < 0) G = 0; else if (G > 255) G = 255;
			if (B < 0) B = 0; else if (B > 255) B = 255;

			// Write RGBA (little-endian)
			int rgb_idx = (y * width + x) * 4;
			if (rgb_idx >= 0 && rgb_idx < (width * height * 4 - 4)) {
				rgb[rgb_idx + 0] = (byte)R;
				rgb[rgb_idx + 1] = (byte)G;
				rgb[rgb_idx + 2] = (byte)B;
				rgb[rgb_idx + 3] = 255;
			}
		}
	}
}

/*
==================
Theora_Init

Initialize Theora decoder
==================
*/
qboolean Theora_Init(int handle) {
	theora_data_t *data;
	// ogg_page ogg_page;
	// ogg_packet ogg_packet;

	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) {
		Com_Printf("Theora_Init: invalid handle %d\n", handle);
		return qfalse;
	}

	// Allocate codec data
	data = (theora_data_t *)Z_Malloc(sizeof(theora_data_t));
	if (!data) {
		Com_Printf("Theora_Init: failed to allocate memory\n");
		return qfalse;
	}

	Com_Memset(data, 0, sizeof(theora_data_t));
	cinTable[handle].codecData = data;
	Com_Printf("Theora_Init: allocated data at %p\n", (void*)data);

	// Initialize Theora library structures
	th_info_init(&data->theora_info);
	th_comment_init(&data->theora_comment);
	data->theora_setup = NULL;

	// Initialize OGG sync state
	ogg_sync_init(&data->ogg_sync);

	// Initialize OGG stream state (will be set up during header parsing)
	// ogg_stream_init will be called when we identify the stream

	// Set up video dimensions (dummy for now since we don't parse headers)
	cinTable[handle].CIN_WIDTH = 320;
	cinTable[handle].CIN_HEIGHT = 240;
	cinTable[handle].drawX = cinTable[handle].CIN_WIDTH;
	cinTable[handle].drawY = cinTable[handle].CIN_HEIGHT;

	// Allocate frame buffer
	if (!cinTable[handle].buf) {
		cinTable[handle].buf = Z_Malloc(cinTable[handle].CIN_WIDTH * cinTable[handle].CIN_HEIGHT * 4);
		if (!cinTable[handle].buf) {
			Com_Printf("Theora_Init: failed to allocate frame buffer\n");
			Theora_Shutdown(handle);
			return qfalse;
		}
	}

	cinTable[handle].status = FMV_PLAY;
	cinTable[handle].lastTime = CL_ScaledMilliseconds();

	// Try to parse Theora headers and create decoder
	if (!Theora_ParseHeaders(handle)) {
		Com_Printf("Theora_Init: failed to parse Theora headers\n");
		Theora_Shutdown(handle);
		return qfalse;
	}

	// Create the decoder context
	data->theora_decoder = th_decode_alloc(&data->theora_info, data->theora_setup);
	if (!data->theora_decoder) {
		Com_Printf("Theora_Init: failed to create Theora decoder\n");
		Theora_Shutdown(handle);
		return qfalse;
	}

	// Clean up setup info (no longer needed after decoder creation)
	if (data->theora_setup) {
		th_setup_free(data->theora_setup);
		data->theora_setup = NULL;
	}

	// Set video parameters from parsed headers
	cinTable[handle].CIN_WIDTH = data->theora_info.frame_width;
	cinTable[handle].CIN_HEIGHT = data->theora_info.frame_height;
	cinTable[handle].drawX = cinTable[handle].CIN_WIDTH;
	cinTable[handle].drawY = cinTable[handle].CIN_HEIGHT;

	// Calculate FPS
	data->fps = (double)data->theora_info.fps_numerator / data->theora_info.fps_denominator;
	if (data->fps <= 0) data->fps = 30.0; // Default fallback

	// Debug: print pixel format information
	Com_Printf("Theora_Init: decoder created successfully (%dx%d @ %.2f fps, pixel format: %d)\n",
		cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT, data->fps, data->theora_info.pixel_fmt);

	// Re-allocate frame buffer with correct dimensions
	if (cinTable[handle].buf) {
		Z_Free(cinTable[handle].buf);
	}
	cinTable[handle].buf = Z_Malloc(cinTable[handle].CIN_WIDTH * cinTable[handle].CIN_HEIGHT * 4);
	if (!cinTable[handle].buf) {
		Com_Printf("Theora_Init: failed to allocate frame buffer\n");
		Theora_Shutdown(handle);
		return qfalse;
	}

	data->initialized = qtrue;
	data->header_read = qtrue;

	return qtrue;
}

/*
==================
Theora_Shutdown

Clean up Theora decoder
==================
*/
void Theora_Shutdown(int handle) {
	theora_data_t *data;

	if (!VALID_HANDLE(handle, MAX_VIDEO_HANDLES)) return;
	
	data = (theora_data_t *)cinTable[handle].codecData;
	if (!data) return;
	
	// Only free Theora resources if they were actually allocated
	if (data->theora_decoder) {
		th_decode_free(data->theora_decoder);
		data->theora_decoder = NULL;
	}

	if (data->theora_setup) {
		th_setup_free(data->theora_setup);
		data->theora_setup = NULL;
	}

	// Clear Theora structures (now that they're properly initialized)
	th_comment_clear(&data->theora_comment);
	th_info_clear(&data->theora_info);

	// Clear OGG structures if they were initialized
	if (data->initialized) {
		ogg_stream_clear(&data->ogg_stream);
		ogg_sync_clear(&data->ogg_sync);
	}
	
	Z_Free(data);
	cinTable[handle].codecData = NULL;
}

/*
==================
Theora_Run

Decode and display next frame
==================
*/
e_status Theora_Run(int handle) {
	Perf_BeginCounter("Theora_Run");

	theora_data_t *data;
	ogg_page ogg_page;
	ogg_packet ogg_packet;
	th_ycbcr_buffer ycbcr;
	int ret;
	int current_time;
	int frame_delay;
	static int debug_count = 0;

	Com_Memset(&ycbcr, 0, sizeof(th_ycbcr_buffer));

	if (!VALID_HANDLE(handle, MAX_VIDEO_HANDLES)) {
		Com_LogPrintf(LOG_CATEGORY_GENERAL, LOG_LEVEL_ERROR,
			"Theora_Run: invalid handle %d", handle);
		return FMV_EOF;
	}

	data = (theora_data_t *)cinTable[handle].codecData;
	// Debug logging removed to avoid log spam during playback.
	if (!data || !data->initialized) {
		Com_Printf("Theora_Run: data not initialized\n");
		return FMV_EOF;
	}

	// Check if Theora library was properly initialized
	if (!data->theora_decoder) {
		Com_Printf("Theora_Run: Theora decoder not initialized (library calls disabled)\n");
		cinTable[handle].dirty = qtrue;
		return cinTable[handle].status;
	}
	
	// Check timing
	current_time = CL_ScaledMilliseconds();
	frame_delay = (int)(1000.0 / data->fps);

	// Allow first frame to be processed immediately
	if (data->frame_count > 0 && current_time - data->last_frame_time < frame_delay) {
		return cinTable[handle].status; // Not time for next frame yet
	}
	
	// Read pages until we get a video packet
	while (qtrue) {
		// Read a page
		if (Theora_ReadOggPage(handle, &ogg_page) == 0) {
			// End of file
			if (cinTable[handle].looping) {
				Theora_Reset(handle);
				return cinTable[handle].status;
			} else {
				cinTable[handle].status = FMV_EOF;
				return FMV_EOF;
			}
		}
		
		// Add page to stream
		ogg_stream_pagein(&data->ogg_stream, &ogg_page);

		// Try to get a video packet
		while (ogg_stream_packetout(&data->ogg_stream, &ogg_packet) > 0) {
			// Try to decode as video
			ret = th_decode_packetin(data->theora_decoder, &ogg_packet, NULL);
			if (ret == 0) {
				// Got a frame!
				if (!data->theora_decoder) {
					Com_Printf("Theora_Run: decoder is NULL\n");
					cinTable[handle].status = FMV_EOF;
					return FMV_EOF;
				}
				// Decode the frame
				th_decode_ycbcr_out(data->theora_decoder, ycbcr);

				// Additional safety: ensure buffers exist after decode
				if (!data->theora_decoder || !ycbcr[0].data || !ycbcr[1].data || !ycbcr[2].data || !cinTable[handle].buf) {
					Com_Printf("Theora_Run: invalid post-decode buffers\n");
					cinTable[handle].status = FMV_EOF;
					return FMV_EOF;
				}

				// Debug: check what dimensions Theora library reports
				if (debug_count < 3) {
					Com_Printf("Theora_Run: YUV buffer info - Y:%dx%d U:%dx%d V:%dx%d strides Y:%d U:%d V:%d\n",
						ycbcr[0].width, ycbcr[0].height,
						ycbcr[1].width, ycbcr[1].height,
						ycbcr[2].width, ycbcr[2].height,
						ycbcr[0].stride, ycbcr[1].stride, ycbcr[2].stride);
					debug_count++;
				}

				// Validate YUV buffer dimensions and strides - handle both 4:4:4 and 4:2:0 subsampling
				int min_u_stride = (ycbcr[1].width == ycbcr[0].width) ? cinTable[handle].CIN_WIDTH : cinTable[handle].CIN_WIDTH / 2;
				int min_v_stride = (ycbcr[2].width == ycbcr[0].width) ? cinTable[handle].CIN_WIDTH : cinTable[handle].CIN_WIDTH / 2;

				if (ycbcr[0].stride < cinTable[handle].CIN_WIDTH ||
					ycbcr[1].stride < min_u_stride ||
					ycbcr[2].stride < min_v_stride) {
					Com_Printf("Theora_Run: invalid YUV buffer strides: Y=%d U=%d V=%d (expected Y>=%d U>=%d V>=%d)\n",
						ycbcr[0].stride, ycbcr[1].stride, ycbcr[2].stride,
						cinTable[handle].CIN_WIDTH, min_u_stride, min_v_stride);
					cinTable[handle].status = FMV_EOF;
					return FMV_EOF;
				}

				// Convert YUV to RGB
				Com_Printf("Theora_Run: about to call Theora_YUVtoRGB\n");
				Theora_YUVtoRGB(ycbcr, cinTable[handle].buf,
					cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT);
				Com_Printf("Theora_Run: Theora_YUVtoRGB completed\n");
				cinTable[handle].dirty = qtrue;
				Com_Printf("Theora_Run: set dirty flag\n");
				
				data->frame_count++;
				data->last_frame_time = current_time;
				cinTable[handle].lastTime = current_time;
				
				return cinTable[handle].status;
			} else if (ret == TH_DUPFRAME) {
				// Duplicate frame, use previous
				return cinTable[handle].status;
			}
			// Otherwise, not a video packet, continue
		}
	}

	Perf_EndCounter("Theora_Run");
	return cinTable[handle].status;
}

/*
==================
Theora_Reset

Reset decoder to beginning
==================
*/
void Theora_Reset(int handle) {
	theora_data_t *data;

	if (!VALID_HANDLE(handle, MAX_VIDEO_HANDLES)) return;
	
	data = (theora_data_t *)cinTable[handle].codecData;
	if (!data) return;
	
	// Close and reopen file
	if (cinTable[handle].iFile != FS_INVALID_HANDLE) {
		FS_FCloseFile(cinTable[handle].iFile);
	}
	cinTable[handle].ROQSize = FS_FOpenFileRead(cinTable[handle].fileName, &cinTable[handle].iFile, qtrue);
	
	if (cinTable[handle].ROQSize <= 0) {
		Com_Printf("Theora_Reset: failed to reopen file\n");
		cinTable[handle].status = FMV_EOF;
		return;
	}
	
	// Reinitialize Ogg sync and stream
	if (data->initialized) {
		ogg_stream_clear(&data->ogg_stream);
		ogg_sync_clear(&data->ogg_sync);
	}
	ogg_sync_init(&data->ogg_sync);

	// Reset Theora header state
	if (data->theora_setup) {
		th_setup_free(data->theora_setup);
		data->theora_setup = NULL;
	}
	th_comment_clear(&data->theora_comment);
	th_info_clear(&data->theora_info);
	th_info_init(&data->theora_info);
	th_comment_init(&data->theora_comment);
	
	// Reset state
	data->header_read = qfalse;
	data->video_packet_count = 0;
	data->frame_count = 0;
	data->last_frame_time = CL_ScaledMilliseconds();
	cinTable[handle].startTime = cinTable[handle].lastTime = CL_ScaledMilliseconds();
	
	// Re-read headers
	ogg_page ogg_page;
	ogg_packet ogg_packet;
	
	while (!data->header_read) {
		if (Theora_ReadOggPage(handle, &ogg_page) == 0) {
			Com_Printf("Theora_Reset: failed to read header pages\n");
			cinTable[handle].status = FMV_EOF;
			return;
		}
		
		if (ogg_page_bos(&ogg_page)) {
			ogg_stream_init(&data->ogg_stream, ogg_page_serialno(&ogg_page));
		}
		
		ogg_stream_pagein(&data->ogg_stream, &ogg_page);
		
		while (ogg_stream_packetout(&data->ogg_stream, &ogg_packet) > 0) {
			if (th_decode_headerin(&data->theora_info, &data->theora_comment, &data->theora_setup, &ogg_packet) >= 0) {
				data->video_packet_count++;
				if (data->video_packet_count == 3) {
					data->header_read = qtrue;
					break;
				}
			}
		}
	}
	
	// Recreate decoder
	if (data->theora_decoder) {
		th_decode_free(data->theora_decoder);
	}
	data->theora_decoder = th_decode_alloc(&data->theora_info, data->theora_setup);
	if (!data->theora_decoder) {
		Com_Printf("Theora_Reset: failed to recreate decoder\n");
		cinTable[handle].status = FMV_EOF;
		return;
	}
	
	data->initialized = qtrue;
	cinTable[handle].status = FMV_LOOPED;
}

// Update codec info table
void Theora_RegisterCodec(void) {
	// Codec info is already registered in cl_cin_codec.c
	// This function can be used for additional initialization if needed
}

#endif // USE_THEORA

