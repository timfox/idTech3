/*
 * Unit tests: byte-order helpers in q_shared.c (ShortSwap, LongSwap, 64-bit,
 * float, Copy*Swap). Complements unit_qhelpers without duplicating its cases.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"

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

#define ASSERT_NEAR(a, b, eps, msg) do { \
	if (fabsf((a) - (b)) > (eps)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_no_swap_identity(void)
{
	short s = -12345;
	int L = 0x12345678;
	qint64 q = { 1, 2, 3, 4, 5, 6, 7, 8 };
	float f = 2.5f;

	ASSERT_EQ( ShortNoSwap( s ), s, "ShortNoSwap" );
	ASSERT_EQ( LongNoSwap( L ), L, "LongNoSwap" );
	{
		qint64 o = Long64NoSwap( q );
		ASSERT_EQ( o.b0, q.b0, "Long64NoSwap b0" );
		ASSERT_EQ( o.b7, q.b7, "Long64NoSwap b7" );
	}
	ASSERT_NEAR( FloatNoSwap( &f ), f, 0.0f, "FloatNoSwap" );
	return 0;
}

static int test_short_long_swap_roundtrip(void)
{
	short vals[] = { 0, 1, -1, 0x7fff, (short)0x8000, 0x1234 };
	int i;

	for ( i = 0; i < (int)( sizeof( vals ) / sizeof( vals[0] ) ); i++ ) {
		short v = vals[i];
		ASSERT_EQ( ShortSwap( ShortSwap( v ) ), v, "ShortSwap involution" );
	}

	ASSERT_EQ( LongSwap( LongSwap( 0 ) ), 0, "LongSwap 0" );
	ASSERT_EQ( LongSwap( LongSwap( -1 ) ), -1, "LongSwap -1" );
	ASSERT_EQ( LongSwap( LongSwap( 0x0badf00d ) ), (int)0x0badf00d, "LongSwap pattern" );
	return 0;
}

static int test_long64_swap(void)
{
	qint64 in = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	qint64 out = Long64Swap( in );

	ASSERT_EQ( out.b0, 0x08, "Long64Swap b0" );
	ASSERT_EQ( out.b1, 0x07, "Long64Swap b1" );
	ASSERT_EQ( out.b6, 0x02, "Long64Swap b6" );
	ASSERT_EQ( out.b7, 0x01, "Long64Swap b7" );

	ASSERT_EQ( Long64Swap( Long64Swap( in ) ).b0, in.b0, "Long64Swap involution b0" );
	ASSERT_EQ( Long64Swap( Long64Swap( in ) ).b7, in.b7, "Long64Swap involution b7" );
	return 0;
}

static int test_float_swap_bitpattern(void)
{
	float f = 3.14159265f;
	floatint_t u;
	int raw;

	u.f = f;
	raw = u.i;
	u.i = LongSwap( raw );
	ASSERT_NEAR( FloatSwap( &f ), u.f, 1e-6f, "FloatSwap matches LongSwap bits" );

	{
		float t = FloatSwap( &f );
		ASSERT_NEAR( FloatSwap( &t ), f, 1e-6f, "FloatSwap involution" );
	}
	return 0;
}

static int test_copy_swap_roundtrip_memory(void)
{
	unsigned char buf[8];
	int L = 0xaabbccdd;

	memset( buf, 0, sizeof( buf ) );
	CopyLongSwap( buf, &L );
	CopyLongSwap( &L, buf );
	ASSERT_EQ( L, (int)0xaabbccdd, "CopyLongSwap round-trip" );

	{
		short s = 0x5a5b;
		unsigned char b[2];
		CopyShortSwap( b, &s );
		CopyShortSwap( &s, b );
		ASSERT_EQ( (unsigned short)s, (unsigned short)0x5a5b, "CopyShortSwap round-trip" );
	}
	return 0;
}

int main( void )
{
	if ( test_no_swap_identity() ) return 1;
	if ( test_short_long_swap_roundtrip() ) return 1;
	if ( test_long64_swap() ) return 1;
	if ( test_float_swap_bitpattern() ) return 1;
	if ( test_copy_swap_roundtrip_memory() ) return 1;
	return 0;
}
