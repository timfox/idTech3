/*
===========================================================================
  MP3 codec wrapper for idTech3 sound system
===========================================================================
*/

#include "../../client/client.h"
#include "snd_codec.h"
#include "mp3/mp3struct.h"
#include "../snd_local.h"

typedef struct {
	byte *data;
	int length;
	MP3STREAM mp3;
} mp3_stream_ctx_t;

static int MP3_AbsInt( int value )
{
	return value < 0 ? -value : value;
}

// forward declarations
void *S_MP3_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_MP3_CodecOpenStream(const char *filename);
int S_MP3_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);
void S_MP3_CodecCloseStream(snd_stream_t *stream);

snd_codec_t mp3_codec =
{
	"mp3",
	S_MP3_CodecLoad,
	S_MP3_CodecOpenStream,
	S_MP3_CodecReadStream,
	S_MP3_CodecCloseStream,
	NULL
};

/*
=================
S_MP3_CodecLoad

Load entire MP3 file, decode to raw PCM and return buffer (caller frees)
=================
*/
void *S_MP3_CodecLoad(const char *filename, snd_info_t *info)
{
	fileHandle_t f;
	int length;
	byte *filebuf = NULL;
	char *err;
	int unpackSize = 0;
	byte *outbuf = NULL;
	int stereoDesired = (dma.channels == 2) ? 1 : 0;
	int maxSoundBytes;

	if (!filename || !info) return NULL;

	length = FS_FOpenFileRead(filename, &f, qtrue);
	if ( f == FS_INVALID_HANDLE ) {
		return NULL;
	}

	// read file into temp buffer
	filebuf = Z_Malloc(length);
	if (!filebuf) {
		FS_FCloseFile(f);
		return NULL;
	}
	FS_Read(filebuf, length, f);
	FS_FCloseFile(f);

	// get header info
	{
		int rate=0, width=0, channels=0;
		err = C_MP3_GetHeaderData(filebuf, length, &rate, &width, &channels, stereoDesired);
		if (err) {
			Z_Free(filebuf);
			return NULL;
		}
		info->rate = rate;
		info->width = width;
		info->channels = channels;
	}

	// estimate unpacked size
	err = C_MP3_GetUnpackedSize(filebuf, length, &unpackSize, stereoDesired);
	if (err || unpackSize <= 0) {
		// fallback: free and fail
		Z_Free(filebuf);
		return NULL;
	}
	maxSoundBytes = Cvar_VariableIntegerValue("com_soundMegs") * 1024 * 1024;
	if (maxSoundBytes <= 0) {
		maxSoundBytes = 8 * 1024 * 1024;
	}
	if (unpackSize > maxSoundBytes) {
		Com_Printf(S_COLOR_YELLOW "WARNING: MP3 %s is too large for SFX (%d bytes). Use music instead.\n",
			filename, unpackSize);
		Z_Free(filebuf);
		return NULL;
	}

	outbuf = Hunk_AllocateTempMemory(unpackSize);
	if (!outbuf) {
		Z_Free(filebuf);
		return NULL;
	}

	err = C_MP3_UnpackRawPCM(filebuf, length, &unpackSize, outbuf, stereoDesired);
	if (err) {
		Z_Free(filebuf);
		Hunk_FreeTempMemory(outbuf);
		return NULL;
	}

	info->size = unpackSize;
	info->samples = unpackSize / (info->width * info->channels);
	info->dataofs = 0;

	// free file buffer
	Z_Free(filebuf);

	return outbuf;
}

