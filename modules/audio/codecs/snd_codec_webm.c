/*
===============================================================================
WebM audio codec wrapper for idTech3 sound system
===============================================================================
*/

#include "../../client/client.h"
#include "snd_codec.h"
#include "../snd_local.h"
#include <string.h>

#ifdef USE_WEBM
#include <nestegg/nestegg.h>
#include <opus.h>

typedef struct {
	fileHandle_t file;
	int length;
} webm_io_t;

typedef enum {
	WEBM_CODEC_UNKNOWN = 0,
	WEBM_CODEC_OPUS,
	WEBM_CODEC_VORBIS
} webm_codec_t;

static int S_WEBM_Read( void *buffer, size_t length, void *userdata )
{
	webm_io_t *io = (webm_io_t *)userdata;
	int bytesRead;

	if ( !io || !buffer || length == 0 ) {
		return -1;
	}

	bytesRead = FS_Read( buffer, (int)length, io->file );
	if ( bytesRead < 0 ) {
		return -1;
	}
	if ( bytesRead == 0 ) {
		return 0;
	}
	if ( (size_t)bytesRead < length ) {
		return 0;
	}

	return 1;
}

static int S_WEBM_Seek( int64_t offset, int whence, void *userdata )
{
	webm_io_t *io = (webm_io_t *)userdata;
	int origin;

	if ( !io ) {
		return -1;
	}

	switch ( whence ) {
		case NESTEGG_SEEK_SET: origin = FS_SEEK_SET; break;
		case NESTEGG_SEEK_CUR: origin = FS_SEEK_CUR; break;
		case NESTEGG_SEEK_END: origin = FS_SEEK_END; break;
		default: return -1;
	}

	return FS_Seek( io->file, (long)offset, origin ) == 0 ? 0 : -1;
}

static int64_t S_WEBM_Tell( void *userdata )
{
	webm_io_t *io = (webm_io_t *)userdata;
	if ( !io ) {
		return -1;
	}
	return (int64_t)FS_FTell( io->file );
}

static qboolean S_WEBM_ParseOpusHeader( const unsigned char *data, size_t length, int *channels, int *preskip )
{
	if ( !data || length < 19 || !channels || !preskip ) {
		return qfalse;
	}

	if ( memcmp( data, "OpusHead", 8 ) != 0 ) {
		return qfalse;
	}

	*channels = data[9];
	*preskip = (int)( data[10] | ( data[11] << 8 ) );
	return qtrue;
}

static webm_codec_t S_WEBM_DetectCodec( nestegg *ctx, unsigned int track, int *channels, int *preskip )
{
	unsigned int count = 0;
	unsigned char *data = NULL;
	size_t length = 0;
	int codecId;

	if ( channels ) {
		*channels = 0;
	}
	if ( preskip ) {
		*preskip = 0;
	}

	codecId = nestegg_track_codec_id( ctx, track );
	if ( codecId == NESTEGG_CODEC_VORBIS ) {
		return WEBM_CODEC_VORBIS;
	}

	if ( nestegg_track_codec_data_count( ctx, track, &count ) != 0 || count == 0 ) {
		return WEBM_CODEC_UNKNOWN;
	}

	if ( nestegg_track_codec_data( ctx, track, 0, &data, &length ) != 0 || !data || length == 0 ) {
		return WEBM_CODEC_UNKNOWN;
	}

	if ( channels && preskip && S_WEBM_ParseOpusHeader( data, length, channels, preskip ) ) {
		return WEBM_CODEC_OPUS;
	}

	if ( length >= 7 && data[0] == 0x01 && memcmp( data + 1, "vorbis", 6 ) == 0 ) {
		return WEBM_CODEC_VORBIS;
	}

	return WEBM_CODEC_UNKNOWN;
}

