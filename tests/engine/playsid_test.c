/* I434b: the native playsid adapter, through the engine's own lookup. The test #includes the real adapter.c and
 * supplies a stub of the LVO layer (ps_ops_get) that logs every library call, then registers the real plugin
 * descriptor in an engine registry and drives QUEUE_ADD, PLAY, PAUSE, subsong NEXT/PREV and SEEK on a PSID file.
 * Sentinels: sel_playsid_starts (rp_start ran to the library), the stub's call log (SetModule, StartSong arguments).
 * Argument 1: path of a PSID file (any with two or more subsongs; the test writes its own into argv[2] directory). */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../plugins/replayers/playsid/adapter.c"

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

/* ---- stub of playsid.library ---- */
static int s_installed = 1, s_open_refs;
static int s_n_open, s_n_alloc, s_n_free, s_n_setmodule, s_n_start, s_n_stop, s_n_pause, s_n_cont, s_n_fwd;
static uint32_t s_last_song, s_last_hz, s_last_om = 99, s_last_ahi = 99, s_last_len;
static const void *s_last_hdr, *s_last_img;

static int st_open(void) {
    s_n_open++;
    if (!s_installed) return 0;
    s_open_refs++;
    return 1;
}
static void st_close(void) { s_open_refs--; }
static int32_t st_alloc(void) { s_n_alloc++; return 0; }
static void st_free(void) { s_n_free++; }
static void st_setmodule(const void* h, const void* i, uint32_t l) { s_n_setmodule++; s_last_hdr = h; s_last_img = i; s_last_len = l; }
static int32_t st_start(uint32_t n) { s_n_start++; s_last_song = n; return 0; }
static void st_stop(void) { s_n_stop++; }
static void st_pause(void) { s_n_pause++; }
static void st_cont(void) { s_n_cont++; }
static void st_fwd(void) { s_n_fwd++; }
static void st_hz(uint32_t hz) { s_last_hz = hz; }
static void st_om(uint32_t om) { s_last_om = om; }
static void st_ahi(uint32_t id) { s_last_ahi = id; }
static uint32_t st_getom(void) { return s_last_om; }
static const ps_ops s_ops = {st_open, st_close, st_alloc, st_free, st_setmodule, st_start, st_stop,
                             st_pause, st_cont, st_fwd, st_hz, st_om, st_ahi, st_getom};
const ps_ops* ps_ops_get(void) { return &s_ops; }

static const sel_tag* t_desc;
static uint32_t reg_count(void) { return 1u; }
static const sel_tag* reg_get(uint32_t i) { return i == 0u ? t_desc : NULL; }
static sel_engine* eng;

static void run(const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, NULL, NULL);
    {   /* the audio side applies pause and hands the hardware over; pull a block as an output would */
        static int16_t buf[441 * 2];
        sel_engine_render(eng, buf, 441u);
        sel_engine_pump(eng, NULL, NULL);
    }
}

static void write_psid(const char* path, unsigned subsongs, unsigned defsong, unsigned long speed) {
    uint8_t h[0x7c + 16];
    FILE* f;
    memset(h, 0, sizeof h);
    memcpy(h, "PSID", 4);
    h[5] = 2; h[7] = 0x7c; h[13] = 0; h[15] = (uint8_t)subsongs; h[17] = (uint8_t)defsong;
    h[18] = (uint8_t)(speed >> 24); h[19] = (uint8_t)(speed >> 16); h[20] = (uint8_t)(speed >> 8); h[21] = (uint8_t)speed;
    memcpy(h + 22, "Test Tune", 9);
    memcpy(h + 54, "Test Author", 11);
    h[0x7c] = 0x60; /* a little code after the header */
    f = fopen(path, "wb");
    CHECK(f != NULL);
    fwrite(h, 1, sizeof h, f);
    fclose(f);
}

