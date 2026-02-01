/*
===============================================================================
FLAC codec wrapper for idTech3 sound system
===============================================================================
*/

#include "../../client/client.h"
#include "snd_codec.h"
#include "../snd_local.h"

#ifdef USE_FLAC
#include <FLAC/stream_decoder.h>

typedef struct {
	const byte *data;
	int length;
	int pos;
} flac_mem_reader_t;

typedef struct {
	short *pcm;
	int samples;
	int channels;
	int rate;
	int bitsPerSample;
} flac_decode_output_t;

static FLAC__StreamDecoderReadStatus S_FLAC_Read( const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data )
{
	flac_mem_reader_t *reader = (flac_mem_reader_t *)client_data;
	size_t remain;
	size_t toCopy;
	(void)decoder;

	if ( !reader || !bytes ) {
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	if ( *bytes == 0 ) {
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	if ( reader->pos >= reader->length ) {
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}

	remain = (size_t)( reader->length - reader->pos );
	toCopy = *bytes < remain ? *bytes : remain;
	Com_Memcpy( buffer, reader->data + reader->pos, toCopy );
	reader->pos += (int)toCopy;
	*bytes = toCopy;

	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus S_FLAC_Seek( const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data )
{
	flac_mem_reader_t *reader = (flac_mem_reader_t *)client_data;
	(void)decoder;
	if ( !reader || absolute_byte_offset > (FLAC__uint64)reader->length ) {
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	}
	reader->pos = (int)absolute_byte_offset;
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus S_FLAC_Tell( const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data )
{
	flac_mem_reader_t *reader = (flac_mem_reader_t *)client_data;
	(void)decoder;
	if ( !reader || !absolute_byte_offset ) {
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
	}
	*absolute_byte_offset = (FLAC__uint64)reader->pos;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus S_FLAC_Length( const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data )
{
	flac_mem_reader_t *reader = (flac_mem_reader_t *)client_data;
	(void)decoder;
	if ( !reader || !stream_length ) {
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	}
	*stream_length = (FLAC__uint64)reader->length;
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool S_FLAC_Eof( const FLAC__StreamDecoder *decoder, void *client_data )
{
	flac_mem_reader_t *reader = (flac_mem_reader_t *)client_data;
	(void)decoder;
	if ( !reader ) {
		return true;
	}
	return reader->pos >= reader->length;
}

static FLAC__StreamDecoderWriteStatus S_FLAC_Write( const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data )
{
	flac_decode_output_t *out = (flac_decode_output_t *)client_data;
	unsigned int i;
	unsigned int channels;
	unsigned int samples;
	int totalSamples;
	short *dst;
	(void)decoder;

	if ( !out || !frame ) {
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	channels = frame->header.channels;
	samples = frame->header.blocksize;
	totalSamples = out->samples + (int)samples;
	if ( totalSamples > out->samples ) {
		const int oldSamples = out->samples;
		const size_t newBytes = (size_t)totalSamples * channels * sizeof( short );
		short *newBuf = (short *)Z_Malloc( newBytes );
		if ( !newBuf ) {
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		if ( out->pcm && oldSamples > 0 ) {
			const size_t oldBytes = (size_t)oldSamples * channels * sizeof( short );
			Com_Memcpy( newBuf, out->pcm, oldBytes );
			Z_Free( out->pcm );
		}
		out->pcm = newBuf;
	}
	dst = out->pcm + out->samples * (int)channels;

	for ( i = 0; i < samples; i++ ) {
		unsigned int ch;
		for ( ch = 0; ch < channels; ch++ ) {
			int sample = buffer[ch][i];
			if ( sample > 32767 ) {
				sample = 32767;
			} else if ( sample < -32768 ) {
				sample = -32768;
			}
			*dst++ = (short)sample;
		}
	}

	out->samples = totalSamples;
	out->channels = (int)channels;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void S_FLAC_Metadata( const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data )
{
	flac_decode_output_t *out = (flac_decode_output_t *)client_data;
	(void)decoder;

	if ( !out || !metadata ) {
		return;
	}

	if ( metadata->type == FLAC__METADATA_TYPE_STREAMINFO ) {
		out->rate = (int)metadata->data.stream_info.sample_rate;
		out->channels = (int)metadata->data.stream_info.channels;
		out->bitsPerSample = (int)metadata->data.stream_info.bits_per_sample;
	}
}

static void S_FLAC_Error( const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data )
{
	(void)decoder;
	(void)status;
	(void)client_data;
}

static qboolean S_FLAC_DecodeFile( const char *filename, snd_info_t *info, short **outSamples, int *outSampleCount )
{
	fileHandle_t f;
	int length;
	byte *filebuf;
	FLAC__StreamDecoder *decoder;
	FLAC__StreamDecoderInitStatus initStatus;
	flac_mem_reader_t reader;
	flac_decode_output_t output;

	if ( !filename || !info || !outSamples || !outSampleCount ) {
		return qfalse;
	}

	length = FS_FOpenFileRead( filename, &f, qtrue );
	if ( f == FS_INVALID_HANDLE || length <= 0 ) {
		return qfalse;
	}

	filebuf = (byte *)Z_Malloc( length );
	FS_Read( filebuf, length, f );
	FS_FCloseFile( f );

	reader.data = filebuf;
	reader.length = length;
	reader.pos = 0;

	output.pcm = NULL;
	output.samples = 0;
	output.channels = 0;
	output.rate = 0;
	output.bitsPerSample = 0;

	decoder = FLAC__stream_decoder_new();
	if ( !decoder ) {
		Z_Free( filebuf );
		return qfalse;
	}

	initStatus = FLAC__stream_decoder_init_stream( decoder,
		S_FLAC_Read, S_FLAC_Seek, S_FLAC_Tell, S_FLAC_Length, S_FLAC_Eof,
		S_FLAC_Write, S_FLAC_Metadata, S_FLAC_Error, &reader );

	if ( initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK ) {
		FLAC__stream_decoder_delete( decoder );
		Z_Free( filebuf );
		return qfalse;
	}

	FLAC__stream_decoder_process_until_end_of_stream( decoder );
	FLAC__stream_decoder_finish( decoder );
	FLAC__stream_decoder_delete( decoder );
	Z_Free( filebuf );

	if ( output.samples <= 0 || output.channels <= 0 ) {
		if ( output.pcm ) {
			Z_Free( output.pcm );
		}
		return qfalse;
	}

	info->channels = output.channels;
	info->rate = output.rate > 0 ? output.rate : 44100;
	info->width = 2;
	info->samples = output.samples;
	info->size = info->samples * info->channels * info->width;
	info->dataofs = 0;

	*outSamples = output.pcm;
	*outSampleCount = output.samples;
	return qtrue;
}

void *S_FLAC_CodecLoad( const char *filename, snd_info_t *info )
{
	short *pcm = NULL;
	int samples = 0;

	if ( !S_FLAC_DecodeFile( filename, info, &pcm, &samples ) ) {
		return NULL;
	}

	return pcm;
}

snd_stream_t *S_FLAC_CodecOpenStream( const char *filename )
{
	snd_stream_t *stream;
	short *pcm = NULL;
	int samples = 0;

	stream = S_CodecUtilOpen( filename, NULL );
	if ( !stream ) {
		return NULL;
	}

	if ( !S_FLAC_DecodeFile( filename, &stream->info, &pcm, &samples ) ) {
		S_CodecUtilClose( &stream );
		return NULL;
	}

	stream->ptr = pcm;
	stream->length = stream->info.size;
	stream->pos = 0;
	return stream;
}

int S_FLAC_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer )
{
	int remaining;
	int toCopy;

	if ( !stream || !stream->ptr || !buffer || bytes <= 0 ) {
		return 0;
	}

	remaining = stream->length - stream->pos;
	if ( remaining <= 0 ) {
		return 0;
	}

	toCopy = bytes < remaining ? bytes : remaining;
	Com_Memcpy( buffer, (byte *)stream->ptr + stream->pos, toCopy );
	stream->pos += toCopy;
	return toCopy;
}

void S_FLAC_CodecCloseStream( snd_stream_t *stream )
{
	if ( !stream ) {
		return;
	}

	if ( stream->ptr ) {
		Z_Free( stream->ptr );
		stream->ptr = NULL;
	}

	S_CodecUtilClose( &stream );
}

snd_codec_t flac_codec =
{
	"flac",
	S_FLAC_CodecLoad,
	S_FLAC_CodecOpenStream,
	S_FLAC_CodecReadStream,
	S_FLAC_CodecCloseStream,
	NULL
};
#endif
