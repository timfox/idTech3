/*
 * Unit tests: msg_t / bitstream helpers (OOB + Huffman paths) from msg.c.
 * Built with -DDEDICATED so cl_shownet is not referenced.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

extern int gSTUB_SV_UTF8;

/* Huffman streams: MSG_WriteBits sets cursize = (bit>>3)+1, so a full bit
 * stream needs headroom beyond the logical message length. Production pairs
 * byte[MAX_MSGLEN_BUF] with MSG_Init(..., MAX_MSGLEN) - not sizeof(buf). */

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
	if ((a) != (b)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_STREQ(a, b, msg) do { \
	if (strcmp((a), (b)) != 0) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_NEAR(a, b, eps, msg) do { \
	if (fabs((double)(a) - (double)(b)) > (double)(eps)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_oob_roundtrip_primitives(void)
{
	byte buf[256];
	msg_t msg;

	MSG_InitOOB( &msg, buf, (int)sizeof( buf ) );
	MSG_WriteChar( &msg, -128 );
	MSG_WriteChar( &msg, 127 );
	MSG_WriteByte( &msg, 0 );
	MSG_WriteByte( &msg, 255 );
	MSG_WriteShort( &msg, -32768 );
	MSG_WriteShort( &msg, 32767 );
	MSG_WriteLong( &msg, -1 );
	MSG_WriteLong( &msg, 0x7fffffff );
	{
		floatint_t u;
		u.f = 3.14159265f;
		MSG_WriteFloat( &msg, u.f );
	}

	MSG_BeginReadingOOB( &msg );
	ASSERT_EQ( MSG_ReadChar( &msg ), -128, "OOB char min" );
	ASSERT_EQ( MSG_ReadChar( &msg ), 127, "OOB char max" );
	ASSERT_EQ( MSG_ReadByte( &msg ), 0, "OOB byte 0" );
	ASSERT_EQ( MSG_ReadByte( &msg ), 255, "OOB byte 255" );
	ASSERT_EQ( MSG_ReadShort( &msg ), -32768, "OOB short min" );
	ASSERT_EQ( MSG_ReadShort( &msg ), 32767, "OOB short max" );
	ASSERT_EQ( MSG_ReadLong( &msg ), -1, "OOB long -1" );
	ASSERT_EQ( MSG_ReadLong( &msg ), 0x7fffffff, "OOB long max int" );
	{
		floatint_t u;
		u.f = MSG_ReadFloat( &msg );
		ASSERT_NEAR( u.f, 3.14159265f, 1e-5f, "OOB float round-trip" );
	}
	ASSERT_EQ( msg.overflowed, qfalse, "OOB primitives overflow" );
	return 0;
}

static int test_oob_endian_layout(void)
{
	byte buf[16];
	msg_t msg;

	MSG_InitOOB( &msg, buf, (int)sizeof( buf ) );
	MSG_WriteLong( &msg, (int)0x01020304 );

#if defined(Q3_LITTLE_ENDIAN)
	ASSERT_EQ( buf[0], 0x04, "OOB long LE b0" );
	ASSERT_EQ( buf[1], 0x03, "OOB long LE b1" );
	ASSERT_EQ( buf[2], 0x02, "OOB long LE b2" );
	ASSERT_EQ( buf[3], 0x01, "OOB long LE b3" );
#endif
	MSG_BeginReadingOOB( &msg );
	ASSERT_EQ( MSG_ReadLong( &msg ), (int)0x01020304, "OOB long value round-trip" );
	return 0;
}

static int test_oob_write_data_read_data(void)
{
	byte buf[64];
	const char *payload = "ABCD";
	msg_t msg;

	MSG_InitOOB( &msg, buf, (int)sizeof( buf ) );
	MSG_WriteData( &msg, payload, 5 );

	MSG_BeginReadingOOB( &msg );
	{
		char out[8];
		MSG_ReadData( &msg, out, 5 );
		ASSERT_STREQ( out, "ABCD", "MSG_ReadData string" );
	}
	return 0;
}

static int test_oob_strings(void)
{
	byte buf[1024];
	msg_t msg;

	MSG_InitOOB( &msg, buf, (int)sizeof( buf ) );
	MSG_WriteString( &msg, "hello%world" );
	MSG_BeginReadingOOB( &msg );
	ASSERT_STREQ( MSG_ReadString( &msg ), "hello.world", "ReadString sanitizes %" );

	MSG_Clear( &msg );
	MSG_WriteString( &msg, NULL );
	MSG_BeginReadingOOB( &msg );
	ASSERT_STREQ( MSG_ReadString( &msg ), "", "NULL write -> empty read" );

	MSG_Clear( &msg );
	MSG_WriteBigString( &msg, "x\xffy" );
	MSG_BeginReadingOOB( &msg );
	ASSERT_STREQ( MSG_ReadBigString( &msg ), "x.y", "BigString strips high ASCII" );

	MSG_Clear( &msg );
	MSG_WriteString( &msg, "line1\nline2" );
	MSG_BeginReadingOOB( &msg );
	ASSERT_STREQ( MSG_ReadStringLine( &msg ), "line1", "ReadStringLine stops at newline" );

	MSG_Clear( &msg );
	MSG_WriteString( &msg, "" );
	MSG_BeginReadingOOB( &msg );
	ASSERT_STREQ( MSG_ReadString( &msg ), "", "empty WriteString" );
	return 0;
}

static int test_write_string_utf8_stub_branches(void)
{
	byte buf[64];
	msg_t msg;

	/* High byte on wire: UTF-8 on keeps 0xff; legacy strips to '.' */
	gSTUB_SV_UTF8 = 1;
	MSG_ResetStringCvarCacheForTesting();
	MSG_InitOOB( &msg, buf, (int)sizeof( buf ) );
	MSG_WriteString( &msg, "x\xffy" );
	MSG_BeginReadingOOB( &msg );
	ASSERT_EQ( MSG_ReadByte( &msg ), (int)(unsigned char)'x', "utf8 on wire x" );
	ASSERT_EQ( MSG_ReadByte( &msg ), 0xff, "utf8 on keeps high byte" );
	ASSERT_EQ( MSG_ReadByte( &msg ), (int)(unsigned char)'y', "utf8 on wire y" );
	ASSERT_EQ( MSG_ReadByte( &msg ), 0, "utf8 on nul" );

	gSTUB_SV_UTF8 = 0;
	MSG_ResetStringCvarCacheForTesting();
	MSG_Clear( &msg );
	MSG_WriteString( &msg, "x\xffy" );
	MSG_BeginReadingOOB( &msg );
	ASSERT_EQ( MSG_ReadByte( &msg ), (int)(unsigned char)'x', "utf8 off wire x" );
	ASSERT_EQ( MSG_ReadByte( &msg ), (int)'.', "utf8 off strips high byte" );
	ASSERT_EQ( MSG_ReadByte( &msg ), (int)(unsigned char)'y', "utf8 off wire y" );
	ASSERT_EQ( MSG_ReadByte( &msg ), 0, "utf8 off nul" );

	gSTUB_SV_UTF8 = 1;
	MSG_ResetStringCvarCacheForTesting();
	return 0;
}

static int test_bitstream_long_edges(void)
{
	byte buf[MAX_MSGLEN_BUF];
	msg_t msg;

	MSG_Init( &msg, buf, MAX_MSGLEN );
	MSG_Bitstream( &msg );
	MSG_WriteLong( &msg, 0 );
	MSG_WriteLong( &msg, (int)0x80000000 );

	MSG_BeginReading( &msg );
	ASSERT_EQ( MSG_ReadLong( &msg ), 0, "huffman long 0" );
	ASSERT_EQ( MSG_ReadLong( &msg ), (int)0x80000000, "huffman long sign bit" );
	return 0;
}

static int test_angle16_roundtrip(void)
{
	byte buf[64];
	msg_t msg;
	float ang = 90.0f;

	MSG_InitOOB( &msg, buf, (int)sizeof( buf ) );
	MSG_WriteAngle16( &msg, ang );
	MSG_BeginReadingOOB( &msg );
	ASSERT_NEAR( MSG_ReadAngle16( &msg ), ang, 0.02, "angle16 round-trip" );
	return 0;
}

static int test_entitynum_bitstream(void)
{
	byte buf[MAX_MSGLEN_BUF];
	msg_t msg;
	const int n = (1 << (GENTITYNUM_BITS - 1)) - 1;

	MSG_Init( &msg, buf, MAX_MSGLEN );
	MSG_Bitstream( &msg );
	MSG_WriteBits( &msg, n, GENTITYNUM_BITS );
	MSG_BeginReading( &msg );
	ASSERT_EQ( MSG_ReadEntitynum( &msg ), n, "entitynum round-trip" );
	return 0;
}

static int test_bitstream_roundtrip_short(void)
{
	byte buf[MAX_MSGLEN_BUF];
	msg_t msg;

	MSG_Init( &msg, buf, MAX_MSGLEN );
	MSG_Bitstream( &msg );
	MSG_WriteShort( &msg, -12345 );
	MSG_WriteShort( &msg, 32000 );

	MSG_BeginReading( &msg );
	ASSERT_EQ( MSG_ReadShort( &msg ), -12345, "huffman short a" );
	ASSERT_EQ( MSG_ReadShort( &msg ), 32000, "huffman short b" );
	ASSERT_EQ( msg.overflowed, qfalse, "bitstream short overflow" );
	return 0;
}

static int test_bitstream_char_negative_roundtrip(void)
{
	byte buf[MAX_MSGLEN_BUF];
	msg_t msg;

	/* WriteChar masks to 8 bits; ReadChar reinterprets as signed char */
	MSG_Init( &msg, buf, MAX_MSGLEN );
	MSG_Bitstream( &msg );
	MSG_WriteChar( &msg, -3 );

	MSG_BeginReading( &msg );
	ASSERT_EQ( MSG_ReadChar( &msg ), -3, "huffman char negative round-trip" );
	return 0;
}

static int test_overflow_sets_flag(void)
{
	byte buf[4];
	msg_t msg;

	MSG_InitOOB( &msg, buf, 4 );
	MSG_WriteLong( &msg, 1 );
	MSG_WriteLong( &msg, 2 );
	ASSERT_EQ( msg.overflowed, qtrue, "OOB overflow flag" );
	return 0;
}

static int test_msg_hash_key(void)
{
	ASSERT_EQ( MSG_HashKey( "ab", 4 ), MSG_HashKey( "ab", 4 ), "MSG_HashKey stable" );
	ASSERT_EQ( MSG_HashKey( "a%b", 4 ), MSG_HashKey( "a.b", 4 ), "MSG_HashKey % like high ascii" );
	return 0;
}

static int test_delta_usercmd_roundtrip(void)
{
	byte buf[MAX_MSGLEN_BUF];
	msg_t msg;
	usercmd_t from, to;

	memset( &from, 0, sizeof( from ) );
	memset( &to, 0, sizeof( to ) );
	from.serverTime = 1000;
	to.serverTime = 1005;
	from.angles[0] = from.angles[1] = from.angles[2] = 0;
	/* Keep angles in 0..32767 so delta XOR round-trips as plain ints in usercmd_t */
	to.angles[0] = 100;
	to.angles[1] = 200;
	to.angles[2] = 300;
	from.forwardmove = 10;
	to.forwardmove = -128;
	from.rightmove = 0;
	to.rightmove = 20;
	from.upmove = 0;
	to.upmove = -30;
	from.buttons = 0;
	to.buttons = 0xabcd;
	from.weapon = 3;
	to.weapon = 7;

	MSG_Init( &msg, buf, MAX_MSGLEN );
	MSG_Bitstream( &msg );
	MSG_WriteDeltaUsercmdKey( &msg, 0x11223344, &from, &to, 16 );

	MSG_BeginReading( &msg );
	{
		usercmd_t out;
		memset( &out, 0, sizeof( out ) );
		MSG_ReadDeltaUsercmdKey( &msg, 0x11223344, &from, &out, 16 );
		ASSERT_EQ( out.serverTime, to.serverTime, "delta cmd time" );
		ASSERT_EQ( out.angles[0], to.angles[0], "delta cmd pitch" );
		ASSERT_EQ( out.angles[1], to.angles[1], "delta cmd yaw (non-negative)" );
		ASSERT_EQ( out.angles[2], to.angles[2], "delta cmd roll" );
		ASSERT_EQ( out.forwardmove, -127, "delta cmd forward -128 -> -127" );
		ASSERT_EQ( out.rightmove, to.rightmove, "delta cmd right" );
		ASSERT_EQ( out.upmove, to.upmove, "delta cmd up" );
		ASSERT_EQ( out.buttons, to.buttons, "delta cmd buttons" );
		ASSERT_EQ( out.weapon, to.weapon, "delta cmd weapon" );
	}
	return 0;
}

static int test_delta_usercmd_no_change(void)
{
	byte buf[MAX_MSGLEN_BUF];
	msg_t msg;
	usercmd_t a;

	memset( &a, 0, sizeof( a ) );
	a.serverTime = 500;
	a.angles[0] = a.angles[1] = a.angles[2] = 16;
	a.forwardmove = 1;
	a.rightmove = 2;
	a.upmove = 3;
	a.buttons = 4;
	a.weapon = 5;

	MSG_Init( &msg, buf, MAX_MSGLEN );
	MSG_Bitstream( &msg );
	MSG_WriteDeltaUsercmdKey( &msg, 0, &a, &a, 16 );

	MSG_BeginReading( &msg );
	{
		usercmd_t out;
		memset( &out, 0xff, sizeof( out ) );
		MSG_ReadDeltaUsercmdKey( &msg, 0, &a, &out, 16 );
		ASSERT_EQ( out.serverTime, a.serverTime, "no-change time" );
		ASSERT_EQ( out.angles[0], a.angles[0], "no-change angles[0]" );
		ASSERT_EQ( out.forwardmove, a.forwardmove, "no-change forward" );
		ASSERT_EQ( out.weapon, a.weapon, "no-change weapon" );
	}
	return 0;
}

static int test_delta_usercmd_vr_buttons32(void)
{
	byte buf[MAX_MSGLEN_BUF];
	msg_t msg;
	usercmd_t from, to;
	/* Pitch/yaw packed into bits 12-25 (protocol: never all-zero for VR) */
	const int vrButtons = (7 << 12) | (64 << 19) | 0x001; /* bit0 + VR payload */

	memset( &from, 0, sizeof( from ) );
	memset( &to, 0, sizeof( to ) );
	from.serverTime = 1000;
	to.serverTime = 1008;
	to.buttons = vrButtons;
	to.weapon = 2;

	MSG_Init( &msg, buf, MAX_MSGLEN );
	MSG_Bitstream( &msg );
	MSG_WriteDeltaUsercmdKey( &msg, 0x55aa, &from, &to, 32 );

	MSG_BeginReading( &msg );
	{
		usercmd_t out;
		memset( &out, 0, sizeof( out ) );
		MSG_ReadDeltaUsercmdKey( &msg, 0x55aa, &from, &out, 32 );
		ASSERT_EQ( out.buttons, vrButtons, "32-bit VR buttons roundtrip" );
		ASSERT_EQ( out.weapon, to.weapon, "weapon after 32-bit buttons" );
	}
	return 0;
}

static int test_msg_init_clear(void)
{
	byte buf[32];
	msg_t msg;

	MSG_Init( &msg, buf, (int)sizeof( buf ) );
	ASSERT_EQ( msg.maxsize, (int)sizeof( buf ), "MSG_Init maxsize" );
	ASSERT_EQ( msg.data, buf, "MSG_Init data ptr" );
	MSG_WriteByte( &msg, 0x5a );
	ASSERT_EQ( msg.cursize > 0, qtrue, "wrote byte" );
	MSG_Clear( &msg );
	ASSERT_EQ( msg.cursize, 0, "MSG_Clear cursize" );
	ASSERT_EQ( msg.overflowed, qfalse, "MSG_Clear overflow" );
	return 0;
}

int main( void )
{
	if ( test_msg_init_clear() ) return 1;
	if ( test_oob_roundtrip_primitives() ) return 1;
	if ( test_oob_endian_layout() ) return 1;
	if ( test_oob_write_data_read_data() ) return 1;
	if ( test_oob_strings() ) return 1;
	if ( test_write_string_utf8_stub_branches() ) return 1;
	if ( test_bitstream_long_edges() ) return 1;
	if ( test_angle16_roundtrip() ) return 1;
	if ( test_entitynum_bitstream() ) return 1;
	if ( test_bitstream_roundtrip_short() ) return 1;
	if ( test_bitstream_char_negative_roundtrip() ) return 1;
	if ( test_overflow_sets_flag() ) return 1;
	if ( test_msg_hash_key() ) return 1;
	if ( test_delta_usercmd_roundtrip() ) return 1;
	if ( test_delta_usercmd_no_change() ) return 1;
	if ( test_delta_usercmd_vr_buttons32() ) return 1;
	return 0;
}