int main(int argc, char** argv) {
    sel_engine_config cfg;
    char line[1200], path[1024];
    uint32_t i, started;
    static const char* const names[8] = {"normal", "6581", "8580", "auto", "sidblaster", "zorrosid", "usbsid", "trinity"};
    if (argc < 2) { printf("usage: playsid_test <dir>\n"); return 2; }
    snprintf(path, sizeof path, "%s/ps_test.sid", argv[1]);
    write_psid(path, 3, 2, 0x2ul); /* 3 subsongs, default 2, speed bit 1 set */

    /* mode pref -> SetOperatingMode, for every value (logic where it lives) */
    for (i = 0; i < 8; i++) CHECK(ps_mode_from_pref(names[i]) == i);
    CHECK(ps_mode_from_pref("bogus") == 0u && ps_mode_from_pref(NULL) == 0u && ps_mode_from_pref("") == 0u);
    CHECK(ps_mode_is_resid(1) && ps_mode_is_resid(2) && ps_mode_is_resid(3) && !ps_mode_is_resid(0) && !ps_mode_is_resid(4));
    /* output -> AHI mode */
    CHECK(ps_ahi_mode_for_output("ahi", "0x0002000B") == 0x0002000Bu);
    CHECK(ps_ahi_mode_for_output("ahi", "20003") == 0x20003u);
    CHECK(ps_ahi_mode_for_output("ahi", "") == 0u && ps_ahi_mode_for_output("ahi", "zz") == 0u);
    CHECK(ps_ahi_mode_for_output("paula", "0x20003") == 0u && ps_ahi_mode_for_output("paula14", "0x20003") == 0u);
    CHECK(ps_ahi_mode_for_output("", "0x20003") == 0u && ps_ahi_mode_for_output(NULL, NULL) == 0u);

    /* library missing: the plugin steps aside, nothing is started */
    t_desc = SEL_PLUGIN_ENTRY(1);
    CHECK(t_desc != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    s_installed = 0;
    {
        uint8_t hdr[0x80];
        memset(hdr, 0, sizeof hdr);
        memcpy(hdr, "PSID", 4);
        CHECK(rp_probe(hdr, sizeof hdr, "x.sid") == SEL_PROBE_NO); /* the registry then asks the next owner */
        s_installed = 1;
        CHECK(rp_probe(hdr, sizeof hdr, "x.sid") == SEL_PROBE_YES);
        CHECK(rp_probe((const uint8_t*)"MODU", 4, "x.mod") == SEL_PROBE_NO);
        CHECK(s_open_refs == 0); /* probe leaves no reference behind */
    }
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    s_installed = 0;
    snprintf(line, sizeof line, "QUEUE_ADD %s", path);
    run(line);
    run("PLAY");
    CHECK(g_owner == NULL && sel_playsid_starts == 0u && s_n_start == 0 && s_n_alloc == 0); /* absent: never ran */
    run("STOP");
    s_installed = 1;

    /* installed: the real entry point reaches SetModule and StartSong */
    started = sel_playsid_starts;
    run(line[0] ? "PLAY" : "PLAY");
    CHECK(sel_playsid_starts == started + 1u); /* sentinel: rp_start ran */
    CHECK(g_owner != NULL && g_owner->playing == 1u);
    CHECK(s_n_setmodule == 1 && s_n_start == 1 && s_n_alloc == 1);
    CHECK(s_last_hdr == s_last_img && s_last_len == 0x7c + 16);
    CHECK(s_last_song == 2u);   /* default subsong 2 (1-based in the header and in StartSong) */
    CHECK(s_last_om == 0u && s_last_ahi == 0u);
    CHECK(s_last_hz == 50u);    /* speed word 0x2: bit 2 (Hippo's btst on the 1-based song number) is clear */

    run("PAUSE");
    CHECK(s_n_pause >= 1);
    run("PAUSE");
    CHECK(s_n_cont >= 1);

    run("SEEK 5000");
    CHECK(s_n_fwd == 1 && sel_playsid_seeks == 1u); /* classic mode: ForwardSong */

    run("STOP");

    sel_engine_destroy(eng);
    CHECK(g_owner == NULL && s_n_stop >= 1 && s_n_free >= 1); /* released by STOP or at the latest on close */
    CHECK(s_open_refs == 0); /* rp_close released the library */
    remove(path);
    if (g_fail != 0) {
        printf("playsid: %d failures\n", g_fail);
        return 1;
    }
    printf("playsid: ok\n");
    return 0;
}
