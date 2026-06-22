/*
===============================================================================
Opus codec wrapper for idTech3 sound system
===============================================================================
*/

#include "../../client/client.h"
#include "snd_codec.h"
#include "../snd_local.h"

#ifdef USE_OPUS
#include <opusfile.h>

typedef struct {
	OggOpusFile *opus;
} opus_stream_ctx_t;

static int S_OPUS_Callback_read( void *datasource, unsigned char *ptr, int nbytes )
{
	snd_stream_t *stream;
	int bytesRead;

	if ( !ptr || !datasource || nbytes <= 0 ) {
		return -1;
	}

	stream = (snd_stream_t *)datasource;
	bytesRead = FS_Read( ptr, nbytes, stream->file );
	if ( bytesRead < 0 ) {
		return -1;
	}
	stream->pos += bytesRead;
	return bytesRead;
}

static int S_OPUS_Callback_seek( void *datasource, opus_int64 offset, int whence )
{
	snd_stream_t *stream;
	int origin;

	if ( !datasource ) {
		return -1;
	}

	stream = (snd_stream_t *)datasource;

	switch ( whence ) {
		case SEEK_SET: origin = FS_SEEK_SET; break;
		case SEEK_CUR: origin = FS_SEEK_CUR; break;
		case SEEK_END: origin = FS_SEEK_END; break;
		default: return -1;
	}

	if ( FS_Seek( stream->file, (long)offset, origin ) != 0 ) {
		return -1;
	}

	stream->pos = FS_FTell( stream->file );
	return 0;
}

static opus_int64 S_OPUS_Callback_tell( void *datasource )
{
	snd_stream_t *stream;

	if ( !datasource ) {
		return -1;
	}

	stream = (snd_stream_t *)datasource;
	return stream->pos;
}

static int S_OPUS_Callback_close( void *datasource )
{
	(void)datasource;
	return 0;
}

static const OpusFileCallbacks opus_callbacks = {
	S_OPUS_Callback_read,
	S_OPUS_Callback_seek,
	S_OPUS_Callback_tell,
	S_OPUS_Callback_close
};

static qboolean S_OPUS_FillInfo( OggOpusFile *opus, snd_info_t *info )
{
	const OpusHead *head = op_head( opus, -1 );
	opus_int64 total = op_pcm_total( opus, -1 );

	if ( !head || !info ) {
		return qfalse;
	}

	info->channels = head->channel_count;
	info->rate = 48000;
	info->width = 2;
	info->samples = (int)total;
	info->size = info->samples * info->channels * info->width;
	info->dataofs = 0;

	return qtrue;
}

void *S_OPUS_CodecLoad( const char *filename, snd_info_t *info )
{
	int error;
	opus_int64 total;
	int samplesLeft;
	short *out;
	int channels;
	int outSamples;
	snd_stream_t *stream;
	opus_stream_ctx_t *ctx;
	int offset = 0;

	stream = S_CodecUtilOpen( filename, &opus_codec );
	if ( !stream ) {
		return NULL;
	}

	ctx = (opus_stream_ctx_t *)Z_Malloc( sizeof( *ctx ) );
	ctx->opus = op_open_callbacks( stream, &opus_callbacks, NULL, 0, &error );
	if ( !ctx->opus ) {
		Z_Free( ctx );
		S_CodecUtilClose( &stream );
		return NULL;
	}

	if ( !S_OPUS_FillInfo( ctx->opus, info ) ) {
		op_free( ctx->opus );
		Z_Free( ctx );
		S_CodecUtilClose( &stream );
		return NULL;
	}

	channels = info->channels;
	total = op_pcm_total( ctx->opus, -1 );
	samplesLeft = (int)total;
	out = (short *)Z_Malloc( samplesLeft * channels * sizeof( short ) );

	while ( samplesLeft > 0 ) {
		int read = op_read( ctx->opus, out + offset * channels, samplesLeft, NULL );
		if ( read <= 0 ) {
			break;
		}
		offset += read;
		samplesLeft -= read;
	}

	outSamples = offset;
	info->samples = outSamples;
	info->size = outSamples * channels * info->width;

	op_free( ctx->opus );
	Z_Free( ctx );
	S_CodecUtilClose( &stream );

	return out;
}

snd_stream_t *S_OPUS_CodecOpenStream( const char *filename )
{
	int error;
	snd_stream_t *stream;
	opus_stream_ctx_t *ctx;

	stream = S_CodecUtilOpen( filename, &opus_codec );
	if ( !stream ) {
		return NULL;
	}

	ctx = (opus_stream_ctx_t *)Z_Malloc( sizeof( *ctx ) );
	ctx->opus = op_open_callbacks( stream, &opus_callbacks, NULL, 0, &error );
	if ( !ctx->opus ) {
		Z_Free( ctx );
		S_CodecUtilClose( &stream );
		return NULL;
	}

	if ( !S_OPUS_FillInfo( ctx->opus, &stream->info ) ) {
		op_free( ctx->opus );
		Z_Free( ctx );
		S_CodecUtilClose( &stream );
		return NULL;
	}

	stream->ptr = ctx;
	return stream;
}

int S_OPUS_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer )
{
	opus_stream_ctx_t *ctx;
	int samplesRequested;
	int samplesRead;
	int channels;

	if ( !stream || !stream->ptr || !buffer || bytes <= 0 ) {
		return 0;
	}

	ctx = (opus_stream_ctx_t *)stream->ptr;
	channels = stream->info.channels;
	samplesRequested = bytes / ( channels * sizeof( short ) );

	samplesRead = op_read( ctx->opus, (short *)buffer, samplesRequested, NULL );
	if ( samplesRead <= 0 ) {
		return 0;
	}

	return samplesRead * channels * sizeof( short );
}

void S_OPUS_CodecCloseStream( snd_stream_t *stream )
{
	opus_stream_ctx_t *ctx;

	if ( !stream ) {
		return;
	}

	ctx = (opus_stream_ctx_t *)stream->ptr;
	if ( ctx ) {
		if ( ctx->opus ) {
			op_free( ctx->opus );
		}
		Z_Free( ctx );
		stream->ptr = NULL;
	}

	S_CodecUtilClose( &stream );
}

snd_codec_t opus_codec =
{
	"opus",
	S_OPUS_CodecLoad,
	S_OPUS_CodecOpenStream,
	S_OPUS_CodecReadStream,
	S_OPUS_CodecCloseStream,
	NULL
};
#endif
