#ifndef NET_P2P_STUN_CODEC_H
#define NET_P2P_STUN_CODEC_H

#include "q_shared.h"

#ifndef NET_P2P_STUN_CODEC_STANDALONE
#include "qcommon.h"
#else
typedef enum {
	NA_IP = 2
} netadrtype_t;

typedef struct {
	netadrtype_t type;
	union {
		byte _4[4];
	} ipv;
	uint16_t port;
} netadr_t;
#endif

#define P2P_STUN_MAGIC_COOKIE 0x2112A442u
#define P2P_STUN_BINDING_REQUEST  0x0001u
#define P2P_STUN_BINDING_RESPONSE 0x0101u
#define P2P_STUN_ALLOCATE_REQUEST 0x0003u
#define P2P_STUN_ALLOCATE_SUCCESS 0x0103u
#define P2P_STUN_ERROR_RESPONSE   0x0111u
#define P2P_STUN_REFRESH_REQUEST  0x0004u
#define P2P_STUN_REFRESH_SUCCESS  0x0104u
#define P2P_STUN_CREATE_PERMISSION_REQUEST 0x0008u
#define P2P_STUN_CREATE_PERMISSION_SUCCESS 0x0108u
#define P2P_STUN_CHANNEL_BIND_REQUEST 0x0009u
#define P2P_STUN_CHANNEL_BIND_SUCCESS 0x0109u

#define P2P_STUN_ATTR_MAPPED_ADDRESS      0x0001u
#define P2P_STUN_ATTR_XOR_MAPPED_ADDRESS  0x0020u
#define P2P_STUN_ATTR_REALM               0x0014u
#define P2P_STUN_ATTR_NONCE               0x0015u
#define P2P_STUN_ATTR_USERNAME            0x0006u
#define P2P_STUN_ATTR_MESSAGE_INTEGRITY   0x0008u
#define P2P_STUN_ATTR_XOR_RELAYED_ADDRESS 0x0016u
#define P2P_STUN_ATTR_REQUESTED_TRANSPORT 0x0019u
#define P2P_STUN_ATTR_LIFETIME            0x000Du
#define P2P_STUN_ATTR_ERROR_CODE          0x0009u
#define P2P_STUN_ATTR_XOR_PEER_ADDRESS    0x0012u
#define P2P_STUN_ATTR_CHANNEL_NUMBER      0x000Cu

typedef enum {
	P2P_CAND_HOST = 0,
	P2P_CAND_SRFLX,
	P2P_CAND_RELAY,
	P2P_CAND_COUNT
} p2p_candidate_type_t;

typedef struct {
	uint16_t msgType;
	uint16_t msgLen;
	byte transactionId[12];
	qboolean haveMapped;
	netadr_t mappedAdr;
	qboolean haveRelayed;
	netadr_t relayedAdr;
	char realm[64];
	char nonce[128];
	int errorCode;
	char errorReason[64];
	uint32_t lifetime;
} p2p_stun_parse_result_t;

uint16_t NET_P2P_StunRead16( const byte *p );
uint32_t NET_P2P_StunRead32( const byte *p );
void NET_P2P_StunWrite16( byte *p, uint16_t v );
void NET_P2P_StunWrite32( byte *p, uint32_t v );
int NET_P2P_StunPad4( int len );

int NET_P2P_StunBuildBindingRequest( byte *out, int outSize, const byte *transactionId );
int NET_P2P_StunBuildAllocateAttrs( byte *out, int outSize, const char *username, const char *realm, const char *nonce );
int NET_P2P_StunBuildCreatePermissionAttrs( byte *out, int outSize, const netadr_t *peer, const char *username, const char *realm, const char *nonce );
int NET_P2P_StunBuildRefreshAttrs( byte *out, int outSize, uint32_t lifetimeSec, const char *username, const char *realm, const char *nonce );
int NET_P2P_StunBuildChannelBindAttrs( byte *out, int outSize, uint16_t channelNumber, const netadr_t *peer, const char *username, const char *realm, const char *nonce );

qboolean NET_P2P_StunParseMappedAddress( const byte *value, int valueLen, netadr_t *out, qboolean xored, const byte *transactionId );
qboolean NET_P2P_StunParseMessage( const byte *data, int len, const byte *expectedTransactionId, p2p_stun_parse_result_t *result );

int NET_P2P_StunCandidatePriority( p2p_candidate_type_t local, p2p_candidate_type_t remote );

#endif /* NET_P2P_STUN_CODEC_H */
