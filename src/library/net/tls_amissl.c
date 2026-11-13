/* TLS over AmiSSL 5 (OpenSSL 3 API behind amisslmaster.library), Amiga builds only (I899g). The library is the user's
 * install (AmiSSL v5), opened at runtime: when it is missing or older than v5 sel_amissl_probe answers 1 (the full sel_amissl_open runs on the first connect), sel_tls_available
 * (tls_gate.c) turns 0 and https mirrors are skipped with a one-line hint. Open/init sequence follows
 * vtcon's claude/tls_amissl.c (OpenAmiSSLTags, AmiSSL_ErrNoPtr, SSL_CTX_set_default_verify_paths against AmiSSL:Certs).
 *
 * Verification is always on: SSL_VERIFY_PEER, the chain against AmiSSL:Certs (or SELECTA_CA_FILE), the host name with
 * SSL_set1_host, SNI, TLS 1.2 or later. There is no unverified mode on the Amiga: sel_tls_open_insecure answers
 * SEL_TLS_E_UNAVAILABLE.
 *
 * The OpenSSL engine reads and writes through a BIO over the socket table (sel_net_send/recv), so every network wait
 * has a deadline like the plain path. A handshake step is minutes of CPU on a 68020 (measured by vtcon: ~12 s per step),
 * so during the handshake the deadline restarts after each completed BIO call: it bounds a wait for the peer, not the
 * computing. Randomness: the OpenSSL pool is fed from src/library/entropy.c before every handshake and RAND_status must
 * report seeded; a handshake never starts on an unseeded pool. */
#include "tls.h"

#include "net.h"
#include "../entropy.h"

#include <exec/types.h>
#include <proto/exec.h>
#include <utility/tagitem.h>
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>

#include <errno.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

struct Library* sel_net_amiga_socketbase(void); /* core/amiga/net.c */

struct Library* AmiSSLMasterBase;
struct Library* AmiSSLBase;
struct Library* AmiSSLExtBase;

typedef struct tls_slot {
    SSL* ssl;
    int timed_out;
    unsigned timeout_ms; /* handshake: the per-wait bound that restarts after each BIO call */
    unsigned long deadline;
    int handshaking;
} tls_slot;

static tls_slot g_slot[SEL_NET_MAX_CONN];
static unsigned g_handshakes;
static SSL_CTX* g_ctx;
static BIO_METHOD* g_meth;
static int g_opened;

static unsigned remaining_ms(unsigned long deadline) {
    unsigned long now = sel_net_now_ms();
    return now >= deadline ? 0u : (unsigned)(deadline - now);
}

/* Cheap startup probe (I1344e): amisslmaster.library present and at least the v5 the code needs. Opens no bsdsocket and
 * never calls OpenAmiSSLTags, so cold start cannot hang on the TCP/IP stack. 0 usable, 1 missing or too old. */
int sel_amissl_probe(void) {
    struct Library* lib = OpenLibrary((STRPTR) "amisslmaster.library", AMISSLMASTER_MIN_VERSION);
    if (lib == NULL) {
        return 1;
    }
    CloseLibrary(lib);
    return 0;
}

static int g_open_failed; /* a failed full open is final: later connects answer unavailable without retrying */

int sel_amissl_open(void) {
    struct Library* sb;
    if (g_opened) {
        return 0;
    }
    if (g_open_failed) {
        return 2;
    }
    g_open_failed = 1;
    sb = sel_net_amiga_socketbase();
    if (sb == NULL) {
        return 2; /* no TCP/IP stack: nothing to secure, and not a missing-AmiSSL case */
    }
    AmiSSLMasterBase = OpenLibrary((STRPTR) "amisslmaster.library", AMISSLMASTER_MIN_VERSION);
    if (AmiSSLMasterBase == NULL) {
        return 1;
    }
    if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION, AmiSSL_UsesOpenSSLStructs, FALSE, AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
                       AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase, AmiSSL_SocketBase, (ULONG)sb, AmiSSL_ErrNoPtr,
                       (ULONG)&errno, TAG_DONE) != 0) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
        return 1; /* AmiSSL older than the v5 API */
    }
    g_opened = 1;
    g_open_failed = 0;
    return 0;
}

