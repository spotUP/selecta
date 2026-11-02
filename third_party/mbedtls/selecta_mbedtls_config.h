/* Selecta mbedTLS configuration (plan 2026-10-25-m3-library.md, D-M3-4): the upstream default
 * configuration for a TLS 1.2 + 1.3 client, minus everything a client does not use. The
 * upstream tree in this directory is unmodified; only this wrapper differs. */
#include "mbedtls/mbedtls_config.h"

/* Own socket table (src/core/<os>/net.c), no mbedTLS net layer, timing or DTLS. */
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_SSL_PROTO_DTLS
#undef MBEDTLS_SSL_DTLS_ANTI_REPLAY
#undef MBEDTLS_SSL_DTLS_HELLO_VERIFY
#undef MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE
#undef MBEDTLS_SSL_DTLS_CONNECTION_ID
#undef MBEDTLS_SSL_DTLS_SRTP
#undef MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT
#undef MBEDTLS_SSL_COOKIE_C
/* Client only. */
#undef MBEDTLS_SSL_SRV_C
#undef MBEDTLS_SSL_CACHE_C
#undef MBEDTLS_SSL_TICKET_C
#undef MBEDTLS_SSL_SESSION_TICKETS
/* No persistent PSA key storage on disk. */
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C
/* No self test output. */
#undef MBEDTLS_SELF_TEST

/* Amiga (I1004): no /dev/urandom or Win32 entropy source. The RAOP code takes its RNG from the caller (raop_rng_fn) and
 * links only aes, base64 and rsa out of this static library, so the platform entropy poll is declared absent; the TLS
 * client, which would need entropy, is I899 and brings its own mbedtls_hardware_poll. */
#if defined(__amigaos__)
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#endif
