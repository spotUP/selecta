/* I710ga3: system-file download loop over the net fixture (plan thoughts/shared/plans/2026-11-06-I710ga-system-files.md
 * section 4). argv: 1 scratch dir, 2 sha256 of fixture mirror.sysfile.test/sysfiles/good.bin, 3 db path.
 * SELECTA_NET_MAP / SELECTA_NET_ALLOW come from sysfile_fetch.cmake. Never the real network. */
#include "db/db.h"
#include "sysfile.h"
#include "sysfile_fetch.h"

#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/registry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            fails++;                                    \
        }                                               \
    } while (0)

#define U404 "http://csdb.test/files/missing.bin"
#define UGOOD "http://mirror.sysfile.test/sysfiles/good.bin"
#define UBAD "http://mirror.sysfile.test/sysfiles/bad.bin"

static const sel_host* g_host;
static char g_dir[400];

static sel_sysfile_decl decl(const char* name, const char* sha, const char* urls) {
    sel_sysfile_decl d;

    memset(&d, 0, sizeof d);
    d.plugin = "test";
    d.name = name;
    d.label = "Test file";
    d.hint = "";
    d.sha256 = sha;
    d.urls = urls;
    return d;
}

static int stored(const char* name) {
    char p[512];

    snprintf(p, sizeof p, "%s/%s", g_dir, name);
    return access(p, F_OK) == 0;
}

int main(int argc, char** argv) {
    sel_sysfile_res r;
    sel_sysfile_decl d;
    sel_sysfile_override ov;
    sel_library* lib;
    int i, st;

    if (argc < 4) {
        return 2;
    }
    snprintf(g_dir, sizeof g_dir, "%s", argv[1]);
    g_host = sel_host_default();
    CHECK(sel_registry_init(g_host) > 0);
    unsetenv("SELECTA_SYSFILE_URLS_A_BIN");

    /* (a) first URL is a 404, the second serves the file */
    d = decl("a.bin", argv[2], U404 ";" UGOOD);
    CHECK(sel_sysfile_fetch(g_host, g_dir, &d, &r) == SEL_SYSF_OK);
    CHECK(r.tries == 2 && r.requests == 2 && r.rejected == 0);
    CHECK(strcmp(r.served, UGOOD) == 0);
    CHECK(stored("a.bin") && !stored("a.bin.dl") && sel_sysfile_resolve(g_dir, &d, NULL, 0));

    /* (c) every URL fails */
    d = decl("c.bin", argv[2], U404 ";http://csdb.test/other.bin");
    CHECK(sel_sysfile_fetch(g_host, g_dir, &d, &r) == SEL_SYSF_ALL_FAILED);
    CHECK(r.tries == 2 && r.requests == 2 && r.served[0] == '\0');
    CHECK(!stored("c.bin") && !stored("c.bin.dl"));

    /* (b) bodies arrive but are not the accepted file: NO_MATCH, nothing stored */
    d = decl("b.bin", argv[2], U404 ";" UBAD ";" UBAD);
    CHECK(sel_sysfile_fetch(g_host, g_dir, &d, &r) == SEL_SYSF_NO_MATCH);
    CHECK(r.tries == 3 && r.rejected == 2);
    CHECK(!stored("b.bin") && !stored("b.bin.dl") && !stored("b.bin.part"));

    /* (e) env override replaces the list: the overriding URL is the only one requested */
    d = decl("a.bin", argv[2], U404 ";" UBAD);
    {
        char p[512];
        snprintf(p, sizeof p, "%s/a.bin", g_dir);
        remove(p);
    }
    setenv("SELECTA_SYSFILE_URLS_A_BIN", UGOOD, 1);
    CHECK(sel_sysfile_fetch(g_host, g_dir, &d, &r) == SEL_SYSF_OK);
    CHECK(r.tries == 1 && r.requests == 1 && strcmp(r.served, UGOOD) == 0);
    unsetenv("SELECTA_SYSFILE_URLS_A_BIN");

    /* env wins over the I1076 table; the table prepends when replace is 0 */
    ov.name = "a.bin";
    ov.urls = UGOOD;
    ov.replace = 0;
    sel_sysfile_set_overrides(&ov, 1);
    {
        char p[512];
        char urls[6][SEL_SYSFILE_URL_MAX];
        unsigned n;

        snprintf(p, sizeof p, "%s/a.bin", g_dir);
        remove(p);
        n = sel_sysfile_urls(&d, urls, 6);
        CHECK(n == 3 && strcmp(urls[0], UGOOD) == 0 && strcmp(urls[1], U404) == 0);
        CHECK(sel_sysfile_fetch(g_host, g_dir, &d, &r) == SEL_SYSF_OK && r.tries == 1);
        setenv("SELECTA_SYSFILE_URLS_A_BIN", U404, 1);
        n = sel_sysfile_urls(&d, urls, 6);
        CHECK(n == 1 && strcmp(urls[0], U404) == 0);
        unsetenv("SELECTA_SYSFILE_URLS_A_BIN");
    }
    sel_sysfile_set_overrides(NULL, 0);

    /* job kind on the library job thread: request, poll to the result, the file is there */
    lib = sel_library_open(g_host, sel_machine_probe(g_host), 0, argv[3]);
    CHECK(lib != NULL);
    if (lib != NULL) {
        char p[512];

        d = decl("job.bin", argv[2], U404 ";" UGOOD);
        CHECK(sel_library_sysfile_poll(lib, "job.bin") == SEL_SYSF_NONE);
        CHECK(sel_library_sysfile_request(lib, g_dir, &d) == 1);
        st = SEL_SYSF_RUNNING;
        for (i = 0; i < 100 && st == SEL_SYSF_RUNNING; i++) {
            usleep(100000);
            st = sel_library_sysfile_poll(lib, "job.bin");
        }
        CHECK(st == SEL_SYSF_OK);
        CHECK(stored("job.bin") && sel_sysfile_resolve(g_dir, &d, p, sizeof p));
        /* a name that fails reports ALL_FAILED through the same poll */
        d = decl("jobf.bin", argv[2], U404);
        CHECK(sel_library_sysfile_request(lib, g_dir, &d) == 1);
        st = SEL_SYSF_RUNNING;
        for (i = 0; i < 100 && st == SEL_SYSF_RUNNING; i++) {
            usleep(100000);
            st = sel_library_sysfile_poll(lib, "jobf.bin");
        }
        CHECK(st == SEL_SYSF_ALL_FAILED);
        sel_library_close(lib);
    }

    if (fails != 0) {
        printf("sysfile_fetch: %d failure(s)\n", fails);
        return 1;
    }
    printf("sysfile_fetch_ok\n");
    return 0;
}
