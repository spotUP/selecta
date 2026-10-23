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
