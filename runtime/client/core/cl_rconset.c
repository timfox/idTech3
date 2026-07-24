// cl_rconset.c -- Engine-side rcon_autoset handler. See header for
// protocol overview.

#include "client.h"
#include "cl_rconset.h"

// ---------------------------------------------------------------------------
// SipHash-2-4, 128-bit output, raw byte form. Byte-identical to the Go
// implementation used by the Surf hub encryptor (and to Trinity's
// prior siphash.go / QVM BG_HashKeyed layout). Operates on byte buffers
// with explicit lengths so payloads with embedded NULs are safe.
// ---------------------------------------------------------------------------

#define SIPROUND \
	v0 += v1; v2 += v3; \
	v1 = (v1 << 13) | (v1 >> (64 - 13)); \
	v3 = (v3 << 16) | (v3 >> (64 - 16)); \
	v1 ^= v0; v3 ^= v2; \
	v0 = (v0 << 32) | (v0 >> (64 - 32)); \
	v2 += v1; v0 += v3; \
	v1 = (v1 << 17) | (v1 >> (64 - 17)); \
	v3 = (v3 << 21) | (v3 >> (64 - 21)); \
	v1 ^= v2; v3 ^= v0; \
	v2 = (v2 << 32) | (v2 >> (64 - 32))

// DeriveKey folds an arbitrary-length byte slice into two uint64 key
// halves.
static void DeriveKey( const byte *key, int keyLen, uint64_t *k0, uint64_t *k1 ) {
	unsigned int h[4];
	int i;
	h[0] = 0x736f6d65;
	h[1] = 0x646f7261;
	h[2] = 0x6c796765;
	h[3] = 0x74656462;
	for ( i = 0; i < keyLen; i++ ) {
		h[i & 3] ^= (unsigned char)key[i];
		h[i & 3] *= 0x01000193u;
	}
	*k0 = ( (uint64_t)h[0] << 32 ) | (uint64_t)h[1];
	*k1 = ( (uint64_t)h[2] << 32 ) | (uint64_t)h[3];
}

static void SipHash128Raw( const byte *key, int keyLen, const byte *msg, int msgLen, byte out[16] ) {
	uint64_t v0, v1, v2, v3, k0, k1, m;
	int blocks, i, left;
	uint64_t hash0, hash1;

	DeriveKey( key, keyLen, &k0, &k1 );

	v0 = k0 ^ 0x736f6d6570736575ULL;
	v1 = k1 ^ 0x646f72616e646f6dULL;
	v2 = k0 ^ 0x6c7967656e657261ULL;
	v3 = k1 ^ 0x7465646279746573ULL;

	v1 ^= 0xeeULL; // 128-bit output tag

	blocks = msgLen / 8;
	for ( i = 0; i < blocks; i++ ) {
		m = (uint64_t)msg[i*8]
			| ( (uint64_t)msg[i*8+1] << 8 )
			| ( (uint64_t)msg[i*8+2] << 16 )
			| ( (uint64_t)msg[i*8+3] << 24 )
			| ( (uint64_t)msg[i*8+4] << 32 )
			| ( (uint64_t)msg[i*8+5] << 40 )
			| ( (uint64_t)msg[i*8+6] << 48 )
			| ( (uint64_t)msg[i*8+7] << 56 );
		v3 ^= m;
		SIPROUND;
		SIPROUND;
		v0 ^= m;
	}

	m = 0;
	left = msgLen & 7;
	{
		int j;
		for ( j = left - 1; j >= 0; j-- ) {
			m <<= 8;
			m |= (uint64_t)(unsigned char)msg[blocks*8 + j];
		}
	}
	m |= (uint64_t)( msgLen & 0xff ) << 56;
	v3 ^= m;
	SIPROUND;
	SIPROUND;
	v0 ^= m;

	v2 ^= 0xeeULL;
	SIPROUND; SIPROUND; SIPROUND; SIPROUND;
	hash0 = v0 ^ v1 ^ v2 ^ v3;

	v1 ^= 0xddULL;
	SIPROUND; SIPROUND; SIPROUND; SIPROUND;
	hash1 = v0 ^ v1 ^ v2 ^ v3;

	// Byte layout:
	//   bytes 0-3:  hash0 low-32 big-endian
	//   bytes 4-7:  hash0 high-32 big-endian
	//   bytes 8-11: hash1 low-32 big-endian
	//   bytes 12-15: hash1 high-32 big-endian
	out[0]  = (byte)( hash0 >> 24 );
	out[1]  = (byte)( hash0 >> 16 );
	out[2]  = (byte)( hash0 >> 8 );
	out[3]  = (byte)( hash0 );
	out[4]  = (byte)( hash0 >> 56 );
	out[5]  = (byte)( hash0 >> 48 );
	out[6]  = (byte)( hash0 >> 40 );
	out[7]  = (byte)( hash0 >> 32 );
	out[8]  = (byte)( hash1 >> 24 );
	out[9]  = (byte)( hash1 >> 16 );
	out[10] = (byte)( hash1 >> 8 );
	out[11] = (byte)( hash1 );
	out[12] = (byte)( hash1 >> 56 );
	out[13] = (byte)( hash1 >> 48 );
	out[14] = (byte)( hash1 >> 40 );
	out[15] = (byte)( hash1 >> 32 );
}

