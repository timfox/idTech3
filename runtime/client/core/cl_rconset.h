// cl_rconset.h -- Engine-side handler for the rcon_autoset server command.
// The hub computes an encryption key from the user's long-lived auth
// token (cl_authToken) + a per-handshake nonce; the collector encrypts
// the local rcon password under that key; the engine (here) decrypts
// and sets the rconPassword cvar so the connected player can rcon the
// server they're admin of without ever typing the password.
//
// Wire format: rcon_autoset <hexblob>
// Decoded blob:
//   byte 0    : version (must be 1)
//   byte 1    : ct_len (1..64)
//   byte 2-17 : epoch_nonce
//   byte 18.. : ciphertext (ct_len bytes)
//   trailing 8 bytes : SipHash MAC over (header || ciphertext) keyed by K
//
// K = SipHash128(token_bytes, "surf-rconset-key-v1" || epoch_nonce)
// ciphertext = plaintext XOR keystream
// keystream block i = SipHash128(K, "surf-rconset-ks-v1" || nonce || u32be(i))
// MAC = SipHash128(K, "surf-rconset-mac-v1" || header || ct)[:8]
//
// Hub encryptors MUST use the same surf-rconset-*-v1 KDF labels (not the
// legacy trinity-rconset-*-v1 strings).

#ifndef CL_RCONSET_H
#define CL_RCONSET_H

// CL_HandleRconAutoset is called from the cl_cgame.c server-command
// dispatch when the server sends "rcon_autoset <hex>". hexArg is the
// single hex-encoded payload argument. Silently drops on any failure
// (token missing, MAC mismatch, version skew, etc.).
void CL_HandleRconAutoset( const char *hexArg );

// CL_RconsetSelfTest encrypts a known plaintext with a known token +
// nonce, then decrypts and Com_Errors if the round-trip fails. Called
// once at client init so a crypto regression refuses to boot.
void CL_RconsetSelfTest( void );

#endif