static qboolean S_WEBM_AppendSamples( short **pcm, int *samples, int channels, const short *data, int count )
{
	int newSamples;
	short *newBuf;

	if ( !pcm || !samples || !data || count <= 0 ) {
		return qfalse;
	}

	newSamples = *samples + count;
	{
		const size_t newBytes = (size_t)newSamples * channels * sizeof( short );
		newBuf = (short *)Z_Malloc( newBytes );
	}
	if ( !newBuf ) {
		if ( *pcm ) {
			Z_Free( *pcm );
			*pcm = NULL;
		}
		*samples = 0;
		return qfalse;
	}
	if ( *pcm && *samples > 0 ) {
		const size_t oldBytes = (size_t)( *samples ) * channels * sizeof( short );
		Com_Memcpy( newBuf, *pcm, oldBytes );
		Z_Free( *pcm );
	}
	Com_Memcpy( newBuf + ( *samples * channels ), data, count * channels * sizeof( short ) );
	*pcm = newBuf;
	*samples = newSamples;
	return qtrue;
}

static qboolean S_WEBM_DecodeOpus( nestegg *ctx, unsigned int track, int channels, int preskip, short **outSamples, int *outSampleCount )
{
	const int maxFrameSamples = 5760;
	OpusDecoder *decoder;
	short *pcm = NULL;
	short *temp;
	int totalSamples = 0;
	int r;
	int err;
	nestegg_packet *pkt = NULL;

	if ( !ctx || !outSamples || !outSampleCount || channels <= 0 ) {
		return qfalse;
	}

	decoder = opus_decoder_create( 48000, channels, &err );
	if ( !decoder || err != OPUS_OK ) {
		return qfalse;
	}

	temp = (short *)Z_Malloc( maxFrameSamples * channels * sizeof( short ) );
	if ( !temp ) {
		opus_decoder_destroy( decoder );
		return qfalse;
	}

	while ( ( r = nestegg_read_packet( ctx, &pkt ) ) > 0 ) {
		unsigned int pktTrack = 0;
		unsigned int chunks = 0;
		unsigned int i;

		if ( nestegg_packet_track( pkt, &pktTrack ) != 0 ) {
			nestegg_free_packet( pkt );
			continue;
		}

		if ( pktTrack != track ) {
			nestegg_free_packet( pkt );
			continue;
		}

		if ( nestegg_packet_count( pkt, &chunks ) != 0 ) {
			nestegg_free_packet( pkt );
			continue;
		}

		for ( i = 0; i < chunks; i++ ) {
			unsigned char *data = NULL;
			size_t length = 0;
			int decoded;
			int start = 0;

			if ( nestegg_packet_data( pkt, i, &data, &length ) != 0 || !data || length == 0 ) {
				continue;
			}

			decoded = opus_decode( decoder, data, (opus_int32)length, temp, maxFrameSamples, 0 );
			if ( decoded <= 0 ) {
				continue;
			}

			if ( preskip > 0 ) {
				int skip = preskip < decoded ? preskip : decoded;
				preskip -= skip;
				start = skip;
				decoded -= skip;
			}

			if ( decoded > 0 ) {
				if ( !S_WEBM_AppendSamples( &pcm, &totalSamples, channels, temp + start * channels, decoded ) ) {
					nestegg_free_packet( pkt );
					Z_Free( temp );
					opus_decoder_destroy( decoder );
					return qfalse;
				}
			}
		}

		nestegg_free_packet( pkt );
	}

	if ( r < 0 ) {
		if ( pcm ) {
			Z_Free( pcm );
			pcm = NULL;
		}
	}

	Z_Free( temp );
	opus_decoder_destroy( decoder );

	if ( !pcm || totalSamples <= 0 ) {
		return qfalse;
	}

	*outSamples = pcm;
	*outSampleCount = totalSamples;
	return qtrue;
}

