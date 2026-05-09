/*
 * Unit tests: adaptive Huffman packet compression/decompression.
 */
#include <stdio.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_EQ_INT(a, b, msg) do { \
	if ((a) != (b)) { \
		fprintf(stderr, "FAIL: %s (got %d want %d)\n", msg, (int)(a), (int)(b)); \
		return 1; \
	} \
} while (0)

#define ASSERT_MEMEQ(a, b, len, msg) do { \
	if (memcmp((a), (b), (len)) != 0) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static void init_msg( msg_t *msg, byte *data, int maxsize, int cursize )
{
	memset( msg, 0, sizeof( *msg ) );
	msg->data = data;
	msg->maxsize = maxsize;
	msg->maxbits = maxsize * 8;
	msg->cursize = cursize;
}

static int test_binary_payload_roundtrip_with_offset( void )
{
	enum {
		OFFSET = 12,
		PAYLOAD_LEN = 512,
		BUFFER_LEN = 8192
	};
	byte data[BUFFER_LEN];
	byte original[OFFSET + PAYLOAD_LEN];
	msg_t msg;
	int i;

	memset( data, 0xa5, sizeof( data ) );
	for ( i = 0; i < OFFSET; i++ ) {
		data[i] = (byte)(0x40 + i);
	}
	for ( i = 0; i < PAYLOAD_LEN; i++ ) {
		data[OFFSET + i] = (byte)(i & 0xff);
	}
	memcpy( original, data, sizeof( original ) );

	init_msg( &msg, data, BUFFER_LEN, OFFSET + PAYLOAD_LEN );
	Huff_Compress( &msg, OFFSET );

	ASSERT( msg.cursize > OFFSET, "compressed stream keeps offset header" );
	ASSERT( msg.cursize <= msg.maxsize, "compressed stream stays in buffer" );
	ASSERT_MEMEQ( data, original, OFFSET, "compress preserves bytes before offset" );

	Huff_Decompress( &msg, OFFSET );

	ASSERT_EQ_INT( msg.cursize, OFFSET + PAYLOAD_LEN, "decompressed cursize" );
	ASSERT_MEMEQ( data, original, sizeof( original ), "binary payload round-trip" );
	return 0;
}

static int test_truncated_stream_does_not_read_past_cursize( void )
{
	enum {
		OFFSET = 4,
		DECLARED_LEN = 4,
		BUFFER_LEN = 32
	};
	byte data[BUFFER_LEN];
	byte prefix[OFFSET];
	msg_t msg;
	int i;

	memset( data, 0x7f, sizeof( data ) );
	for ( i = 0; i < OFFSET; i++ ) {
		data[i] = (byte)(0x20 + i);
		prefix[i] = data[i];
	}
	data[OFFSET + 0] = 0;
	data[OFFSET + 1] = DECLARED_LEN;

	init_msg( &msg, data, BUFFER_LEN, OFFSET + 2 );
	Huff_Decompress( &msg, OFFSET );

	ASSERT_EQ_INT( msg.cursize, OFFSET + DECLARED_LEN, "truncated stream expands only to declared length" );
	ASSERT_MEMEQ( data, prefix, OFFSET, "truncated stream preserves prefix" );
	for ( i = 0; i < DECLARED_LEN; i++ ) {
		ASSERT_EQ_INT( data[OFFSET + i], 0, "truncated stream zero-fills unread payload" );
	}
	return 0;
}

static int test_declared_length_caps_to_message_buffer( void )
{
	enum {
		OFFSET = 6,
		BUFFER_LEN = 18
	};
	byte data[BUFFER_LEN];
	msg_t msg;
	int i;

	memset( data, 0, sizeof( data ) );
	for ( i = 0; i < OFFSET; i++ ) {
		data[i] = (byte)(0x60 + i);
	}
	data[OFFSET + 0] = 0xff;
	data[OFFSET + 1] = 0xff;

	init_msg( &msg, data, BUFFER_LEN, OFFSET + 2 );
	Huff_Decompress( &msg, OFFSET );

	ASSERT_EQ_INT( msg.cursize, BUFFER_LEN, "declared length caps at maxsize-offset" );
	for ( i = OFFSET; i < BUFFER_LEN; i++ ) {
		ASSERT_EQ_INT( data[i], 0, "capped malformed stream is deterministic" );
	}
	return 0;
}

int main( void )
{
	if ( test_binary_payload_roundtrip_with_offset() ) return 1;
	if ( test_truncated_stream_does_not_read_past_cursize() ) return 1;
	if ( test_declared_length_caps_to_message_buffer() ) return 1;
	printf( "PASS: unit_huffman_adaptive\n" );
	return 0;
}
