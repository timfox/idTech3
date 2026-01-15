#ifdef USE_THEORA

#include "client.h"
#include "cl_cin_codec.h"
#include <theora/theora.h>
// region agent log
static void agent_log(const char* hypothesisId, const char* location, const char* message) {
  FILE* f = fopen("/home/tim/Desktop/idtech3/.cursor/debug.log", "a");
  if (!f) return;
  long long ts = (long long)time(NULL) * 1000;
  fprintf(f, "{\"sessionId\":\"debug-session\",\"runId\":\"runtime-test\",\"hypothesisId\":\"%s\",\"location\":\"%s\",\"message\":\"%s\",\"timestamp\":%lld}\n", hypothesisId, location, message, ts);
  fclose(f);
}
// endregion
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

// Convert YUV to RGB
static void Theora_YUVtoRGB(th_ycbcr_buffer ycbcr, byte *rgb, int width, int height) {
	int x, y;
	int Y, Cb, Cr;
	int R, G, B;
	byte *y_plane = ycbcr[0].data;
	byte *u_plane = ycbcr[1].data;
	byte *v_plane = ycbcr[2].data;
	int y_stride = ycbcr[0].stride;
	int u_stride = ycbcr[1].stride;
	int v_stride = ycbcr[2].stride;
	
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
Theora_Init

Initialize Theora decoder
==================
*/
qboolean Theora_Init(int handle) {
	theora_data_t *data;
	ogg_page ogg_page;
	ogg_packet ogg_packet;

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
	
	// Initialize Ogg sync
	ogg_sync_init(&data->ogg_sync);
	
	// Read initial pages to find Theora stream
	data->header_read = qfalse;
	int pages_read = 0;
	const int MAX_HEADER_PAGES = 10; // Limit header reading attempts
	
	while (!data->header_read && pages_read < MAX_HEADER_PAGES) {
		if (Theora_ReadOggPage(handle, &ogg_page) == 0) {
			if (pages_read == 0) {
				Com_Printf("Theora_Init: failed to read Ogg page\n");
			} else {
				Com_Printf("Theora_Init: no Theora stream found in Ogg file\n");
			}
			Theora_Shutdown(handle);
			return qfalse;
		}
		pages_read++;
		
		// Initialize stream if this is the first page
		if (ogg_page_bos(&ogg_page)) {
			ogg_stream_init(&data->ogg_stream, ogg_page_serialno(&ogg_page));
		}
		
		// Add page to stream
		ogg_stream_pagein(&data->ogg_stream, &ogg_page);

		// Try to get Theora header packets
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
	
	// Create decoder
	Com_Printf("Theora_Init: creating decoder with info pic_width=%d pic_height=%d\n",
		data->theora_info.frame_width, data->theora_info.frame_height);
	data->theora_decoder = th_decode_alloc(&data->theora_info, data->theora_setup);
	if (!data->theora_decoder) {
		Com_Printf("Theora_Init: failed to create decoder\n");
		Theora_Shutdown(handle);
		return qfalse;
	}
	Com_Printf("Theora_Init: decoder allocated at %p\n", (void*)data->theora_decoder);

	// Set up video dimensions
	cinTable[handle].CIN_WIDTH = data->theora_info.frame_width;
	cinTable[handle].CIN_HEIGHT = data->theora_info.frame_height;
	cinTable[handle].drawX = cinTable[handle].CIN_WIDTH;
	cinTable[handle].drawY = cinTable[handle].CIN_HEIGHT;
	
	// Calculate FPS
	if (data->theora_info.fps_denominator > 0) {
		data->fps = (double)data->theora_info.fps_numerator / (double)data->theora_info.fps_denominator;
	} else {
		data->fps = 30.0; // Default
	}
	
	// Initialize buffer
	cinTable[handle].samplesPerPixel = 4; // RGBA
	cinTable[handle].samplesPerLine = cinTable[handle].CIN_WIDTH * 4;
	cinTable[handle].screenDelta = cinTable[handle].CIN_HEIGHT * cinTable[handle].samplesPerLine;
	
	// Allocate frame buffer if needed
	if (!cinTable[handle].buf) {
				cinTable[handle].buf = Z_Malloc(cinTable[handle].CIN_WIDTH * cinTable[handle].CIN_HEIGHT * 4);
		if (!cinTable[handle].buf) {
			Com_Printf("Theora_Init: failed to allocate frame buffer\n");
			Theora_Shutdown(handle);
			return qfalse;
		}
	}
	
	data->initialized = qtrue;
	data->frame_count = 0;
	data->last_frame_time = CL_ScaledMilliseconds();
	
	cinTable[handle].startTime = cinTable[handle].lastTime = CL_ScaledMilliseconds();
	
	Com_DPrintf("Theora_Init: initialized %dx%d @ %.2f fps\n", 
		cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT, data->fps);
	
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
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return;
	
	data = (theora_data_t *)cinTable[handle].codecData;
	if (!data) return;
	
	if (data->theora_decoder) {
		th_decode_free(data->theora_decoder);
		data->theora_decoder = NULL;
	}
	
	if (data->theora_setup) {
		th_setup_free(data->theora_setup);
		data->theora_setup = NULL;
	}
	
	th_comment_clear(&data->theora_comment);
	th_info_clear(&data->theora_info);
	
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
	theora_data_t *data;
	ogg_page ogg_page;
	ogg_packet ogg_packet;
	th_ycbcr_buffer ycbcr;
	int ret;
	int current_time;
	int frame_delay;

	Com_Memset(&ycbcr, 0, sizeof(th_ycbcr_buffer));

	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) {
		Com_Printf("Theora_Run: invalid handle %d\n", handle);
		return FMV_EOF;
	}

	data = (theora_data_t *)cinTable[handle].codecData;
	// Debug logging removed to avoid log spam during playback.
	if (!data || !data->initialized) {
		Com_Printf("Theora_Run: data not initialized\n");
		return FMV_EOF;
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
	th_decode_ycbcr_out(data->theora_decoder, ycbcr);
    // Additional safety: ensure buffers exist after decode
    if (!data->theora_decoder || !ycbcr[0].data || !ycbcr[1].data || !ycbcr[2].data || !cinTable[handle].buf) {
        Com_Printf("Theora_Run: invalid post-decode buffers\n");
        cinTable[handle].status = FMV_EOF;
        return FMV_EOF;
    }
    // Validate YUV buffer
    if (!ycbcr[0].data || !ycbcr[1].data || !ycbcr[2].data) {
					Com_Printf("Theora_Run: invalid YUV buffer from decoder\n");
					cinTable[handle].status = FMV_EOF;
					return FMV_EOF;
				}

                // Convert YUV to RGB (guard against invalid/NULL YCbCr buffers)
                if (cinTable[handle].buf && ycbcr[0].data && ycbcr[1].data && ycbcr[2].data) {
                Theora_YUVtoRGB(ycbcr, cinTable[handle].buf, 
                    cinTable[handle].CIN_WIDTH, cinTable[handle].CIN_HEIGHT);
                cinTable[handle].dirty = qtrue;
                }
				
				data->frame_count++;
				data->last_frame_time = current_time;
				
				return cinTable[handle].status;
			} else if (ret == TH_DUPFRAME) {
				// Duplicate frame, use previous
				return cinTable[handle].status;
			}
			// Otherwise, not a video packet, continue
		}
	}
	
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
	
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES) return;
	
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