/* BIO over the socket table. A timeout is a retry, so SSL_* answers WANT_READ/WANT_WRITE and the caller decides. */
static int bio_write(BIO* b, const char* buf, int len) {
    tls_slot* s = BIO_get_data(b);
    int rc;
    BIO_clear_retry_flags(b);
    if (s->handshaking) {
        s->deadline = sel_net_now_ms() + s->timeout_ms;
    }
    rc = sel_net_send((int)(s - g_slot), buf, (size_t)len, remaining_ms(s->deadline));
    if (rc == 0) {
        return len;
    }
    if (rc == SEL_NET_E_TIMEOUT) {
        BIO_set_retry_write(b);
        s->timed_out = 1;
    }
    return -1;
}

static int bio_read(BIO* b, char* buf, int len) {
    tls_slot* s = BIO_get_data(b);
    int n;
    BIO_clear_retry_flags(b);
    if (len <= 0) {
        return 0;
    }
    if (s->handshaking) {
        s->deadline = sel_net_now_ms() + s->timeout_ms;
    }
    n = sel_net_recv((int)(s - g_slot), buf, (size_t)len, remaining_ms(s->deadline));
    if (n > 0) {
        return n;
    }
    if (n == SEL_NET_E_TIMEOUT) {
        BIO_set_retry_read(b);
        s->timed_out = 1;
        return -1;
    }
    return n == SEL_NET_E_CLOSED ? 0 : -1;
}

static long bio_ctrl(BIO* b, int cmd, long num, void* ptr) {
    (void)b;
    (void)num;
    (void)ptr;
    return cmd == BIO_CTRL_FLUSH ? 1 : 0;
}

static int bio_create(BIO* b) {
    BIO_set_init(b, 1);
    return 1;
}

static int tls_init_once(void) {
    const char* ca;
    if (g_ctx != NULL) {
        return 0;
    }
    if (!g_opened) {
        return -1;
    }
    g_meth = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "selecta-socket");
    if (g_meth == NULL || !BIO_meth_set_write(g_meth, bio_write) || !BIO_meth_set_read(g_meth, bio_read) ||
        !BIO_meth_set_ctrl(g_meth, bio_ctrl) || !BIO_meth_set_create(g_meth, bio_create)) {
        return -1;
    }
    g_ctx = SSL_CTX_new(TLS_client_method());
    if (g_ctx == NULL) {
        return -1;
    }
    SSL_CTX_set_min_proto_version(g_ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(g_ctx, SSL_VERIFY_PEER, NULL);
    ca = sel_net_ca_file();
    if (ca != NULL ? !SSL_CTX_load_verify_locations(g_ctx, ca, NULL) : !SSL_CTX_set_default_verify_paths(g_ctx)) {
        SSL_CTX_free(g_ctx);
        g_ctx = NULL;
        return -1;
    }
    return 0;
}

/* Feeds the OpenSSL pool from the shared entropy pool and checks it reports seeded. */
static int seed_rand(void) {
    unsigned char seed[32];
    if (sel_entropy_ready() && sel_entropy_bytes(seed, sizeof seed) == 0) {
        RAND_add(seed, (int)sizeof seed, (double)sizeof seed);
        memset(seed, 0, sizeof seed);
    }
    return RAND_status() == 1;
}

static void slot_release(tls_slot* s) {
    if (s->ssl != NULL) {
        SSL_free(s->ssl);
        s->ssl = NULL;
    }
}

