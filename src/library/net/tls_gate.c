/* TLS availability gate (I899g). Which backend this build has and whether it is usable right now. mbedTLS is always
 * usable; AmiSSL is opened at runtime (sel_amissl_open in tls_amissl.c) and may be missing, in which case https is off
 * and the UI shows one hint line. Kept apart from tls.c so a host test can compile it with SEL_TLS_AMISSL and a stub
 * sel_amissl_open. */
#include "tls.h"

#define NOTE_INSTALL "Install AmiSSL for secure downloads"

static int g_forced = -1;

#ifdef SEL_TLS_AMISSL
/* tls_amissl.c: 0 opened, 1 amisslmaster.library missing or too old, 2 any other failure (no TCP/IP stack, bad open). */
int sel_amissl_open(void);

static int g_state;      /* 0 not probed, 1 open, -1 failed */
static int g_open_rc;

static void probe(void) {
    if (g_state == 0) {
        g_open_rc = sel_amissl_open();
        g_state = g_open_rc == 0 ? 1 : -1;
    }
}

const char* sel_tls_backend(void) {
    return "amissl";
}

int sel_tls_available(void) {
    if (g_forced >= 0) {
        return g_forced;
    }
    probe();
    return g_state > 0;
}

const char* sel_tls_missing_note(void) {
    if (g_forced >= 0) {
        return g_forced ? NULL : NOTE_INSTALL;
    }
    probe();
    return g_state < 0 && g_open_rc == 1 ? NOTE_INSTALL : NULL;
}
#else
const char* sel_tls_backend(void) {
    return "mbedtls";
}

int sel_tls_available(void) {
    return g_forced >= 0 ? g_forced : 1;
}

const char* sel_tls_missing_note(void) {
    return g_forced == 0 ? NOTE_INSTALL : NULL;
}
#endif

void sel_tls_test_force(int available) {
    g_forced = available < 0 ? -1 : (available ? 1 : 0);
}
