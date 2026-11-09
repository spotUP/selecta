/* I1127b: content companions in the library cache. The sanitiser is security-relevant (names come from song bytes
 * of any upload), so every reject case of the design (section 7) is a row here; trim exemption (U2) is driven with
 * a fake registry whose one hook names banks. The wiring through the real product is the remote-eup-banks test. */
#include "cache.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #cond); \
            failures++;                                    \
        }                                                  \
    } while (0)

static unsigned g_probe_calls, g_data_calls;
static const char* g_names[16];
static unsigned g_nnames;

/* Fake hook: claims *.fk; returns g_names verbatim. */
static unsigned hook(const char* name, const uint8_t* data, size_t size, char out[][SEL_RP_COMPANION_NAME_MAX],
                     unsigned max, const sel_host* host) {
    size_t n = strlen(name);
    unsigned i, k = 0;

    (void)size;
    (void)host;
    if (data == NULL) {
        g_probe_calls++; /* every claim probe counts, claimed or not */
    }
    if (n < 3 || strcasecmp(name + n - 3, ".fk") != 0) {
        return 0;
    }
    if (data == NULL) {
        return 1;
    }
    g_data_calls++;
    for (i = 0; i < g_nnames && k < max; i++) {
        strncpy(out[k], g_names[i], SEL_RP_COMPANION_NAME_MAX); /* a 64+ character name fills all 64 bytes, no NUL */
        k++;
    }
    return k;
}

static const sel_tag g_hooks[] = {{SEL_RP_CONTENT_COMPANIONS, (uintptr_t)hook}, {SEL_TAG_END, 0}};
static const sel_tag g_desc[] = {{SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER}, {SEL_PLUGIN_TAGS, (uintptr_t)g_hooks}, {SEL_TAG_END, 0}};
static uint32_t reg_count(void) { return 1; }
static const sel_tag* reg_get(uint32_t i) { return i == 0 ? g_desc : NULL; }

static void* host_alloc(const sel_host* h, size_t n, uint32_t kind) { (void)h; (void)kind; return malloc(n); }
static void host_free(const sel_host* h, void* p) { (void)h; free(p); }

static unsigned names_for(sel_cache* c, const char* path, const char** in, unsigned nin, char out[][SEL_CACHE_PATH_MAX]) {
    uint8_t head[4] = {0, 0, 0, 0};
    unsigned i;

    for (i = 0; i < nin; i++) {
        g_names[i] = in[i];
    }
    g_nnames = nin;
    return sel_content_companion_names(c, path, head, sizeof head, out, SEL_CACHE_CONTENT_MAX);
}