#undef SIPROUND

// ---------------------------------------------------------------------------
// Rconset protocol constants. Hub encryptor must match these labels.
// ---------------------------------------------------------------------------

#define RCONSET_VERSION         1
#define RCONSET_NONCE_LEN       16
#define RCONSET_MAC_LEN         8
#define RCONSET_MAX_CT          64
#define RCONSET_HEADER_LEN      ( 2 + RCONSET_NONCE_LEN )
#define RCONSET_MAX_BLOB_BYTES  ( RCONSET_HEADER_LEN + RCONSET_MAX_CT + RCONSET_MAC_LEN )

static const char LABEL_KEY[] = "surf-rconset-key-v1";
static const char LABEL_KS[]  = "surf-rconset-ks-v1";
static const char LABEL_MAC[] = "surf-rconset-mac-v1";

// HexDecode parses up to outCap bytes from src (NUL-terminated) into out.
// Returns the number of bytes decoded on success, -1 on bad input.
static int HexDecode( const char *src, byte *out, int outCap ) {
	int srcLen = (int)strlen( src );
	int i;
	if ( srcLen & 1 ) return -1;
	if ( srcLen / 2 > outCap ) return -1;
	for ( i = 0; i < srcLen / 2; i++ ) {
		int hi, lo;
		char a = src[i*2], b = src[i*2 + 1];
		if      ( a >= '0' && a <= '9' ) hi = a - '0';
		else if ( a >= 'a' && a <= 'f' ) hi = 10 + a - 'a';
		else if ( a >= 'A' && a <= 'F' ) hi = 10 + a - 'A';
		else return -1;
		if      ( b >= '0' && b <= '9' ) lo = b - '0';
		else if ( b >= 'a' && b <= 'f' ) lo = 10 + b - 'a';
		else if ( b >= 'A' && b <= 'F' ) lo = 10 + b - 'A';
		else return -1;
		out[i] = (byte)( ( hi << 4 ) | lo );
	}
	return srcLen / 2;
}

// CTEqual is constant-time byte comparison. Returns 0 on equal.
static int CTEqual( const byte *a, const byte *b, int n ) {
	int i, diff = 0;
	for ( i = 0; i < n; i++ ) diff |= a[i] ^ b[i];
	return diff;
}

// Derive session key K from token + epoch nonce.
static void rconsetDeriveK( const byte *token, int tokenLen, const byte *epochNonce, byte K[16] ) {
	byte msg[64];
	int  msgLen = (int)strlen( LABEL_KEY );
	Com_Memcpy( msg, LABEL_KEY, msgLen );
	Com_Memcpy( msg + msgLen, epochNonce, RCONSET_NONCE_LEN );
	msgLen += RCONSET_NONCE_LEN;
	SipHash128Raw( token, tokenLen, msg, msgLen, K );
}

