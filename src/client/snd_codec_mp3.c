// includes for the sound system
#include "client.h"
#include "snd_codec.h"

// includes for the MP3 codec
#include "mp3/mhead.h"
#include "mp3/mp3struct.h"

// MP3 decoder function declarations (from mp3)
#ifdef __cplusplus
extern "C" {
#endif

char* C_MP3_IsValid(void *pvData, int iDataLen, int bStereoDesired);
char* C_MP3_GetUnpackedSize(void *pvData, int iDataLen, int *piUnpackedSize, int bStereoDesired);
char* C_MP3_UnpackRawPCM(void *pvData, int iDataLen, int *piUnpackedSize, void *pbUnpackBuffer, int bStereoDesired);
char* C_MP3_GetHeaderData(void *pvData, int iDataLen, int *piRate, int *piWidth, int *piChannels, int bStereoDesired);
char* C_MP3Stream_DecodeInit(LP_MP3STREAM pSFX_MP3Stream, void *pvSourceData, int iSourceBytesRemaining,
							int iGameAudioSampleRate, int iGameAudioSampleBits, int bStereoDesired);
unsigned int C_MP3Stream_Decode(LP_MP3STREAM pSFX_MP3Stream);
char* C_MP3Stream_Rewind(LP_MP3STREAM pSFX_MP3Stream);

#ifdef __cplusplus
}
#endif

// Forward declarations for MP3 codec functions
void *S_MP3_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_MP3_CodecOpenStream(const char *filename);
int S_MP3_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);
void S_MP3_CodecCloseStream(snd_stream_t *stream);

// Q3 MP3 codec
snd_codec_t mp3_codec =
{
	"mp3",
	S_MP3_CodecLoad,
	S_MP3_CodecOpenStream,
	S_MP3_CodecReadStream,
	S_MP3_CodecCloseStream,
	NULL
};

// structure used for info purposes
struct snd_codec_mp3_info
{
	LP_MP3STREAM mp3stream;		// MP3 stream structure
	byte *buffer;			// input buffer
	int buffer_size;		// size of input buffer
	int buffer_pos;			// current position in buffer
	qboolean initialized;		// whether the stream is initialized
};

/*************** MP3 codec functions ***************/

/*
=================
S_MP3_CodecLoad
=================
*/
void *S_MP3_CodecLoad(const char *filename, snd_info_t *info)
{
	fileHandle_t hFile;
	int iFileLen;
	byte *pFileData;
	char *psError;

	// Load the entire file into memory
	iFileLen = FS_FOpenFileRead(filename, &hFile, qtrue);
	if (hFile == FS_INVALID_HANDLE || iFileLen <= 0)
	{
		Com_Printf("S_MP3_CodecLoad: couldn't open %s\n", filename);
		return NULL;
	}

	pFileData = Z_Malloc(iFileLen);
	if (!pFileData)
	{
		FS_FCloseFile(hFile);
		return NULL;
	}

	FS_Read(pFileData, iFileLen, hFile);
	FS_FCloseFile(hFile);

	// Try to get the unpacked size and format info
	int iRate, iWidth, iChannels;
	psError = C_MP3_GetHeaderData(pFileData, iFileLen, &iRate, &iWidth, &iChannels, qfalse);
	if (psError)
	{
		Com_Printf("S_MP3_CodecLoad: %s (%s)\n", psError, filename);
		Z_Free(pFileData);
		return NULL;
	}

	// Get unpacked size
	int iUnpackedSize;
	psError = C_MP3_GetUnpackedSize(pFileData, iFileLen, &iUnpackedSize, qfalse);
	if (psError)
	{
		Com_Printf("S_MP3_CodecLoad: %s (%s)\n", psError, filename);
		Z_Free(pFileData);
		return NULL;
	}

	// Allocate buffer for unpacked PCM data
	byte *pUnpackedData = Z_Malloc(iUnpackedSize);
	if (!pUnpackedData)
	{
		Z_Free(pFileData);
		return NULL;
	}

	// Unpack the MP3 data
	psError = C_MP3_UnpackRawPCM(pFileData, iFileLen, &iUnpackedSize, pUnpackedData, qfalse);
	if (psError)
	{
		Com_Printf("S_MP3_CodecLoad: %s (%s)\n", psError, filename);
		Z_Free(pUnpackedData);
		Z_Free(pFileData);
		return NULL;
	}

	// Fill in info structure
	info->rate = iRate;
	info->width = iWidth;
	info->channels = iChannels;
	info->samples = iUnpackedSize / (iWidth * iChannels);
	info->size = iUnpackedSize;
	info->dataofs = 0;

	Z_Free(pFileData);
	return pUnpackedData;
}

