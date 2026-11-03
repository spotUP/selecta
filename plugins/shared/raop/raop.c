/* RAOP pure part (I892c1); see raop.h. */
#include "raop/raop.h"

#include <stdio.h>
#include <string.h>

#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/rsa.h"

/* The AirPort Express public key every RAOP receiver accepts (modulus and exponent, standard base64). */
#define RAOP_AIRPORT_N_B64                                                                                            \
    "59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUtwC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6" \
    "DZHJ2YCCLlDRKSKv6kDqnw4UwPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf/+sG+NCK3eQJV" \
    "xqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5" \
    "hEew=="
#define RAOP_AIRPORT_E_B64 "AQAB"

/* Appends the low nbits of v, most significant bit first, at bit position *pos of out (zeroed by the caller). */
static void put_bits(uint8_t *out, size_t *pos, uint32_t v, unsigned nbits) {
    while (nbits > 0) {
        unsigned room = 8u - (unsigned)(*pos & 7u);
        unsigned take = nbits < room ? nbits : room;
        unsigned chunk = (v >> (nbits - take)) & ((1u << take) - 1u);
        out[*pos >> 3] = (uint8_t)(out[*pos >> 3] | (chunk << (room - take)));
        *pos += take;
        nbits -= take;
    }
}

size_t raop_alac_pack(const int16_t *pcm, uint8_t *out) {
    /* Escape frame: element tag 1 (3 bits, channel pair), instance 0 (4), unused 0 (12), has-size 0 (1: full frame, the
     * length comes from the fmtp line), unused-bytes 0 (2), not-compressed 1 (1) = 23 bits; then the samples interleaved,
     * 16 bits big endian each; then the end tag 7 (3 bits); zero padding to the byte. */
    size_t pos = 0, i, n = (size_t)RAOP_FRAMES * RAOP_CHANNELS;

    memset(out, 0, RAOP_ALAC_BYTES);
    put_bits(out, &pos, 1, 3);
    put_bits(out, &pos, 0, 4);
    put_bits(out, &pos, 0, 12);
    put_bits(out, &pos, 0, 1);
    put_bits(out, &pos, 0, 2);
    put_bits(out, &pos, 1, 1);
    for (i = 0; i < n; i++) put_bits(out, &pos, (uint16_t)pcm[i], 16);
    put_bits(out, &pos, 7, 3);
    return RAOP_ALAC_BYTES;
}

int raop_aes_encrypt_packet(const uint8_t key[RAOP_AES_KEY_BYTES], const uint8_t iv[RAOP_AES_IV_BYTES], const uint8_t *in,
                            size_t len, uint8_t *out) {
    mbedtls_aes_context ctx;
    uint8_t ivcopy[RAOP_AES_IV_BYTES];
    size_t whole = len & ~(size_t)15;
    int rc = 0;

    if (!key || !iv || !in || !out) return -1;
    mbedtls_aes_init(&ctx);
    memcpy(ivcopy, iv, sizeof ivcopy);
    if (whole > 0 && (mbedtls_aes_setkey_enc(&ctx, key, 128) != 0 ||
                      mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, whole, ivcopy, in, out) != 0))
        rc = -1;
    if (rc == 0 && len > whole) memmove(out + whole, in + whole, len - whole);
    mbedtls_aes_free(&ctx);
    return rc;
}

int raop_airport_pubkey(raop_pubkey *pk) {
    if (!pk) return -1;
    if (mbedtls_base64_decode(pk->n, sizeof pk->n, &pk->nlen, (const unsigned char *)RAOP_AIRPORT_N_B64,
                              strlen(RAOP_AIRPORT_N_B64)) != 0 ||
        pk->nlen != RAOP_RSA_BYTES)
        return -1;
    if (mbedtls_base64_decode(pk->e, sizeof pk->e, &pk->elen, (const unsigned char *)RAOP_AIRPORT_E_B64,
                              strlen(RAOP_AIRPORT_E_B64)) != 0)
        return -1;
    return 0;
}

int raop_rsa_wrap_key(const raop_pubkey *pk, const uint8_t key[RAOP_AES_KEY_BYTES], raop_rng_fn rng, void *rng_ctx,
                      uint8_t out[RAOP_RSA_BYTES]) {
    mbedtls_rsa_context rsa;
    int rc = -1;

    if (!pk || !key || !rng || !out || pk->nlen != RAOP_RSA_BYTES) return -1;
    mbedtls_rsa_init(&rsa);
    if (mbedtls_rsa_import_raw(&rsa, pk->n, pk->nlen, NULL, 0, NULL, 0, NULL, 0, pk->e, pk->elen) == 0 &&
        mbedtls_rsa_complete(&rsa) == 0 && mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1) == 0 &&
        mbedtls_rsa_pkcs1_encrypt(&rsa, rng, rng_ctx, RAOP_AES_KEY_BYTES, key, out) == 0)
        rc = 0;
    mbedtls_rsa_free(&rsa);
    return rc;
}

int raop_base64(const uint8_t *in, size_t len, char *out, size_t cap) {
    size_t olen = 0;

    if (mbedtls_base64_encode((unsigned char *)out, cap, &olen, in, len) != 0) return -1;
    while (olen > 0 && out[olen - 1] == '=') out[--olen] = '\0';
    return (int)olen;
}

int raop_sdp_build(char *out, size_t cap, uint32_t session_id, const char *client_ip, const char *server_ip,
                   const uint8_t wrapped_key[RAOP_RSA_BYTES], const uint8_t iv[RAOP_AES_IV_BYTES]) {
    char key64[RAOP_RSA_BYTES * 2], iv64[40];
    int n;

    if (!out || !client_ip || !server_ip || !wrapped_key || !iv) return -1;
    if (raop_base64(wrapped_key, RAOP_RSA_BYTES, key64, sizeof key64) < 0) return -1;
    if (raop_base64(iv, RAOP_AES_IV_BYTES, iv64, sizeof iv64) < 0) return -1;
    n = snprintf(out, cap,
                 "v=0\r\n"
                 "o=iTunes %lu 0 IN IP4 %s\r\n"
                 "s=iTunes\r\n"
                 "c=IN IP4 %s\r\n"
                 "t=0 0\r\n"
                 "m=audio 0 RTP/AVP 96\r\n"
                 "a=rtpmap:96 AppleLossless\r\n"
                 "a=fmtp:96 352 0 16 40 10 14 2 255 0 0 44100\r\n"
                 "a=rsaaeskey:%s\r\n"
                 "a=aesiv:%s\r\n",
                 (unsigned long)session_id, client_ip, server_ip, key64, iv64);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}
