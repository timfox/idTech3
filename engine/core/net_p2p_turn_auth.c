/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

TURN long-term credential HMAC-SHA1 (RFC 5389) — decoupled from USE_DTLS.
===========================================================================
*/

#include "net_p2p_turn_auth.h"
#include "net_p2p_stun_codec.h"

#include <string.h>

#if defined(USE_DTLS)
#include <openssl/evp.h>
#include <openssl/hmac.h>
#define P2P_TURN_HAVE_OPENSSL 1
#else
#define P2P_TURN_HAVE_OPENSSL 0
#endif

qboolean NET_P2P_TurnAuthAvailable( void )
{
#if P2P_TURN_HAVE_OPENSSL
	return qtrue;
#else
	return qfalse;
#endif
}

int NET_P2P_TurnAppendMessageIntegrity( byte *packet, int len, int maxLen, const char *username, const char *password, const char *realm, const char *nonce )
{
#if P2P_TURN_HAVE_OPENSSL
	unsigned char key[16];
	unsigned char hmac[20];
	unsigned int hmacLen = 0;
	EVP_MD_CTX *mdctx;
	const EVP_MD *md5;

	if ( !packet || len < 20 || maxLen < len + 24 || !username || !password || !realm || !nonce ) {
		return 0;
	}

	mdctx = EVP_MD_CTX_new();
	md5 = EVP_md5();
	if ( !mdctx || !md5 ||
	     EVP_DigestInit_ex( mdctx, md5, NULL ) != 1 ||
	     EVP_DigestUpdate( mdctx, username, strlen( username ) ) != 1 ||
	     EVP_DigestUpdate( mdctx, ":", 1 ) != 1 ||
	     EVP_DigestUpdate( mdctx, realm, strlen( realm ) ) != 1 ||
	     EVP_DigestUpdate( mdctx, ":", 1 ) != 1 ||
	     EVP_DigestUpdate( mdctx, password, strlen( password ) ) != 1 ||
	     EVP_DigestFinal_ex( mdctx, key, NULL ) != 1 ) {
		if ( mdctx ) {
			EVP_MD_CTX_free( mdctx );
		}
		return 0;
	}
	EVP_MD_CTX_free( mdctx );

	NET_P2P_StunWrite16( packet + 2, (uint16_t)( len - 20 ) );
	HMAC( EVP_sha1(), key, sizeof( key ), packet, (size_t)len, hmac, &hmacLen );

	NET_P2P_StunWrite16( packet + len + 0, P2P_STUN_ATTR_MESSAGE_INTEGRITY );
	NET_P2P_StunWrite16( packet + len + 2, 20 );
	Com_Memcpy( packet + len + 4, hmac, 20 );
	len += 24;
	NET_P2P_StunWrite16( packet + 2, (uint16_t)( len - 20 ) );
	return len;
#else
	(void)packet;
	(void)len;
	(void)maxLen;
	(void)username;
	(void)password;
	(void)realm;
	(void)nonce;
	return 0;
#endif
}