/*
=================
S_MP3_CodecOpenStream
=================
*/
snd_stream_t *S_MP3_CodecOpenStream(const char *filename)
{
	fileHandle_t hFile;
	int iFileLen;
	byte *pFileData;
	struct snd_codec_mp3_info *mp3info;
	char *psError;

	// Load the entire file into memory
	iFileLen = FS_FOpenFileRead(filename, &hFile, qtrue);
	if (hFile == FS_INVALID_HANDLE || iFileLen <= 0)
	{
		return NULL;
	}

	pFileData = Z_Malloc(iFileLen);
	if (!pFileData)
	{
		FS_FCloseFile(hFile);
		return NULL;
	}

	FS_Read(pFileData, iFileLen, hFile);
	FS_FCloseFile(hFile);

	// Initialize MP3 stream
	mp3info = Z_Malloc(sizeof(*mp3info));
	if (!mp3info)
	{
		Z_Free(pFileData);
		return NULL;
	}

	Com_Memset(mp3info, 0, sizeof(*mp3info));

	psError = C_MP3Stream_DecodeInit(mp3info->mp3stream, pFileData, iFileLen, 22050, 16, qfalse);
	if (psError)
	{
		Com_Printf("S_MP3_CodecOpenStream: %s (%s)\n", psError, filename);
		Z_Free(mp3info);
		Z_Free(pFileData);
		return NULL;
	}

	mp3info->buffer = pFileData;
	mp3info->buffer_size = iFileLen;
	mp3info->buffer_pos = 0;
	mp3info->initialized = qtrue;

	// Create a minimal stream structure
	snd_stream_t *stream = Z_Malloc(sizeof(snd_stream_t));
	if (!stream)
	{
		if (mp3info->mp3stream)
			Z_Free(mp3info->mp3stream);
		Z_Free(mp3info);
		Z_Free(pFileData);
		return NULL;
	}

	stream->codec = &mp3_codec;
	stream->file = FS_INVALID_HANDLE; // We're using memory buffer
	stream->length = iFileLen;
	stream->ptr = mp3info;

	return stream;
}

/*
=================
S_MP3_CodecCloseStream
=================
*/
void S_MP3_CodecCloseStream(snd_stream_t *stream)
{
	struct snd_codec_mp3_info *mp3info = (struct snd_codec_mp3_info *)stream->ptr;

	if (mp3info)
	{
		if (mp3info->buffer)
			Z_Free(mp3info->buffer);
		if (mp3info->mp3stream)
			Z_Free(mp3info->mp3stream);
		Z_Free(mp3info);
	}

	Z_Free(stream);
}

/*
=================
S_MP3_CodecReadStream
=================
*/
int S_MP3_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer)
{
	struct snd_codec_mp3_info *mp3info = (struct snd_codec_mp3_info *)stream->ptr;

	if (!mp3info || !mp3info->initialized)
		return 0;

	// Decode MP3 data
	unsigned int decodedBytes = C_MP3Stream_Decode(mp3info->mp3stream);

	if (decodedBytes == 0)
		return 0; // End of stream or error

	// Copy decoded data to output buffer
	int copyBytes = bytes;
	if ((int)decodedBytes < copyBytes)
		copyBytes = (int)decodedBytes;

	// The decoded data is in mp3info->mp3stream->bDecodeBuffer
	Com_Memcpy(buffer, mp3info->mp3stream->bDecodeBuffer + mp3info->mp3stream->iCopyOffset, copyBytes);

	return copyBytes;
}