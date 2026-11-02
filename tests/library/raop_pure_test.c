/* I892c1: RAOP pure part (plugins/shared/raop): ALAC packer bytes, AES packet round trip, RSA-OAEP key wrap, SDP text. */
#include "raop/raop.h"

#include <stdio.h>
#include <string.h>

#include "mbedtls/aes.h"
#include "mbedtls/rsa.h"

static int failures;

#define CHECK(c)                                                \
    do {                                                        \
        if (!(c)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
            failures++;                                         \
        }                                                       \
    } while (0)

/* Deterministic test RNG (xorshift32). Not for production: the real caller supplies a mbedtls DRBG. */
static int test_rng(void *ctx, unsigned char *out, size_t len) {
    uint32_t *s = ctx;
    size_t i;
    for (i = 0; i < len; i++) {
        *s ^= *s << 13;
        *s ^= *s >> 17;
        *s ^= *s << 5;
        out[i] = (unsigned char)(*s >> 11);
    }
    return 0;
}

static void test_packer(void) {
    static int16_t pcm[RAOP_FRAMES * 2], vary[RAOP_FRAMES * 2];
    static uint8_t out[RAOP_ALAC_BYTES], ref[RAOP_ALAC_BYTES];
    static const int hdr[23] = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    size_t i, b = 0, k;

    for (i = 0; i < RAOP_FRAMES; i++) {
        pcm[2 * i] = 0x1234;
        pcm[2 * i + 1] = (int16_t)0xABCD;
    }
    CHECK(raop_alac_pack(pcm, out) == RAOP_ALAC_BYTES);
    /* Hand-derived: 23 header bits (001 0000 000000000000 0 00 1), then 0x1234 0xABCD ..., then end tag 111. */
    CHECK(out[0] == 0x20 && out[1] == 0x00 && out[2] == 0x02 && out[3] == 0x24 && out[4] == 0x69);
    CHECK(out[RAOP_ALAC_BYTES - 2] == 0x9B && out[RAOP_ALAC_BYTES - 1] == 0xC0);

    /* Independent check: a naive one-bit-at-a-time stream with varying samples. */
    for (i = 0; i < RAOP_FRAMES * 2; i++) vary[i] = (int16_t)((uint32_t)i * 2654435761u >> 7);
    raop_alac_pack(vary, out);
    memset(ref, 0, sizeof ref);
    for (k = 0; k < 23; k++, b++)
        if (hdr[k]) ref[b >> 3] |= (uint8_t)(0x80 >> (b & 7));
    for (i = 0; i < RAOP_FRAMES * 2; i++)
        for (k = 0; k < 16; k++, b++)
            if (((uint16_t)vary[i] >> (15 - k)) & 1) ref[b >> 3] |= (uint8_t)(0x80 >> (b & 7));
    for (k = 0; k < 3; k++, b++) ref[b >> 3] |= (uint8_t)(0x80 >> (b & 7));
    CHECK(b == 11290);
    CHECK(memcmp(out, ref, RAOP_ALAC_BYTES) == 0);
}

static void test_aes(void) {
    /* NIST SP 800-38A F.2.1, first block, followed by 5 clear tail bytes. */
    static const uint8_t key[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    static const uint8_t iv[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    static const uint8_t pt[21] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e,
                                   0x11, 0x73, 0x93, 0x17, 0x2a, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
    static const uint8_t ct0[16] = {0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d};
    static uint8_t big_pt[RAOP_ALAC_BYTES], big_enc[RAOP_ALAC_BYTES], big_dec[RAOP_ALAC_BYTES];
    uint8_t enc[21], dec[21], ivc[16];
    mbedtls_aes_context ctx;
    const size_t whole = RAOP_ALAC_BYTES & ~(size_t)15;
    size_t i;

    CHECK(raop_aes_encrypt_packet(key, iv, pt, sizeof pt, enc) == 0);
    CHECK(memcmp(enc, ct0, 16) == 0);
    CHECK(memcmp(enc + 16, pt + 16, 5) == 0); /* trailing bytes clear */

    /* Round trip of a full packet through mbedtls decrypt; the caller's IV is not consumed. */
    for (i = 0; i < sizeof big_pt; i++) big_pt[i] = (uint8_t)(i * 7 + 3);
    CHECK(raop_aes_encrypt_packet(key, iv, big_pt, sizeof big_pt, big_enc) == 0);
    CHECK(iv[0] == 0 && iv[15] == 15);
    CHECK(memcmp(big_enc, big_pt, 16) != 0);
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, 128);
    memcpy(ivc, iv, 16);
    CHECK(mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, whole, ivc, big_enc, big_dec) == 0);
    mbedtls_aes_free(&ctx);
    CHECK(RAOP_ALAC_BYTES - whole == 4); /* the clear tail is really exercised */
    CHECK(memcmp(big_enc + whole, big_pt + whole, 4) == 0);
    memcpy(big_dec + whole, big_enc + whole, 4);
    CHECK(memcmp(big_dec, big_pt, sizeof big_pt) == 0);

    /* In place gives the same bytes. */
    memcpy(dec, pt, sizeof pt);
    CHECK(raop_aes_encrypt_packet(key, iv, dec, sizeof dec, dec) == 0);
    CHECK(memcmp(dec, enc, sizeof enc) == 0);
}

