/* RAOP (AirPlay 1) sender, pure part (I892c1). No sockets, no OS calls, no float, no allocation: every function works on
 * caller buffers so the Amiga output plugin (I895) and the host sender core (I892c2) share one copy.
 *   packer  uncompressed ALAC frame (escape frame: header + raw PCM) of 352 frames, 16-bit stereo
 *   aes     AES-128-CBC per packet, fixed IV per packet, whole 16-byte blocks only, trailing bytes clear
 *   rsa     RSA-OAEP (SHA-1) wrap of the session AES key with the AirPort Express public key
 *   sdp     ANNOUNCE body: rtpmap/fmtp, rsaaeskey, aesiv
 * Crypto comes from third_party/mbedtls only. Design: thoughts/shared/research/2026-11-01-airplay-chromecast.md, section 1. */
#ifndef SELECTA_RAOP_H
#define SELECTA_RAOP_H

#include <stddef.h>
#include <stdint.h>

#define RAOP_FRAMES 352 /* frames per RTP packet */
#define RAOP_CHANNELS 2
#define RAOP_ALAC_BYTES 1412 /* 23 header bits + 352*2*16 sample bits + 3 end-tag bits, padded to a byte */
#define RAOP_AES_KEY_BYTES 16
#define RAOP_AES_IV_BYTES 16
#define RAOP_RSA_BYTES 256 /* RSA-2048 modulus and OAEP output */
#define RAOP_SDP_MAX 1024

/* Fills out[RAOP_ALAC_BYTES] from pcm[RAOP_FRAMES * 2] (interleaved left, right; host int16). Always returns RAOP_ALAC_BYTES. */
size_t raop_alac_pack(const int16_t *pcm, uint8_t *out);

/* Encrypts in[0..len) to out[0..len) (in == out allowed): floor(len / 16) blocks AES-128-CBC with a fresh copy of iv, the
 * remaining len % 16 bytes copied clear. Returns 0, or -1 on bad arguments. */
int raop_aes_encrypt_packet(const uint8_t key[RAOP_AES_KEY_BYTES], const uint8_t iv[RAOP_AES_IV_BYTES], const uint8_t *in,
                            size_t len, uint8_t *out);

typedef struct raop_pubkey {
    uint8_t n[RAOP_RSA_BYTES];
    size_t nlen;
    uint8_t e[4];
    size_t elen;
} raop_pubkey;

/* Decodes the well-known AirPort Express public key. Returns 0 or -1. */
int raop_airport_pubkey(raop_pubkey *pk);

typedef int (*raop_rng_fn)(void *ctx, unsigned char *out, size_t len);

/* RSA-OAEP (SHA-1, as RAOP requires) of key under pk into out[RAOP_RSA_BYTES]. rng supplies the OAEP seed (caller owns the
 * entropy source, so this stays pure). Returns 0 or -1. */
int raop_rsa_wrap_key(const raop_pubkey *pk, const uint8_t key[RAOP_AES_KEY_BYTES], raop_rng_fn rng, void *rng_ctx,
                      uint8_t out[RAOP_RSA_BYTES]);

/* Unpadded base64 (RAOP convention) of in[0..len) into out (NUL-terminated); returns the text length, or -1 when cap is
 * too small. */
int raop_base64(const uint8_t *in, size_t len, char *out, size_t cap);

/* Builds the ANNOUNCE SDP body into out (NUL-terminated, CRLF lines). wrapped_key is the raop_rsa_wrap_key output.
 * Returns the length without the NUL, or -1 when cap is too small. */
int raop_sdp_build(char *out, size_t cap, uint32_t session_id, const char *client_ip, const char *server_ip,
                   const uint8_t wrapped_key[RAOP_RSA_BYTES], const uint8_t iv[RAOP_AES_IV_BYTES]);

#endif
