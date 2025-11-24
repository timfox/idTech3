/*
===========================================================================
Copyright (C) 2024 id Tech 3

This file provides OpenSSL integration for cryptography support.
It wraps OpenSSL functions with engine-style APIs.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// CVar to control OpenSSL usage
static cvar_t *com_openssl_enabled;

/*
=================
OpenSSL_Init
=================
Initialize OpenSSL subsystem
=================
*/
void OpenSSL_Init(void)
{
	com_openssl_enabled = Cvar_Get("com_openssl_enabled", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(com_openssl_enabled, "Enable OpenSSL cryptography support (1 = enabled, 0 = disabled)");
	
	if (com_openssl_enabled->integer) {
		// Initialize OpenSSL
		OpenSSL_add_all_algorithms();
		SSL_library_init();
		SSL_load_error_strings();
	}
}

/*
=================
OpenSSL_Shutdown
=================
Shutdown OpenSSL subsystem
=================
*/
void OpenSSL_Shutdown(void)
{
	if (com_openssl_enabled && com_openssl_enabled->integer) {
		EVP_cleanup();
	}
}

/*
=================
OpenSSL_RandomBytes
=================
Generate cryptographically secure random bytes
Returns qtrue on success, qfalse on failure
=================
*/
qboolean OpenSSL_RandomBytes(byte *output, int len)
{
	if (!output || len <= 0)
		return qfalse;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return qfalse;
	
	return (RAND_bytes(output, len) == 1) ? qtrue : qfalse;
}

/*
=================
OpenSSL_SHA256
=================
Compute SHA-256 hash
Returns qtrue on success, qfalse on failure
=================
*/
qboolean OpenSSL_SHA256(const byte *input, int inputLen, byte *output)
{
	unsigned int mdLen;
	
	if (!input || !output || inputLen <= 0)
		return qfalse;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return qfalse;
	
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (!mdctx)
		return qfalse;
	
	if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	if (EVP_DigestUpdate(mdctx, input, inputLen) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	if (EVP_DigestFinal_ex(mdctx, output, &mdLen) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	EVP_MD_CTX_free(mdctx);
	return qtrue;
}

/*
=================
OpenSSL_SHA1
=================
Compute SHA-1 hash
Returns qtrue on success, qfalse on failure
=================
*/
qboolean OpenSSL_SHA1(const byte *input, int inputLen, byte *output)
{
	unsigned int mdLen;
	
	if (!input || !output || inputLen <= 0)
		return qfalse;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return qfalse;
	
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (!mdctx)
		return qfalse;
	
	if (EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	if (EVP_DigestUpdate(mdctx, input, inputLen) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	if (EVP_DigestFinal_ex(mdctx, output, &mdLen) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	EVP_MD_CTX_free(mdctx);
	return qtrue;
}

/*
=================
OpenSSL_MD5
=================
Compute MD5 hash
Returns qtrue on success, qfalse on failure
=================
*/
qboolean OpenSSL_MD5(const byte *input, int inputLen, byte *output)
{
	unsigned int mdLen;
	
	if (!input || !output || inputLen <= 0)
		return qfalse;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return qfalse;
	
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (!mdctx)
		return qfalse;
	
	if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	if (EVP_DigestUpdate(mdctx, input, inputLen) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	if (EVP_DigestFinal_ex(mdctx, output, &mdLen) != 1) {
		EVP_MD_CTX_free(mdctx);
		return qfalse;
	}
	
	EVP_MD_CTX_free(mdctx);
	return qtrue;
}

/*
=================
OpenSSL_AES256_Encrypt
=================
Encrypt data using AES-256-CBC
Returns encrypted length on success, -1 on failure
=================
*/
int OpenSSL_AES256_Encrypt(const byte *key, const byte *iv, const byte *input, int inputLen, byte *output, int outputMaxLen)
{
	EVP_CIPHER_CTX *ctx;
	int len;
	int ciphertextLen;
	
	if (!key || !iv || !input || !output || inputLen <= 0 || outputMaxLen <= 0)
		return -1;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return -1;
	
	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return -1;
	
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return -1;
	}
	
	if (EVP_EncryptUpdate(ctx, output, &len, input, inputLen) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return -1;
	}
	ciphertextLen = len;
	
	if (EVP_EncryptFinal_ex(ctx, output + len, &len) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return -1;
	}
	ciphertextLen += len;
	
	EVP_CIPHER_CTX_free(ctx);
	return ciphertextLen;
}

/*
=================
OpenSSL_AES256_Decrypt
=================
Decrypt data using AES-256-CBC
Returns decrypted length on success, -1 on failure
=================
*/
int OpenSSL_AES256_Decrypt(const byte *key, const byte *iv, const byte *input, int inputLen, byte *output, int outputMaxLen)
{
	EVP_CIPHER_CTX *ctx;
	int len;
	int plaintextLen;
	
	if (!key || !iv || !input || !output || inputLen <= 0 || outputMaxLen <= 0)
		return -1;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return -1;
	
	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return -1;
	
	if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return -1;
	}
	
	if (EVP_DecryptUpdate(ctx, output, &len, input, inputLen) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return -1;
	}
	plaintextLen = len;
	
	if (EVP_DecryptFinal_ex(ctx, output + len, &len) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return -1;
	}
	plaintextLen += len;
	
	EVP_CIPHER_CTX_free(ctx);
	return plaintextLen;
}

/*
=================
OpenSSL_Base64_Encode
=================
Encode data to Base64
Returns encoded length on success, -1 on failure
=================
*/
int OpenSSL_Base64_Encode(const byte *input, int inputLen, char *output, int outputMaxLen)
{
	BIO *bio, *b64;
	BUF_MEM *bufferPtr;
	int len;
	
	if (!input || !output || inputLen <= 0 || outputMaxLen <= 0)
		return -1;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return -1;
	
	b64 = BIO_new(BIO_f_base64());
	if (!b64)
		return -1;
	
	bio = BIO_new(BIO_s_mem());
	if (!bio) {
		BIO_free_all(b64);
		return -1;
	}
	
	bio = BIO_push(b64, bio);
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	
	if (BIO_write(bio, input, inputLen) != inputLen) {
		BIO_free_all(bio);
		return -1;
	}
	
	if (BIO_flush(bio) != 1) {
		BIO_free_all(bio);
		return -1;
	}
	
	BIO_get_mem_ptr(bio, &bufferPtr);
	len = (int)bufferPtr->length;
	
	if (len >= outputMaxLen) {
		BIO_free_all(bio);
		return -1;
	}
	
	Com_Memcpy(output, bufferPtr->data, len);
	output[len] = '\0';
	
	BIO_free_all(bio);
	return len;
}

/*
=================
OpenSSL_Base64_Decode
=================
Decode Base64 data
Returns decoded length on success, -1 on failure
=================
*/
int OpenSSL_Base64_Decode(const char *input, byte *output, int outputMaxLen)
{
	BIO *bio, *b64;
	int len;
	
	if (!input || !output || outputMaxLen <= 0)
		return -1;
	
	if (!com_openssl_enabled || !com_openssl_enabled->integer)
		return -1;
	
	b64 = BIO_new(BIO_f_base64());
	if (!b64)
		return -1;
	
	bio = BIO_new_mem_buf(input, -1);
	if (!bio) {
		BIO_free_all(b64);
		return -1;
	}
	
	bio = BIO_push(b64, bio);
	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	
	len = BIO_read(bio, output, outputMaxLen);
	BIO_free_all(bio);
	
	return len;
}

#endif // USE_OPENSSL