static qboolean S_WEBM_DecodeFile( const char *filename, snd_info_t *info, short **outSamples, int *outSampleCount )
{
	fileHandle_t f;
	int length;
	webm_io_t io;
	nestegg_io callbacks;
	nestegg *ctx = NULL;
	unsigned int tracks = 0;
	unsigned int track;
	nestegg_audio_params params;
	webm_codec_t codec = WEBM_CODEC_UNKNOWN;
	int channels = 0;
	int preskip = 0;
	qboolean decoded = qfalse;

	if ( !filename || !info || !outSamples || !outSampleCount ) {
		return qfalse;
	}

	length = FS_FOpenFileRead( filename, &f, qtrue );
	if ( f == FS_INVALID_HANDLE || length <= 0 ) {
		return qfalse;
	}

	io.file = f;
	io.length = length;

	callbacks.read = S_WEBM_Read;
	callbacks.seek = S_WEBM_Seek;
	callbacks.tell = S_WEBM_Tell;
	callbacks.userdata = &io;

	if ( nestegg_init( &ctx, callbacks, NULL ) != 0 ) {
		FS_FCloseFile( f );
		return qfalse;
	}

	if ( nestegg_track_count( ctx, &tracks ) != 0 || tracks == 0 ) {
		nestegg_destroy( ctx );
		FS_FCloseFile( f );
		return qfalse;
	}

	for ( track = 0; track < tracks; track++ ) {
		if ( nestegg_track_type( ctx, track ) != NESTEGG_TRACK_AUDIO ) {
			continue;
		}

		if ( nestegg_track_audio_params( ctx, track, &params ) != 0 ) {
			continue;
		}

		codec = S_WEBM_DetectCodec( ctx, track, &channels, &preskip );
		if ( codec == WEBM_CODEC_OPUS ) {
			if ( channels <= 0 ) {
				channels = (int)params.channels;
			}
			decoded = S_WEBM_DecodeOpus( ctx, track, channels, preskip, outSamples, outSampleCount );
			break;
		}

		if ( codec == WEBM_CODEC_VORBIS ) {
			Com_Printf( S_COLOR_YELLOW "WebM Vorbis track detected but Vorbis-in-WebM decode is not supported yet.\n" );
			break;
		}

		if ( codec == WEBM_CODEC_UNKNOWN ) {
			Com_Printf( S_COLOR_YELLOW "WebM audio track codec is not supported.\n" );
			break;
		}
	}

	nestegg_destroy( ctx );
	FS_FCloseFile( f );

	if ( !decoded || !*outSamples || *outSampleCount <= 0 || channels <= 0 ) {
		if ( *outSamples ) {
			Z_Free( *outSamples );
			*outSamples = NULL;
		}
		*outSampleCount = 0;
		return qfalse;
	}

	info->channels = channels;
	info->rate = 48000;
	info->width = 2;
	info->samples = *outSampleCount;
	info->size = info->samples * info->channels * info->width;
	info->dataofs = 0;

	return qtrue;
}

void *S_WEBM_CodecLoad( const char *filename, snd_info_t *info )
{
	short *pcm = NULL;
	int samples = 0;

	if ( !S_WEBM_DecodeFile( filename, info, &pcm, &samples ) ) {
		return NULL;
	}

	return pcm;
}

snd_stream_t *S_WEBM_CodecOpenStream( const char *filename )
{
	snd_stream_t *stream;
	short *pcm = NULL;
	int samples = 0;

	stream = S_CodecUtilOpen( filename, &webm_codec );
	if ( !stream ) {
		return NULL;
	}

	if ( !S_WEBM_DecodeFile( filename, &stream->info, &pcm, &samples ) ) {
		S_CodecUtilClose( &stream );
		return NULL;
	}

	stream->ptr = pcm;
	stream->length = stream->info.size;
	stream->pos = 0;
	return stream;
}

int S_WEBM_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer )
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

void S_WEBM_CodecCloseStream( snd_stream_t *stream )
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

snd_codec_t webm_codec =
{
	"webm",
	S_WEBM_CodecLoad,
	S_WEBM_CodecOpenStream,
	S_WEBM_CodecReadStream,
	S_WEBM_CodecCloseStream,
	NULL
};

#endif
