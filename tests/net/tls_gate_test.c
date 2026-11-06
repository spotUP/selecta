/* I899g: TLS backend choice and the missing-library fallback.
 *   tls_gate_test mirrors      host build (selecta_library): backend mbedtls; with TLS forced off the https mirrors are
 *                              skipped (http/ftp list only), the DEViLBOX base is http and the install note is set.
 *   tls_gate_test amissl <rc>  tls_gate.c compiled with SEL_TLS_AMISSL and the stub sel_amissl_open below returning rc
 *                              (0 opened, 1 library missing or old, 2 other failure): the probe runs once and the note
 *                              appears only for rc 1. */
#include "tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GATE_AMISSL_STUB
static int g_rc, g_calls;
int sel_amissl_open(void) {
    g_calls++;
    return g_rc;
}

static int amissl_mode(int rc) {
    const char* note;
    g_rc = rc;
    if (strcmp(sel_tls_backend(), "amissl") != 0) {
        printf("FAIL backend %s\n", sel_tls_backend());
        return 1;
    }
    if (g_calls != 0) {
        printf("FAIL probed before first use\n");
        return 1;
    }
    if (sel_tls_available() != (rc == 0) || sel_tls_available() != (rc == 0) || g_calls != 1) {
        printf("FAIL available rc=%d calls=%d\n", rc, g_calls);
        return 1;
    }
    note = sel_tls_missing_note();
    if ((rc == 1) != (note != NULL) || (note != NULL && strcmp(note, "Install AmiSSL for secure downloads") != 0)) {
        printf("FAIL note rc=%d\n", rc);
        return 1;
    }
    printf("tls_gate_amissl_ok rc=%d calls=%d available=%d note=%s\n", rc, g_calls, sel_tls_available(), note ? note : "-");
    return 0;
}
#else
#include "mirrors.h"

static int has_https(const sel_mirror_set* s) {
    unsigned i;
    for (i = 0; i < s->count; i++) {
        if (strncmp(s->base[i], "https://", 8) == 0) {
            return 1;
        }
    }
    return 0;
}

static int mirrors_mode(void) {
    sel_mirror_set s;
    unsigned n_on_hvsc, n_on_modland;
    if (strcmp(sel_tls_backend(), "mbedtls") != 0 || !sel_tls_available() || sel_tls_missing_note() != NULL) {
        printf("FAIL host backend\n");
        return 1;
    }
    n_on_hvsc = sel_mirrors_init(&s, "hvsc", 0);
    n_on_modland = sel_mirrors_init(&s, "modland", 0);
    if (n_on_hvsc != 1 || strncmp(sel_devilbox_base(), "https://", 8) != 0) {
        printf("FAIL tls on: hvsc=%u base=%s\n", n_on_hvsc, sel_devilbox_base());
        return 1;
    }
    sel_tls_test_force(0);
    if (sel_tls_available() || sel_tls_missing_note() == NULL) {
        printf("FAIL forced off\n");
        return 1;
    }
    if (sel_mirrors_init(&s, "hvsc", 0) != 0 || sel_mirrors_init(&s, "asma", 0) != 0) {
        printf("FAIL https-only source kept a mirror\n");
        return 1;
    }
    if (sel_mirrors_init(&s, "modland", 0) != n_on_modland || has_https(&s) || s.count == 0) {
        printf("FAIL modland list count=%u https=%d\n", s.count, has_https(&s));
        return 1;
    }
    setenv("SELECTA_MIRRORS_HVSC", "https://a.example/,http://b.example/", 1);
    if (sel_mirrors_init(&s, "hvsc", 0) != 1 || has_https(&s) || strcmp(s.base[0], "http://b.example/") != 0) {
        printf("FAIL env list not filtered\n");
        return 1;
    }
    sel_mirrors_init(&s, "modland", 1);
    if (s.count != 1 || strncmp(s.base[0], "http://", 7) != 0) {
        printf("FAIL devilbox route base %s\n", s.count ? s.base[0] : "-");
        return 1;
    }
    sel_tls_test_force(-1);
    printf("tls_gate_mirrors_ok http_only_modland=%u\n", n_on_modland);
    return 0;
}
#endif

int main(int argc, char** argv) {
#ifdef GATE_AMISSL_STUB
    return argc > 2 && strcmp(argv[1], "amissl") == 0 ? amissl_mode(atoi(argv[2])) : 2;
#else
    return argc > 1 && strcmp(argv[1], "mirrors") == 0 ? mirrors_mode() : 2;
#endif
}