// rconsetDecrypt is the protocol-bearing core. Takes a token (raw bytes
// + length), the decoded blob, and writes the plaintext into pt with
// at most ptCap bytes. Returns the plaintext length on success, -1 on
// any failure (bad version, bad ct_len, MAC mismatch, length mismatch).
// Plaintext is NOT validated for printability — caller decides.
static int rconsetDecrypt(
	const byte *token, int tokenLen,
	const byte *blob, int blobLen,
	byte *pt, int ptCap )
{
	byte K[16];
	byte epochNonce[RCONSET_NONCE_LEN];
	int  ctLen;
	int  i, blocks;
	byte macInput[256];
	int  macInputLen;
	byte macFull[16];
	byte keystream[RCONSET_MAX_CT + 16];

	if ( blobLen < RCONSET_HEADER_LEN + RCONSET_MAC_LEN ) return -1;
	if ( blob[0] != RCONSET_VERSION )                     return -1;
	ctLen = (int)blob[1];
	if ( ctLen < 1 || ctLen > RCONSET_MAX_CT )            return -1;
	if ( blobLen != RCONSET_HEADER_LEN + ctLen + RCONSET_MAC_LEN ) return -1;
	if ( ctLen > ptCap )                                  return -1;

	Com_Memcpy( epochNonce, blob + 2, RCONSET_NONCE_LEN );
	rconsetDeriveK( token, tokenLen, epochNonce, K );

	// MAC verify
	{
		int labelLen = (int)strlen( LABEL_MAC );
		macInputLen = labelLen + RCONSET_HEADER_LEN + ctLen;
		if ( macInputLen > (int)sizeof( macInput ) ) return -1;
		Com_Memcpy( macInput, LABEL_MAC, labelLen );
		Com_Memcpy( macInput + labelLen, blob, RCONSET_HEADER_LEN + ctLen );
		SipHash128Raw( K, 16, macInput, macInputLen, macFull );
		if ( CTEqual( macFull, blob + RCONSET_HEADER_LEN + ctLen, RCONSET_MAC_LEN ) != 0 ) {
			return -1;
		}
	}

	// Keystream: KS_i = SipHash128( K, LABEL_KS || epochNonce || u32be(i) )
	blocks = ( ctLen + 15 ) / 16;
	for ( i = 0; i < blocks; i++ ) {
		byte msg[64];
		int  labelLen = (int)strlen( LABEL_KS );
		int  msgLen   = labelLen + RCONSET_NONCE_LEN + 4;
		if ( msgLen > (int)sizeof( msg ) ) return -1;
		Com_Memcpy( msg, LABEL_KS, labelLen );
		Com_Memcpy( msg + labelLen, epochNonce, RCONSET_NONCE_LEN );
		msg[ labelLen + RCONSET_NONCE_LEN + 0 ] = (byte)( i >> 24 );
		msg[ labelLen + RCONSET_NONCE_LEN + 1 ] = (byte)( i >> 16 );
		msg[ labelLen + RCONSET_NONCE_LEN + 2 ] = (byte)( i >> 8 );
		msg[ labelLen + RCONSET_NONCE_LEN + 3 ] = (byte)i;
		SipHash128Raw( K, 16, msg, msgLen, keystream + i * 16 );
	}
	for ( i = 0; i < ctLen; i++ ) {
		pt[i] = blob[ RCONSET_HEADER_LEN + i ] ^ keystream[i];
	}
	return ctLen;
}

// rconsetEncrypt builds a protocol blob for self-test / local round-trip.
// Returns blob length on success, -1 on failure.
static int rconsetEncrypt(
	const byte *token, int tokenLen,
	const byte *epochNonce,
	const byte *pt, int ptLen,
	byte *blob, int blobCap )
{
	byte K[16];
	byte keystream[RCONSET_MAX_CT + 16];
	byte macFull[16];
	byte macInput[256];
	int  i, blocks, blobLen, labelLen, macInputLen;

	if ( ptLen < 1 || ptLen > RCONSET_MAX_CT ) return -1;
	blobLen = RCONSET_HEADER_LEN + ptLen + RCONSET_MAC_LEN;
	if ( blobLen > blobCap ) return -1;

	rconsetDeriveK( token, tokenLen, epochNonce, K );

	blob[0] = RCONSET_VERSION;
	blob[1] = (byte)ptLen;
	Com_Memcpy( blob + 2, epochNonce, RCONSET_NONCE_LEN );

	blocks = ( ptLen + 15 ) / 16;
	for ( i = 0; i < blocks; i++ ) {
		byte msg[64];
		int  ksLabelLen = (int)strlen( LABEL_KS );
		int  msgLen     = ksLabelLen + RCONSET_NONCE_LEN + 4;
		Com_Memcpy( msg, LABEL_KS, ksLabelLen );
		Com_Memcpy( msg + ksLabelLen, epochNonce, RCONSET_NONCE_LEN );
		msg[ ksLabelLen + RCONSET_NONCE_LEN + 0 ] = (byte)( i >> 24 );
		msg[ ksLabelLen + RCONSET_NONCE_LEN + 1 ] = (byte)( i >> 16 );
		msg[ ksLabelLen + RCONSET_NONCE_LEN + 2 ] = (byte)( i >> 8 );
		msg[ ksLabelLen + RCONSET_NONCE_LEN + 3 ] = (byte)i;
		SipHash128Raw( K, 16, msg, msgLen, keystream + i * 16 );
	}
	for ( i = 0; i < ptLen; i++ ) {
		blob[ RCONSET_HEADER_LEN + i ] = pt[i] ^ keystream[i];
	}

	labelLen = (int)strlen( LABEL_MAC );
	macInputLen = labelLen + RCONSET_HEADER_LEN + ptLen;
	if ( macInputLen > (int)sizeof( macInput ) ) return -1;
	Com_Memcpy( macInput, LABEL_MAC, labelLen );
	Com_Memcpy( macInput + labelLen, blob, RCONSET_HEADER_LEN + ptLen );
	SipHash128Raw( K, 16, macInput, macInputLen, macFull );
	Com_Memcpy( blob + RCONSET_HEADER_LEN + ptLen, macFull, RCONSET_MAC_LEN );
	return blobLen;
}