/*
=================
S_MP3_CodecOpenStream

Open MP3 file and prepare decoder for streaming
=================
*/
snd_stream_t *S_MP3_CodecOpenStream(const char *filename)
{
	snd_stream_t *stream;
	int length;
	byte *filebuf = NULL;
	mp3_stream_ctx_t *ctx = NULL;
	char *err;
	int stereoDesired = (dma.channels == 2) ? 1 : 0;
	int sourceRate = 0;
	int sourceWidth = 0;
	int sourceChannels = 0;
	int supportedRates[3];
	int tryRates[4];
	int supportedCount = 0;
	int tryCount = 0;
	int i, j;
	int decodeRate = 0;

	if (!filename) return NULL;

	stream = S_CodecUtilOpen(filename, &mp3_codec);
	if (!stream) return NULL;

	// read entire compressed file into temp memory for the MP3 decoder
	length = stream->length;
	filebuf = Z_Malloc(length);
	if (!filebuf) {
		S_CodecUtilClose(&stream);
		return NULL;
	}
	// seek to start and read
	FS_Seek(stream->file, 0, FS_SEEK_SET);
	FS_Read(filebuf, length, stream->file);

	// allocate context
	ctx = Z_Malloc(sizeof(*ctx));
	if (!ctx) {
		Z_Free(filebuf);
		S_CodecUtilClose(&stream);
		return NULL;
	}
	ctx->data = filebuf;
	ctx->length = length;

	// get source stream properties so we can choose a supported decode rate
	err = C_MP3_GetHeaderData( ctx->data, ctx->length, &sourceRate, &sourceWidth, &sourceChannels, stereoDesired );
	if ( err || sourceRate <= 0 ) {
		Z_Free(ctx);
		Z_Free(filebuf);
		S_CodecUtilClose(&stream);
		return NULL;
	}

	// this decoder supports source, source/2, source/4 output rates only
	supportedRates[supportedCount++] = sourceRate;
	if ( sourceRate / 2 > 0 ) {
		supportedRates[supportedCount++] = sourceRate / 2;
	}
	if ( sourceRate / 4 > 0 ) {
		supportedRates[supportedCount++] = sourceRate / 4;
	}

	// sort supported rates by proximity to current mix rate
	for ( i = 0; i < supportedCount; ++i ) {
		for ( j = i + 1; j < supportedCount; ++j ) {
			if ( MP3_AbsInt( supportedRates[j] - dma.speed ) < MP3_AbsInt( supportedRates[i] - dma.speed ) ) {
				int tmp = supportedRates[i];
				supportedRates[i] = supportedRates[j];
				supportedRates[j] = tmp;
			}
		}
	}

	// first try the exact engine rate, then closest supported fallbacks
	tryRates[tryCount++] = dma.speed;
	for ( i = 0; i < supportedCount; ++i ) {
		if ( supportedRates[i] != dma.speed ) {
			tryRates[tryCount++] = supportedRates[i];
		}
	}

	err = NULL;
	for ( i = 0; i < tryCount; ++i ) {
		decodeRate = tryRates[i];
		err = C_MP3Stream_DecodeInit( &ctx->mp3, ctx->data, ctx->length, decodeRate, dma.samplebits, stereoDesired );
		if ( !err ) {
			break;
		}
	}

	if ( err ) {
		Com_DPrintf( S_COLOR_YELLOW "MP3 stream init failed for %s: %s\n", filename, err );
		Z_Free(ctx);
		Z_Free(filebuf);
		S_CodecUtilClose(&stream);
		return NULL;
	}
	if ( decodeRate != dma.speed ) {
		Com_DPrintf( "MP3 stream %s using %d Hz decode for %d Hz mix\n", filename, decodeRate, dma.speed );
	}

	// fill stream info to indicate decoded output format
	stream->info.rate = decodeRate;
	stream->info.width = dma.samplebits / 8;
	stream->info.channels = dma.channels;
	stream->info.samples = 0;
	stream->info.size = 0;
	stream->info.dataofs = 0;

	stream->ptr = ctx;
	// we will use stream->pos for compressed stream offset if needed
	stream->pos = 0;

	return stream;
}

/*
=================
S_MP3_CodecReadStream

Decode mp3 into provided buffer up to 'bytes' and return number of bytes written
=================
*/
int S_MP3_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer)
{
	mp3_stream_ctx_t *ctx;
	int bytesRead = 0;
	char *bufPtr;
	int bytesAvailable;
	unsigned int out;

	if (!stream || !buffer || bytes <= 0) return 0;
	ctx = (mp3_stream_ctx_t *) stream->ptr;
	if (!ctx) return 0;

	bufPtr = buffer;

	while (bytesRead < bytes) {
		bytesAvailable = ctx->mp3.iBytesDecodedThisPacket - ctx->mp3.iCopyOffset;
		if (bytesAvailable <= 0) {
			out = C_MP3Stream_Decode(&ctx->mp3);
			if (out == 0) {
				break;
			}
			ctx->mp3.iCopyOffset = 0;
			bytesAvailable = ctx->mp3.iBytesDecodedThisPacket;
		}
		// copy decoded data
		{
			int copy = bytesAvailable;
			if (copy + bytesRead > bytes) copy = bytes - bytesRead;
			memcpy(bufPtr + bytesRead, ctx->mp3.bDecodeBuffer + ctx->mp3.iCopyOffset, copy);
			ctx->mp3.iCopyOffset += copy;
			bytesRead += copy;
		}
	}

	return bytesRead;
}

/*
=================
S_MP3_CodecCloseStream
=================
*/
void S_MP3_CodecCloseStream(snd_stream_t *stream)
{
	mp3_stream_ctx_t *ctx;
	if (!stream) return;
	ctx = (mp3_stream_ctx_t *) stream->ptr;
	if (ctx) {
		if (ctx->data) {
			Z_Free(ctx->data);
		}
		Z_Free(ctx);
		stream->ptr = NULL;
	}
	S_CodecUtilClose(&stream);
}