static void sanitiser(sel_cache* c) {
    char out[SEL_CACHE_CONTENT_MAX][SEL_CACHE_PATH_MAX];
    char long63[64], long64[65];
    const char* rejects[] = {"../x", "/etc/x", "a\\b", "c:x", "..", "a..b", ".hidden", "x.part", "X.PART", "song.fk",
                             "SONG.FK", "caf\xc3\xa9.fmb", "\x83\x65.fmb", " lead.pmb", "trail.pmb ", ""};
    unsigned i;
    unsigned before;

    memset(long63, 'a', 63);
    long63[63] = '\0';
    memset(long64, 'a', 64);
    long64[64] = '\0';
    for (i = 0; i < sizeof rejects / sizeof *rejects; i++) {
        const char* in[1];

        in[0] = rejects[i];
        before = c->stats.content_rejected;
        if (names_for(c, "Euphony/A/song.fk", in, 1, out) != 0 || c->stats.content_rejected != before + 1u) {
            printf("FAIL reject case %u not rejected: [%s]\n", i, rejects[i]);
            failures++;
        }
    }
    {
        const char* in[1];

        in[0] = long64; /* 64 characters: rejected (no room for the NUL) */
        before = c->stats.content_rejected;
        CHECK(names_for(c, "Euphony/A/song.fk", in, 1, out) == 0 && c->stats.content_rejected == before + 1u);
        in[0] = long63;
        CHECK(names_for(c, "Euphony/A/song.fk", in, 1, out) == 1);
    }
    {
        const char* in[2] = {"Bank 1.pmb", "BANK 1.PMB"};

        CHECK(names_for(c, "Euphony/A/song.fk", in, 2, out) == 1); /* case-insensitive dedupe */
        CHECK(strcmp(out[0], "Euphony/A/Bank 1.pmb") == 0);
    }
    {
        const char* in[12] = {"a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "a10", "a11", "a12"};

        CHECK(names_for(c, "song.fk", in, 12, out) == 12); /* I1237: up to SEL_CACHE_CONTENT_MAX names, no directory */
        CHECK(strcmp(out[0], "a1") == 0);
    }
    {
        const char* in[1] = {"Instruments/x.instr"};
        const char* bad[4] = {"Instruments/sub/x.instr", "/x.instr", "Instruments//x", "Instruments/.x"};
        unsigned bi;

        CHECK(names_for(c, "A/song.fk", in, 1, out) == 1 && strcmp(out[0], "A/Instruments/x.instr") == 0); /* I1237 */
        for (bi = 0; bi < 4; bi++) {
            CHECK(names_for(c, "A/song.fk", bad + bi, 1, out) == 0);
        }
    }
    {
        /* claim probe: a name no hook claims calls the hook only as a probe, with no bytes */
        unsigned d = g_data_calls, p = g_probe_calls;

        CHECK(sel_content_companion_names(c, "x/other.mod", NULL, 0, NULL, 0) == 0);
        CHECK(sel_content_companion_names(c, "x/a.FK", NULL, 0, NULL, 0) == 1);
        CHECK(g_data_calls == d && g_probe_calls == p + 2u);
    }
}

static void mk(const char* dir, const char* file, const char* body, long mtime) {
    char p[1024];
    FILE* f;
    snprintf(p, sizeof p, "%s/%s", dir, file);
    f = fopen(p, "wb");
    if (f != NULL) {
        fputs(body, f);
        fclose(f);
    }
    {
        char cmd[1100];

        snprintf(cmd, sizeof cmd, "touch -t 2023010100%02ld '%s'", mtime, p);
        (void)system(cmd);
    }
}

static int exists(const char* dir, const char* file) {
    char p[1024];

    snprintf(p, sizeof p, "%s/%s", dir, file);
    return access(p, F_OK) == 0;
}

/* U2: a kept .fk song's banks survive a trim to budget 0; a non-kept song's banks are evicted. */
static int keep_a(void* ctx, const char* key) { (void)ctx; return strcmp(key, "modland:D/a.fk") == 0; }

static void trim(sel_cache* c, const char* root) {
    char d[1024];
    sel_cache_trim_result tr;
    const char* in[2] = {"bank1.fmb", "BANK2.pmb"};

    snprintf(d, sizeof d, "%s/files/modland/D", root);
    mk(d, "a.fk", "x", 5);
    mk(d, "bank1.fmb", "x", 1);
    mk(d, "bank2.pmb", "x", 2); /* stored under another case: matched case-insensitively */
    mk(d, "b.fk", "x", 3);
    mk(d, "other.bin", "x", 4);
    g_names[0] = in[0];
    g_names[1] = in[1];
    g_nnames = 2;
    sel_cache_trim(c, 0, keep_a, NULL, NULL, "", &tr);
    CHECK(tr.companion_exempt == 2u);
    CHECK(exists(d, "a.fk") && exists(d, "bank1.fmb") && exists(d, "bank2.pmb"));
    CHECK(!exists(d, "other.bin"));
    CHECK(!exists(d, "b.fk"));
}

int main(void) {
    sel_host host;
    sel_cache c;
    char root[] = "/tmp/selecta_ccXXXXXX";
    char d[1100];

    memset(&host, 0, sizeof host);
    host.mem_alloc = host_alloc;
    host.mem_free = host_free;
    if (mkdtemp(root) == NULL) {
        return 1;
    }
    sel_cache_init(&c, root, 0);
    c.registry_count = reg_count;
    c.registry_get = reg_get;
    c.hook_host = &host;
    sanitiser(&c);
    snprintf(d, sizeof d, "%s/files/modland/D", root);
    {
        char cmd[1300];

        snprintf(cmd, sizeof cmd, "mkdir -p '%s'", d);
        (void)system(cmd);
        trim(&c, root);
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", root);
        (void)system(cmd);
    }
    if (failures == 0) {
        printf("content_companions_ok\n");
    }
    return failures != 0;
}