static void test_rsa(void) {
    raop_pubkey air, test;
    mbedtls_rsa_context rsa;
    uint32_t seed = 0x1badf00du;
    static const uint8_t key[16] = {0xde, 0xad, 0xbe, 0xef, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    uint8_t wrapped[RAOP_RSA_BYTES], plain[RAOP_RSA_BYTES], wrapped2[RAOP_RSA_BYTES];
    size_t plen = 0;

    /* The AirPort Express key decodes to a 2048-bit modulus (top bit set, odd) and e = 65537, and wraps. */
    CHECK(raop_airport_pubkey(&air) == 0);
    CHECK(air.nlen == RAOP_RSA_BYTES && (air.n[0] & 0x80) && (air.n[RAOP_RSA_BYTES - 1] & 1));
    CHECK(air.elen == 3 && air.e[0] == 1 && air.e[1] == 0 && air.e[2] == 1);
    CHECK(raop_rsa_wrap_key(&air, key, test_rng, &seed, wrapped) == 0);

    /* Decryption proof needs a private key, and the AirPort private key is not in the tree: generate a 2048-bit test key,
     * wrap with its public half through the same code path, decrypt with mbedtls (OAEP, SHA-1). */
    mbedtls_rsa_init(&rsa);
    CHECK(mbedtls_rsa_gen_key(&rsa, test_rng, &seed, 2048, 65537) == 0);
    CHECK(mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1) == 0);
    memset(&test, 0, sizeof test);
    test.nlen = RAOP_RSA_BYTES;
    test.elen = 3;
    test.e[0] = 1;
    test.e[2] = 1;
    CHECK(mbedtls_rsa_export_raw(&rsa, test.n, RAOP_RSA_BYTES, NULL, 0, NULL, 0, NULL, 0, NULL, 0) == 0);
    CHECK(raop_rsa_wrap_key(&test, key, test_rng, &seed, wrapped) == 0);
    CHECK(mbedtls_rsa_pkcs1_decrypt(&rsa, test_rng, &seed, &plen, wrapped, plain, sizeof plain) == 0);
    CHECK(plen == 16 && memcmp(plain, key, 16) == 0);
    /* OAEP is randomised: a second wrap differs. */
    CHECK(raop_rsa_wrap_key(&test, key, test_rng, &seed, wrapped2) == 0);
    CHECK(memcmp(wrapped, wrapped2, sizeof wrapped) != 0);
    mbedtls_rsa_free(&rsa);
}

static void test_sdp(void) {
    uint8_t wrapped[RAOP_RSA_BYTES];
    static const uint8_t iv[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    char sdp[RAOP_SDP_MAX], head[RAOP_SDP_MAX], a342[343];
    int n;

    memset(wrapped, 0, sizeof wrapped); /* 256 zero bytes: 342 'A' once the padding is stripped */
    memset(a342, 'A', 342);
    a342[342] = '\0';
    n = raop_sdp_build(sdp, sizeof sdp, 1234567890u, "192.168.1.10", "192.168.1.20", wrapped, iv);
    snprintf(head, sizeof head,
             "v=0\r\no=iTunes 1234567890 0 IN IP4 192.168.1.10\r\ns=iTunes\r\nc=IN IP4 192.168.1.20\r\nt=0 0\r\n"
             "m=audio 0 RTP/AVP 96\r\na=rtpmap:96 AppleLossless\r\na=fmtp:96 352 0 16 40 10 14 2 255 0 0 44100\r\n"
             "a=rsaaeskey:%s\r\na=aesiv:AAECAwQFBgcICQoLDA0ODw\r\n",
             a342);
    CHECK(n == (int)strlen(head));
    CHECK(strcmp(sdp, head) == 0);
    CHECK(raop_sdp_build(sdp, 100, 1u, "a", "b", wrapped, iv) == -1); /* too small */
}

int main(void) {
    test_packer();
    test_aes();
    test_rsa();
    test_sdp();
    if (failures) {
        printf("raop_pure FAILED (%d)\n", failures);
        return 1;
    }
    printf("raop_pure_ok\n");
    return 0;
}
