/* TLS stream wrapper; see tls.h. mbedTLS is driven over the socket table with
 * deadline-bounded BIO callbacks, so every wait has a timeout like the plain path. */
#include "tls.h"

#include "net.h"

#include <stdlib.h>
#include <string.h>

#ifdef SEL_NO_TLS

int sel_tls_open(int h, const char* host, unsigned timeout_ms) {
    (void)h;
    (void)host;
    (void)timeout_ms;
    return SEL_TLS_E_UNAVAILABLE;
}

unsigned sel_tls_handshakes(void) {
    return 0;
}

int sel_stream_send(int h, const void* data, size_t len, unsigned timeout_ms) {
    return sel_net_send(h, data, len, timeout_ms);
}

int sel_stream_recv(int h, void* buf, size_t cap, unsigned timeout_ms) {
    return sel_net_recv(h, buf, cap, timeout_ms);
}

void sel_stream_close(int h) {
    sel_net_close(h);
}

#else

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

typedef struct tls_slot {
    int active;
    int timed_out;
    unsigned long deadline;
    mbedtls_ssl_context ssl;
} tls_slot;

static tls_slot g_slot[SEL_NET_MAX_CONN];
static unsigned g_handshakes;
static int g_init_state; /* 0 not tried, 1 ready, -1 failed */
static mbedtls_ssl_config g_conf;
static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_drbg;
static mbedtls_x509_crt g_ca;

static unsigned remaining_ms(unsigned long deadline) {
    unsigned long now = sel_net_now_ms();
    return now >= deadline ? 0u : (unsigned)(deadline - now);
}

static void load_trust(void) {
    const char* file = sel_net_ca_file();
    mbedtls_x509_crt_init(&g_ca);
    if (file != NULL) {
        (void)mbedtls_x509_crt_parse_file(&g_ca, file);
    } else {
        size_t len = 0;
        char* pem = sel_net_os_ca_bundle(&len);
        if (pem != NULL) {
            /* A few OS roots use algorithms mbedTLS skips; the rest still load. */
            (void)mbedtls_x509_crt_parse(&g_ca, (const unsigned char*)pem, len + 1);
            free(pem);
        }
    }
}

static int tls_init(void) {
    if (g_init_state != 0) {
        return g_init_state > 0 ? 0 : -1;
    }
    g_init_state = -1;
    mbedtls_ssl_config_init(&g_conf);
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_drbg);
    if (psa_crypto_init() != PSA_SUCCESS ||
        mbedtls_ctr_drbg_seed(&g_drbg, mbedtls_entropy_func, &g_entropy, (const unsigned char*)"selecta", 7) != 0 ||
        mbedtls_ssl_config_defaults(&g_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        return -1;
    }
    load_trust();
    mbedtls_ssl_conf_rng(&g_conf, mbedtls_ctr_drbg_random, &g_drbg);
    mbedtls_ssl_conf_authmode(&g_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&g_conf, &g_ca, NULL);
    g_init_state = 1;
    return 0;
}

/* BIO callbacks: ctx is the slot; the socket handle is the slot index. */
static int bio_send(void* ctx, const unsigned char* buf, size_t len) {
    tls_slot* s = ctx;
    int rc = sel_net_send((int)(s - g_slot), buf, len, remaining_ms(s->deadline));
    if (rc == 0) {
        return (int)len;
    }
    return rc == SEL_NET_E_TIMEOUT ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int bio_recv(void* ctx, unsigned char* buf, size_t len) {
    tls_slot* s = ctx;
    int n = sel_net_recv((int)(s - g_slot), buf, len, remaining_ms(s->deadline));
    if (n > 0) {
        return n;
    }
    if (n == SEL_NET_E_TIMEOUT) {
        s->timed_out = 1;
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return n == SEL_NET_E_CLOSED ? 0 : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static void slot_release(tls_slot* s) {
    if (s->active) {
        mbedtls_ssl_free(&s->ssl);
        s->active = 0;
    }
}

int sel_tls_open(int h, const char* host, unsigned timeout_ms) {
    tls_slot* s;
    int rc;
    if (h < 0 || h >= SEL_NET_MAX_CONN || tls_init() != 0) {
        return SEL_TLS_E_INIT;
    }
    s = &g_slot[h];
    slot_release(s);
    mbedtls_ssl_init(&s->ssl);
    s->active = 1;
    s->timed_out = 0;
    s->deadline = sel_net_now_ms() + timeout_ms;
    if (mbedtls_ssl_setup(&s->ssl, &g_conf) != 0 || mbedtls_ssl_set_hostname(&s->ssl, host) != 0) {
        slot_release(s);
        return SEL_TLS_E_INIT;
    }
    mbedtls_ssl_set_bio(&s->ssl, s, bio_send, bio_recv, NULL);
    for (;;) {
        rc = mbedtls_ssl_handshake(&s->ssl);
        if (rc == 0) {
            g_handshakes++;
            return SEL_TLS_OK;
        }
        if ((rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) && !s->timed_out &&
            remaining_ms(s->deadline) > 0) {
            continue;
        }
        break;
    }
    if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
        rc = SEL_TLS_E_VERIFY;
    } else if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
        rc = SEL_TLS_E_TIMEOUT;
    } else {
        rc = SEL_TLS_E_HANDSHAKE;
    }
    slot_release(s);
    return rc;
}

unsigned sel_tls_handshakes(void) {
    return g_handshakes;
}

int sel_stream_send(int h, const void* data, size_t len, unsigned timeout_ms) {
    tls_slot* s = h >= 0 && h < SEL_NET_MAX_CONN ? &g_slot[h] : NULL;
    const unsigned char* p = data;
    if (s == NULL || !s->active) {
        return sel_net_send(h, data, len, timeout_ms);
    }
    s->deadline = sel_net_now_ms() + timeout_ms;
    while (len > 0) {
        int rc = mbedtls_ssl_write(&s->ssl, p, len);
        if (rc > 0) {
            p += rc;
            len -= (size_t)rc;
        } else if (rc == MBEDTLS_ERR_SSL_WANT_WRITE || rc == MBEDTLS_ERR_SSL_WANT_READ) {
            if (remaining_ms(s->deadline) == 0) {
                return SEL_NET_E_TIMEOUT;
            }
        } else {
            return SEL_NET_E_IO;
        }
    }
    return 0;
}

int sel_stream_recv(int h, void* buf, size_t cap, unsigned timeout_ms) {
    tls_slot* s = h >= 0 && h < SEL_NET_MAX_CONN ? &g_slot[h] : NULL;
    if (s == NULL || !s->active) {
        return sel_net_recv(h, buf, cap, timeout_ms);
    }
    s->deadline = sel_net_now_ms() + timeout_ms;
    s->timed_out = 0;
    for (;;) {
        int rc = mbedtls_ssl_read(&s->ssl, buf, cap);
        if (rc > 0) {
            return rc;
        }
        if (rc == 0 || rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || rc == MBEDTLS_ERR_SSL_CONN_EOF) {
            return SEL_NET_E_CLOSED;
        }
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
            rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            if (s->timed_out || remaining_ms(s->deadline) == 0) {
                return SEL_NET_E_TIMEOUT;
            }
            continue;
        }
        return SEL_NET_E_IO;
    }
}

void sel_stream_close(int h) {
    if (h >= 0 && h < SEL_NET_MAX_CONN && g_slot[h].active) {
        g_slot[h].deadline = sel_net_now_ms() + 200u;
        (void)mbedtls_ssl_close_notify(&g_slot[h].ssl);
        slot_release(&g_slot[h]);
    }
    sel_net_close(h);
}

#endif