int sel_tls_open(int h, const char* host, unsigned timeout_ms) {
    tls_slot* s;
    BIO* bio;
    int rc;
    if (!sel_tls_available() || sel_amissl_open() != 0) { /* the full AmiSSL open happens here, on the first real connect */
        return SEL_TLS_E_UNAVAILABLE;
    }
    if (h < 0 || h >= SEL_NET_MAX_CONN || tls_init_once() != 0 || !seed_rand()) {
        return SEL_TLS_E_INIT;
    }
    s = &g_slot[h];
    slot_release(s);
    s->ssl = SSL_new(g_ctx);
    bio = BIO_new(g_meth);
    if (s->ssl == NULL || bio == NULL) {
        if (bio != NULL) {
            BIO_free(bio);
        }
        slot_release(s);
        return SEL_TLS_E_INIT;
    }
    BIO_set_data(bio, s);
    SSL_set_bio(s->ssl, bio, bio);
    if (!SSL_set_tlsext_host_name(s->ssl, host) || !SSL_set1_host(s->ssl, host)) {
        slot_release(s);
        return SEL_TLS_E_INIT;
    }
    s->timed_out = 0;
    s->timeout_ms = timeout_ms;
    s->deadline = sel_net_now_ms() + timeout_ms;
    s->handshaking = 1;
    for (;;) {
        rc = SSL_connect(s->ssl);
        if (rc == 1) {
            break;
        }
        rc = SSL_get_error(s->ssl, rc);
        if ((rc == SSL_ERROR_WANT_READ || rc == SSL_ERROR_WANT_WRITE) && !s->timed_out) {
            continue;
        }
        break;
    }
    s->handshaking = 0;
    if (rc == 1) {
        if (SSL_get_verify_result(s->ssl) == X509_V_OK) {
            g_handshakes++;
            return SEL_TLS_OK;
        }
        rc = SEL_TLS_E_VERIFY;
    } else if (SSL_get_verify_result(s->ssl) != X509_V_OK) {
        rc = SEL_TLS_E_VERIFY;
    } else if (rc == SSL_ERROR_WANT_READ || rc == SSL_ERROR_WANT_WRITE) {
        rc = SEL_TLS_E_TIMEOUT;
    } else {
        rc = SEL_TLS_E_HANDSHAKE;
    }
    slot_release(s);
    return rc;
}

int sel_tls_open_insecure(int h, const char* host, unsigned timeout_ms) {
    (void)h;
    (void)host;
    (void)timeout_ms;
    return SEL_TLS_E_UNAVAILABLE; /* verification is never switched off on the Amiga */
}

unsigned sel_tls_handshakes(void) {
    return g_handshakes;
}

int sel_stream_send(int h, const void* data, size_t len, unsigned timeout_ms) {
    tls_slot* s = h >= 0 && h < SEL_NET_MAX_CONN ? &g_slot[h] : NULL;
    const unsigned char* p = data;
    if (s == NULL || s->ssl == NULL) {
        return sel_net_send(h, data, len, timeout_ms);
    }
    s->deadline = sel_net_now_ms() + timeout_ms;
    s->timed_out = 0;
    while (len > 0) {
        int rc = SSL_write(s->ssl, p, (int)len);
        if (rc > 0) {
            p += rc;
            len -= (size_t)rc;
        } else {
            int e = SSL_get_error(s->ssl, rc);
            if ((e != SSL_ERROR_WANT_WRITE && e != SSL_ERROR_WANT_READ) || s->timed_out || remaining_ms(s->deadline) == 0) {
                return e == SSL_ERROR_WANT_WRITE || e == SSL_ERROR_WANT_READ ? SEL_NET_E_TIMEOUT : SEL_NET_E_IO;
            }
        }
    }
    return 0;
}

int sel_stream_recv(int h, void* buf, size_t cap, unsigned timeout_ms) {
    tls_slot* s = h >= 0 && h < SEL_NET_MAX_CONN ? &g_slot[h] : NULL;
    if (s == NULL || s->ssl == NULL) {
        return sel_net_recv(h, buf, cap, timeout_ms);
    }
    s->deadline = sel_net_now_ms() + timeout_ms;
    s->timed_out = 0;
    for (;;) {
        int rc = SSL_read(s->ssl, buf, (int)cap);
        int e;
        if (rc > 0) {
            return rc;
        }
        e = SSL_get_error(s->ssl, rc);
        if (e == SSL_ERROR_ZERO_RETURN) {
            return SEL_NET_E_CLOSED;
        }
        if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
            if (s->timed_out || remaining_ms(s->deadline) == 0) {
                return SEL_NET_E_TIMEOUT;
            }
            continue;
        }
        return rc == 0 ? SEL_NET_E_CLOSED : SEL_NET_E_IO; /* rc 0 with SSL_ERROR_SYSCALL: peer closed without close_notify */
    }
}

void sel_stream_close(int h) {
    if (h >= 0 && h < SEL_NET_MAX_CONN && g_slot[h].ssl != NULL) {
        g_slot[h].deadline = sel_net_now_ms() + 200u;
        (void)SSL_shutdown(g_slot[h].ssl);
        slot_release(&g_slot[h]);
    }
    sel_net_close(h);
}
