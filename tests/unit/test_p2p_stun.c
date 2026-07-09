/*
 * Unit tests: STUN/TURN codec helpers for P2P ICE-lite.
 */
#include <stdio.h>
#include <string.h>

#include "net_p2p_stun_codec.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_build_binding_request(void) {
	byte packet[32];
	byte tid[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
	int len;

	len = NET_P2P_StunBuildBindingRequest( packet, sizeof( packet ), tid );
	ASSERT( len == 20, "binding request length" );
	ASSERT( NET_P2P_StunRead16( packet + 0 ) == P2P_STUN_BINDING_REQUEST, "binding request type" );
	ASSERT( NET_P2P_StunRead32( packet + 4 ) == P2P_STUN_MAGIC_COOKIE, "magic cookie" );
	ASSERT( memcmp( packet + 8, tid, 12 ) == 0, "transaction id" );
	return 0;
}

static int test_parse_xor_mapped_address(void) {
	byte response[32];
	byte tid[12] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
	p2p_stun_parse_result_t result;
	netadr_t expected;
	int pos = 20;
	uint16_t port = 54321;
	uint32_t ipv4 = 0x7f000001u;

	NET_P2P_StunWrite16( response + 0, P2P_STUN_BINDING_RESPONSE );
	NET_P2P_StunWrite16( response + 2, 12 );
	NET_P2P_StunWrite32( response + 4, P2P_STUN_MAGIC_COOKIE );
	Com_Memcpy( response + 8, tid, 12 );

	NET_P2P_StunWrite16( response + pos + 0, P2P_STUN_ATTR_XOR_MAPPED_ADDRESS );
	NET_P2P_StunWrite16( response + pos + 2, 8 );
	response[pos + 4] = 0;
	response[pos + 5] = 0x01;
	NET_P2P_StunWrite16( response + pos + 6, port ^ (uint16_t)( P2P_STUN_MAGIC_COOKIE >> 16 ) );
	NET_P2P_StunWrite32( response + pos + 8, ipv4 ^ P2P_STUN_MAGIC_COOKIE ^ NET_P2P_StunRead32( tid ) );

	ASSERT( NET_P2P_StunParseMessage( response, pos + 12, tid, &result ) == qtrue, "parse binding response" );
	ASSERT( result.haveMapped == qtrue, "mapped present" );
	ASSERT( result.mappedAdr.type == NA_IP, "mapped type" );
	ASSERT( result.mappedAdr.ipv._4[0] == 127 && result.mappedAdr.ipv._4[3] == 1, "mapped ip" );
	ASSERT( result.mappedAdr.port == port, "mapped port" );

	Com_Memset( &expected, 0, sizeof( expected ) );
	ASSERT( NET_P2P_StunParseMappedAddress( response + pos + 4, 8, &expected, qtrue, tid ) == qtrue, "direct mapped parse" );
	ASSERT( expected.port == port, "direct mapped port" );
	return 0;
}

static int NET_P2P_StunAppendStringAttr( byte *out, int outSize, int pos, uint16_t attrType, const char *value )
{
	int ulen = (int)strlen( value );
	if ( pos + 4 + NET_P2P_StunPad4( ulen ) > outSize ) {
		return -1;
	}
	NET_P2P_StunWrite16( out + pos + 0, attrType );
	NET_P2P_StunWrite16( out + pos + 2, (uint16_t)ulen );
	Com_Memcpy( out + pos + 4, value, ulen );
	return pos + 4 + NET_P2P_StunPad4( ulen );
}

static int test_parse_error_realm_nonce(void) {
	byte response[96];
	byte tid[12] = { 0 };
	p2p_stun_parse_result_t result;
	int pos = 20;

	NET_P2P_StunWrite16( response + 0, P2P_STUN_ERROR_RESPONSE );
	NET_P2P_StunWrite16( response + 2, 0 );
	NET_P2P_StunWrite32( response + 4, P2P_STUN_MAGIC_COOKIE );

	pos = NET_P2P_StunAppendStringAttr( response, sizeof( response ), pos, P2P_STUN_ATTR_REALM, "example.org" );
	ASSERT( pos > 0, "append realm" );
	pos = NET_P2P_StunAppendStringAttr( response, sizeof( response ), pos, P2P_STUN_ATTR_NONCE, "nonce123" );
	ASSERT( pos > 0, "append nonce" );
	NET_P2P_StunWrite16( response + 2, (uint16_t)( pos - 20 ) );

	ASSERT( NET_P2P_StunParseMessage( response, pos, NULL, &result ) == qtrue, "parse error response" );
	ASSERT( strcmp( result.realm, "example.org" ) == 0, "realm value" );
	ASSERT( strcmp( result.nonce, "nonce123" ) == 0, "nonce value" );
	return 0;
}

static int test_allocate_attrs_shape(void) {
	byte attrs[256];
	int len;

	len = NET_P2P_StunBuildAllocateAttrs( attrs, sizeof( attrs ), "user", "realm", "nonce" );
	ASSERT( len > 8, "allocate attrs length" );
	ASSERT( NET_P2P_StunRead16( attrs + 0 ) == P2P_STUN_ATTR_REQUESTED_TRANSPORT, "transport attr" );
	return 0;
}

static int test_candidate_priority(void) {
	ASSERT( NET_P2P_StunCandidatePriority( P2P_CAND_SRFLX, P2P_CAND_SRFLX ) >
	        NET_P2P_StunCandidatePriority( P2P_CAND_HOST, P2P_CAND_HOST ), "srflx beats host" );
	ASSERT( NET_P2P_StunCandidatePriority( P2P_CAND_RELAY, P2P_CAND_SRFLX ) >
	        NET_P2P_StunCandidatePriority( P2P_CAND_SRFLX, P2P_CAND_HOST ), "relay beats srflx-host" );
	return 0;
}

int main(void) {
	if ( test_build_binding_request() ) return 1;
	if ( test_parse_xor_mapped_address() ) return 1;
	if ( test_parse_error_realm_nonce() ) return 1;
	if ( test_allocate_attrs_shape() ) return 1;
	if ( test_candidate_priority() ) return 1;
	printf("unit_p2p_stun: ok\n");
	return 0;
}