// ---------------------------------------------------------------------------
// Public entry points.
// ---------------------------------------------------------------------------

void CL_HandleRconAutoset( const char *hexArg ) {
	byte blob[RCONSET_MAX_BLOB_BYTES];
	int  blobLen;
	char token[256];
	byte pt[RCONSET_MAX_CT + 1];
	int  ptLen;
	int  i;

	// Refuse outside a real multiplayer session: no demos, no local games.
	if ( clc.demoplaying ) {
		Com_DPrintf( "rcon_autoset: demo playback, ignoring\n" );
		return;
	}
	if ( com_sv_running && com_sv_running->integer ) {
		Com_DPrintf( "rcon_autoset: local-game context, ignoring\n" );
		return;
	}

	if ( !hexArg || !hexArg[0] ) {
		Com_DPrintf( "rcon_autoset: empty argument\n" );
		return;
	}

	blobLen = HexDecode( hexArg, blob, RCONSET_MAX_BLOB_BYTES );
	if ( blobLen < 0 ) {
		Com_DPrintf( "rcon_autoset: hex decode failed\n" );
		return;
	}

	Cvar_VariableStringBuffer( "cl_authToken", token, sizeof( token ) );
	if ( !token[0] ) {
		Com_DPrintf( "rcon_autoset: cl_authToken empty, ignoring\n" );
		return;
	}

	ptLen = rconsetDecrypt( (const byte *)token, (int)strlen( token ),
	                        blob, blobLen, pt, sizeof( pt ) - 1 );
	if ( ptLen < 0 ) {
		Com_DPrintf( "rcon_autoset: decrypt/MAC failed\n" );
		return;
	}

	// Validate plaintext: printable ASCII, no space, NUL-terminate.
	for ( i = 0; i < ptLen; i++ ) {
		if ( pt[i] < 0x21 || pt[i] > 0x7E ) {
			Com_DPrintf( "rcon_autoset: non-printable byte at %d\n", i );
			return;
		}
	}
	pt[ptLen] = '\0';

	Cvar_Set( "rconPassword", (const char *)pt );
	Com_Printf( "^5rcon access enabled for this server\n" );
}

// CL_RconsetSelfTest round-trips a known plaintext under surf-rconset
// labels so a crypto regression refuses to boot.
void CL_RconsetSelfTest( void ) {
	static const char token[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	static const char expectedPt[] = "x";
	byte epochNonce[RCONSET_NONCE_LEN];
	byte blob[RCONSET_MAX_BLOB_BYTES];
	int  blobLen;
	byte pt[RCONSET_MAX_CT + 1];
	int  ptLen;

	Com_Memset( epochNonce, 0, sizeof( epochNonce ) );

	blobLen = rconsetEncrypt(
		(const byte *)token, (int)strlen( token ),
		epochNonce,
		(const byte *)expectedPt, (int)strlen( expectedPt ),
		blob, sizeof( blob ) );
	if ( blobLen < 0 ) {
		Com_Error( ERR_FATAL, "rcon_autoset selftest: encrypt failed" );
	}

	ptLen = rconsetDecrypt( (const byte *)token, (int)strlen( token ),
	                        blob, blobLen, pt, sizeof( pt ) - 1 );
	if ( ptLen != (int)strlen( expectedPt ) ) {
		Com_Error( ERR_FATAL,
			"rcon_autoset selftest: ptLen = %d, want %d",
			ptLen, (int)strlen( expectedPt ) );
	}
	pt[ptLen] = '\0';
	if ( strcmp( (const char *)pt, expectedPt ) != 0 ) {
		Com_Error( ERR_FATAL,
			"rcon_autoset selftest: plaintext mismatch (got %s, want %s)",
			(const char *)pt, expectedPt );
	}

	// Tamper MAC — decrypt must fail.
	blob[blobLen - 1] ^= 0x01;
	ptLen = rconsetDecrypt( (const byte *)token, (int)strlen( token ),
	                        blob, blobLen, pt, sizeof( pt ) - 1 );
	if ( ptLen >= 0 ) {
		Com_Error( ERR_FATAL, "rcon_autoset selftest: tampered MAC accepted" );
	}

	Com_DPrintf( "rcon_autoset selftest passed\n" );
}
