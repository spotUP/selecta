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
#include <unistd.h>

#include <selecta/machine.h>

#include "db/db.h"
#include "db/db_internal.h"
#include <pthread.h>
#include "song_key.h"

#include <selecta/format_label.h>
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
static uint32_t s_filt_in = 99, s_filt_ext = 99, s_boost = 99, s_over = 99, s_n_filt, s_n_boost, s_n_over;
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
static void st_filt(uint32_t i, uint32_t e) { s_n_filt++; s_filt_in = i; s_filt_ext = e; }
static void st_boost(uint32_t b) { s_n_boost++; s_boost = b; }
static void st_over(uint32_t o) { s_n_over++; s_over = o; }
static uint32_t st_getom(void) { return s_last_om; }
/* GetResidAudioBuffer stub: s_buf_sids SIDs (0 = no buffer), a different ramp per SID */
static int8_t s_wave[3][1200];
static uint32_t s_buf_sids, s_buf_len = 1200, s_n_getbuf;
static uint32_t st_getbuf(const int8_t* bufs[3], uint32_t* period) {
    s_n_getbuf++;
    *period = 128;
    bufs[0] = s_buf_sids ? s_wave[0] : NULL;
    bufs[1] = s_buf_sids > 1 ? s_wave[1] : bufs[0];
    bufs[2] = s_buf_sids > 2 ? s_wave[2] : NULL;
    return s_buf_sids ? s_buf_len : 0;
}
/* MeasureResidPerformance stub (I434h): s_meas_ms against the one-SID reference s_meas_ref */
static uint32_t s_meas_ms = 10, s_meas_ref = 100, s_n_meas;
static uint32_t st_measure(uint32_t o, uint32_t i, uint32_t e, uint32_t* limit) {
    (void)o; (void)i; (void)e;
    s_n_meas++;
    *limit = s_meas_ref;
    return s_meas_ms;
}
/* ReadSidRegs stub (I434j): a known pattern per chip, counting calls */
static uint8_t s_sid_regs[3][25], s_sid_env[3][3];
static uint32_t s_n_readregs;
static uint32_t st_readregs(uint8_t* regs, uint8_t* env, uint32_t chips) {
    s_n_readregs++;
    memcpy(regs, s_sid_regs, chips * 25u);
    memcpy(env, s_sid_env, chips * 3u);
    return chips;
}
/* SetChannelEnable stub (MPS-19): logs every call's four enable words */
static uint16_t s_en_log[64][4];
static uint32_t s_n_en;
static void st_chenable(const uint16_t enable[4]) {
    if (s_n_en < 64u) memcpy(s_en_log[s_n_en], enable, 4u * sizeof(uint16_t));
    s_n_en++;
}
static const ps_ops s_ops = {st_open, st_close, st_alloc, st_free, st_setmodule, st_start, st_stop,
                             st_pause, st_cont, st_fwd, st_hz, st_om, st_ahi, st_getom, st_filt, st_boost, st_over, st_getbuf, st_measure, st_readregs, st_chenable};
const ps_ops* ps_ops_get(void) { return &s_ops; }

static const sel_tag* t_desc;
static uint32_t reg_count(void) { return 1u; }
static const sel_tag* reg_get(uint32_t i) { return i == 0u ? t_desc : NULL; }
static sel_engine* eng;

static int g_run_wait = 1;
static void run(const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    if (g_run_wait) {
        sel_engine_pump_reads(eng, NULL, NULL, 2000u); /* I906g: a song start waits for its posted library read */
    } else {
        sel_engine_pump(eng, NULL, NULL);
    }
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

/* A new rp_start through the engine: SUBSONG reopens the file in the idle slot; the sentinel proves the adapter ran. */
static void restart(void) {
    static unsigned n;
    uint32_t before = sel_playsid_starts;
    char l[32];
    snprintf(l, sizeof l, "SUBSONG %u", 1u + (n++ & 1u));
    run(l);
    CHECK(sel_playsid_starts == before + 1u);
}

/* I434g: HW-kind scope feed. A v4 PSID with `sids` chips (header bytes 0x7a / 0x7b), reSID mode, a stub buffer per SID:
 * the engine's viz frame gets voice_count = SID count and the stub's samples in scope[k]; classic mode gets 0 voices. */
static void tick(void);
static void viz_scope_test(const char* dir) {
    static const char* const modes[2] = {"6581", "normal"};
    char p[1100], line[1200];
    uint32_t sids, m, k, i;
    snprintf(p, sizeof p, "%s/ps_viz.sid", dir);
    for (k = 0; k < 3; k++) for (i = 0; i < 1200; i++) s_wave[k][i] = (int8_t)(((int)i % 50 - 25) * (int)(k + 1));
    for (sids = 1; sids <= 3; sids++) {
        write_psid(p, 1, 1, 0);
        {
            FILE* f = fopen(p, "r+b");
            uint8_t b;
            CHECK(f != NULL);
            if (f == NULL) return;
            b = 4; fseek(f, 5, SEEK_SET); fwrite(&b, 1, 1, f);
            b = sids > 1 ? 0x42 : 0; fseek(f, 0x7a, SEEK_SET); fwrite(&b, 1, 1, f);
            b = sids > 2 ? 0x44 : 0; fseek(f, 0x7b, SEEK_SET); fwrite(&b, 1, 1, f);
            fclose(f);
        }
        for (m = 0; m < 2; m++) {
            sel_engine_config cfg;
            const sel_viz_frame* vf;
            uint32_t fills0 = sel_playsid_viz_fills, calls0 = s_n_getbuf, nz = 0;
            memset(&cfg, 0, sizeof cfg);
            cfg.registry_count = reg_count;
            cfg.registry_get = reg_get;
            s_installed = 1;
            s_buf_sids = sids;
            eng = sel_engine_create(sel_host_default(), &cfg);
            CHECK(eng != NULL);
            snprintf(line, sizeof line, "PREF_SET playsid.mode \"%s\"", modes[m]);
            run(line);
            snprintf(line, sizeof line, "QUEUE_ADD %s", p);
            run(line);
            run("PLAY");
            tick();
            tick();
            vf = sel_engine_state(eng)->viz;
            CHECK(vf != NULL && vf->seq != 0u);
            if (m == 0) { /* reSID: one voice per SID, the stub's samples scaled by 256, SIDs differ */
                CHECK(vf->voice_count == sids);
                for (k = 0; k < sids; k++) {
                    for (i = 0; i < SEL_VIZ_SCOPE_FRAMES; i++) if (vf->scope[k][i] != 0) nz++;
                    CHECK(vf->peak[k] == (int16_t)(25 * (int)(k + 1) * 256));
                }
                CHECK(nz > 0u);
                if (sids > 1) CHECK(vf->scope[0][7] != vf->scope[1][7]);
                CHECK(sel_playsid_viz_fills > fills0 && s_n_getbuf > calls0); /* sentinel: the hook ran */
            } else { /* classic: no buffer, the library is not asked, the scope keeps its empty state */
                CHECK(vf->voice_count == 0u && vf->scope[0][5] == 0);
                CHECK(sel_playsid_viz_fills == fills0 && s_n_getbuf == calls0);
            }
            run("STOP");
            sel_engine_destroy(eng);
            eng = NULL;
        }
    }
    s_buf_sids = 0;
    remove(p);
}

/* I434j: HW-kind SID register feed. 1/2/3SID x classic/reSID, through the engine's viz frame: sid_chips = SID count,
 * reg/env equal the stub bytes, the sentinel and the stub call count rise; Gate on then off glows 4,3,2,1,0; paused
 * gives no chips and moves neither counter. */
static void viz_sid_test(const char* dir) {
    static const char* const modes[2] = {"6581", "normal"};
    char p[1100], line[1200];
    uint32_t sids, m, k, i;
    snprintf(p, sizeof p, "%s/ps_vizsid.sid", dir);
    for (sids = 1; sids <= 3; sids++) {
        write_psid(p, 1, 1, 0);
        {
            FILE* f = fopen(p, "r+b");
            uint8_t b;
            CHECK(f != NULL);
            if (f == NULL) return;
            b = 4; fseek(f, 5, SEEK_SET); fwrite(&b, 1, 1, f);
            b = sids > 1 ? 0x42 : 0; fseek(f, 0x7a, SEEK_SET); fwrite(&b, 1, 1, f);
            b = sids > 2 ? 0x44 : 0; fseek(f, 0x7b, SEEK_SET); fwrite(&b, 1, 1, f);
            fclose(f);
        }
        for (m = 0; m < 2; m++) {
            sel_engine_config cfg;
            const sel_viz_frame* vf;
            uint32_t fills0, calls0, n;
            uint8_t seq[16];
            uint32_t nseq = 0, last_seq = 0;
            memset(&cfg, 0, sizeof cfg);
            cfg.registry_count = reg_count;
            cfg.registry_get = reg_get;
            s_installed = 1;
            memset(s_sid_regs, 0, sizeof s_sid_regs);
            for (k = 0; k < 3; k++) {
                for (i = 0; i < 25; i++) s_sid_regs[k][i] = (uint8_t)(0x10 * (k + 1) + i);
                s_sid_regs[k][4] = 0x41; /* voice 1: Gate + Pulse */
                for (i = 0; i < 3; i++) s_sid_env[k][i] = (uint8_t)(10 * (k + 1) + i);
            }
            eng = sel_engine_create(sel_host_default(), &cfg);
            CHECK(eng != NULL);
            snprintf(line, sizeof line, "PREF_SET playsid.mode \"%s\"", modes[m]);
            run(line);
            snprintf(line, sizeof line, "QUEUE_ADD %s", p);
            run(line);
            run("PLAY");
            fills0 = sel_playsid_reg_fills;
            calls0 = s_n_readregs;
            tick();
            tick();
            vf = sel_engine_state(eng)->viz;
            CHECK(vf != NULL && vf->seq != 0u);
            CHECK(vf->sid_chips == sids);
            for (k = 0; k < sids; k++) {
                CHECK(memcmp(vf->sid[k].reg, s_sid_regs[k], 25u) == 0);
                CHECK(memcmp(vf->sid[k].env, s_sid_env[k], 3u) == 0);
            }
            CHECK(vf->sid[0].glow[0][0] == 0); /* Gate on */
            CHECK(sel_playsid_reg_fills > fills0 && s_n_readregs > calls0); /* sentinel: the hook ran */
            /* Gate off: glow 4,3,2,1,0 over the next frames */
            s_sid_regs[0][4] = 0x40;
            seq[nseq++] = vf->sid[0].glow[0][0];
            last_seq = vf->seq;
            for (n = 0; n < 60u && nseq < 16u; n++) {
                static int16_t buf[441 * 2];
                sel_engine_render(eng, buf, 441u);
                sel_engine_pump(eng, NULL, NULL);
                vf = sel_engine_state(eng)->viz;
                if (vf->seq != last_seq) {
                    last_seq = vf->seq;
                    if (vf->sid[0].glow[0][0] != seq[nseq - 1]) seq[nseq++] = vf->sid[0].glow[0][0];
                }
            }
            CHECK(nseq >= 6u && seq[0] == 0 && seq[1] == 4 && seq[2] == 3 && seq[3] == 2 && seq[4] == 1 && seq[5] == 0);
            /* paused: no chips, neither counter moves */
            run("PAUSE");
            tick();
            fills0 = sel_playsid_reg_fills;
            calls0 = s_n_readregs;
            tick();
            tick();
            vf = sel_engine_state(eng)->viz;
            CHECK(vf->sid_chips == 0u);
            CHECK(sel_playsid_reg_fills == fills0 && s_n_readregs == calls0);
            run("STOP");
            sel_engine_destroy(eng);
            eng = NULL;
        }
    }
    remove(p);
}

/* MPS-19: classic-mode voice mute through SetChannelEnable, driven from the engine's MUTE / SOLO commands. */
static int en_is(uint32_t idx, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
    return idx < 64u && s_en_log[idx][0] == a && s_en_log[idx][1] == b && s_en_log[idx][2] == c && s_en_log[idx][3] == d;
}
static void mute_test(const char* dir) {
    char p[1100], line[1200];
    sel_engine_config cfg;
    sel_engine_stats st;
    uint32_t sets0, n;
    snprintf(p, sizeof p, "%s/ps_mute.sid", dir);
    write_psid(p, 2, 1, 0);
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    s_installed = 1;
    CHECK(ps_mode_can_mute(PS_OM_NORMAL) == 1);
    CHECK(ps_mode_can_mute(PS_OM_RESID_6581) == 0 && ps_mode_can_mute(PS_OM_USBSID_PICO) == 0);
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    run("PREF_SET playsid.mode \"normal\"");
    snprintf(line, sizeof line, "QUEUE_ADD %s", p);
    run(line);
    s_n_en = 0;
    run("PLAY");
    CHECK(s_n_en == 0u); /* nothing muted yet: no call */
    sets0 = sel_playsid_mute_sets;
    run("MUTE 1 on");
    sel_engine_stats_get(eng, &st);
    CHECK(st.rp_mute_calls == 1u); /* sentinel: the engine's RP_MUTE path ran */
    CHECK(sel_playsid_mute_sets == sets0 + 1u && s_n_en == 1u && en_is(0, 0, 1, 1, 1));
    run("MUTE 4 on"); /* the digi voice */
    CHECK(s_n_en == 2u && en_is(1, 0, 1, 1, 0));
    run("MUTE 4 off");
    CHECK(s_n_en == 3u && en_is(2, 0, 1, 1, 1));
    run("MUTE 1 off");
    CHECK(s_n_en == 4u && en_is(3, 1, 1, 1, 1));
    run("SOLO 2"); /* the engine walks the changed bits one channel at a time; the last call holds the final state */
    CHECK(s_n_en > 4u && en_is(s_n_en - 1u, 0, 1, 0, 0));
    run("SOLO 0");
    CHECK(en_is(s_n_en - 1u, 1, 1, 1, 1));
    run("MUTE 3 on");
    CHECK(en_is(s_n_en - 1u, 1, 1, 0, 1));
    n = s_n_en;
    restart(); /* a subsong restart re-applies the mute */
    CHECK(s_n_en > n && en_is(s_n_en - 1u, 1, 1, 0, 1));
    run("STOP");
    sel_engine_destroy(eng);
    eng = NULL;
    CHECK(en_is(s_n_en - 1u, 1, 1, 1, 1)); /* close restores all channels */

    /* reSID mode: the old library cannot mute, so there is no mute surface and no SetChannelEnable call */
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    run("PREF_SET playsid.mode \"6581\"");
    snprintf(line, sizeof line, "QUEUE_ADD %s", p);
    run(line);
    s_n_en = 0;
    run("PLAY");
    run("MUTE 1 on");
    sel_engine_stats_get(eng, &st);
    CHECK(s_n_en == 0u); /* the adapter never reaches SetChannelEnable in a mode the library cannot mute */
    run("STOP");
    sel_engine_destroy(eng);
    eng = NULL;
    remove(p);
}

/* I434f: the catalogue length ends the song through rp_hw_poll. A fake host clock drives the adapter's play time. */
static uint32_t g_clock;
static uint32_t fake_clock(const sel_host* h) { (void)h; return g_clock; }
extern uint32_t sel_engine_catalogue_lengths; /* sentinel in src/engine/engine.c: a lengths_ms entry was found */

static void tick(void) {
    static int16_t buf[441 * 2];
    sel_engine_render(eng, buf, 441u);
    sel_engine_pump(eng, NULL, NULL);
    sel_engine_render(eng, buf, 441u);
    sel_engine_pump(eng, NULL, NULL);
}

/* Plays the library worker mid-job: owns the call lock until released, so any synchronous library request would block. */
static volatile int g_held, g_release;
static void* lock_holder(void* arg) {
    sel_library* lib = arg;
    pthread_mutex_lock(&lib->call_lock);
    g_held = 1;
    while (!g_release) usleep(1000);
    pthread_mutex_unlock(&lib->call_lock);
    return NULL;
}

static void catalogue_length_test(const char* dir) {
    sel_host host = *sel_host_default();
    sel_engine_config cfg;
    sel_library* lib;
    char dbdir[1100], db[1200], pa[1200], pb[1200], key[1400], line[1500];
    uint32_t cat0, ends0;
    memset(&cfg, 0, sizeof cfg);
    host.clock_ms = fake_clock;
    snprintf(dbdir, sizeof dbdir, "%s/ps_lenXXXXXX", dir);
    CHECK(mkdtemp(dbdir) != NULL);
    snprintf(db, sizeof db, "%s/library.db", dbdir);
    snprintf(pa, sizeof pa, "%s/len_a.sid", dbdir);
    snprintf(pb, sizeof pb, "%s/len_b.sid", dbdir);
    write_psid(pa, 3, 2, 0x2ul);
    write_psid(pb, 3, 2, 0x2ul);
    lib = sel_library_open(sel_host_default(), sel_machine_probe(sel_host_default()), 0, db); /* the library keeps real time */
    CHECK(lib != NULL);
    if (lib == NULL) return;
    /* the HVSC sync's sticker for song A (three subsongs); song B has none */
    CHECK(sel_song_key_of(pa, key, sizeof key));
    CHECK(sel_library_sticker_set(lib, key, "lengths_ms", "3000,5000,7000"));
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    cfg.library = lib;
    s_installed = 1;
    eng = sel_engine_create(&host, &cfg);
    CHECK(eng != NULL);
    g_clock = 1000u;
    snprintf(line, sizeof line, "QUEUE_ADD %s", pa);
    run(line);
    cat0 = sel_engine_catalogue_lengths;
    ends0 = sel_playsid_ends;
    run("PLAY"); /* default subsong 2: the second entry, 5000 ms */
    CHECK(sel_engine_catalogue_lengths == cat0 + 1u); /* sentinel: the engine looked the length up */
    CHECK(g_owner != NULL && g_owner->length_ms == 5000u);
    CHECK(sel_engine_state(eng)->song.duration_ms == 5000u); /* I434f (a): the display duration is the same value as the HW end */
    g_clock = 1000u + 4000u;
    tick();
    CHECK(sel_playsid_ends == ends0 && sel_engine_state(eng)->play == SEL_PLAYING); /* 1 s early: still playing */
    g_clock = 1000u + 5000u;
    tick();
    CHECK(sel_playsid_ends == ends0 + 1u); /* sentinel: rp_hw_poll reported the end */
    CHECK(sel_engine_state(eng)->play == SEL_STOPPED); /* the tune ended at the HVSC length */
    /* subsong 1 has its own entry, and a paused tune does not run out */
    g_clock = 20000u;
    run("PLAY");
    run("SUBSONG 0");
    CHECK(g_owner != NULL && g_owner->length_ms == 3000u);
    CHECK(sel_engine_state(eng)->song.duration_ms == 3000u);
    run("PAUSE");
    g_clock = 20000u + 10000u;
    tick();
    CHECK(sel_playsid_ends == ends0 + 1u); /* paused: the clock is held */
    run("PAUSE");
    g_clock = 30000u + 2000u;
    tick();
    CHECK(sel_playsid_ends == ends0 + 1u && sel_engine_state(eng)->play == SEL_PLAYING);
    g_clock = 30000u + 3000u;
    tick();
    CHECK(sel_playsid_ends == ends0 + 2u && sel_engine_state(eng)->play == SEL_STOPPED);
    /* no sticker: the adapter has no length and never ends the tune by itself */
    run("QUEUE_CLEAR");
    snprintf(line, sizeof line, "QUEUE_ADD %s", pb);
    run(line);
    run("PLAY");
    CHECK(g_owner != NULL && g_owner->length_ms == 0u);
    g_clock = 30000u + 3000u + 100000u;
    tick();
    CHECK(sel_playsid_ends == ends0 + 2u && sel_engine_state(eng)->play == SEL_PLAYING);
    run("STOP");
    /* I434f (b): the library is busy (its worker owns the call lock) while the tune starts. start_subsong must not wait:
     * the length is unknown at first, then the pump's retry applies it and the tune ends at it. */
    {
        pthread_t th;
        char pc[1200];
        uint32_t c0;
        int was;
        snprintf(pc, sizeof pc, "%s/len_c.sid", dbdir);
        write_psid(pc, 2, 1, 0x2ul);
        CHECK(sel_song_key_of(pc, key, sizeof key));
        CHECK(sel_library_sticker_set(lib, key, "lengths_ms", "3500,4000"));
        run("PREF_SET library.smart_downloads \"off\""); /* smart_sync's own library wait is a separate one (I906), not under test */
        run("QUEUE_CLEAR");
        snprintf(line, sizeof line, "QUEUE_ADD %s", pc);
        run(line);
        g_clock = 200000u;
        snprintf(line, sizeof line, "OPEN %s", pc);
        run(line); /* library free: the length is read at once */
        run("PLAY");
        tick();
        tick();
        CHECK(g_owner != NULL && g_owner->length_ms == 3500u && sel_engine_state(eng)->song.duration_ms == 3500u);
        g_held = 0;
        g_release = 0;
        pthread_create(&th, NULL, lock_holder, lib);
        while (!g_held) usleep(1000);
        c0 = sel_engine_catalogue_lengths;
        /* I906g: the song start reads its best source, config and loudness first (posted read, blocked behind the held library), so the
         * whole start waits, like it waits for a free slot, and the retry after the release finds the length. */
        g_run_wait = 0;
        was = sel_library_nowait(1);
        run("SUBSONG 1");
        sel_library_nowait(was);
        g_run_wait = 1;
        tick(); /* the old slot is handed back; the start still waits for its read */
        tick();
        CHECK(sel_engine_catalogue_lengths == c0); /* still busy: the start has not run, nothing applied yet */
        g_release = 1;
        pthread_join(th, NULL);
        g_clock = 200000u + 6000u; /* already past the length when it arrives */
        ends0 = sel_playsid_ends;
        sel_engine_pump_reads(eng, NULL, NULL, 2000u); /* the read lands, the waiting start runs on the next pump */
        tick();
        CHECK(sel_engine_catalogue_lengths == c0 + 1u); /* sentinel: the retry found the length */
        CHECK(g_owner != NULL && g_owner->length_ms == 4000u);
        CHECK(sel_engine_state(eng)->song.duration_ms == 4000u);
        tick();
        CHECK(sel_playsid_ends == ends0 + 1u && sel_engine_state(eng)->play == SEL_STOPPED); /* ended on the poll after */
        remove(pc);
    }
    sel_engine_destroy(eng);
    eng = NULL;
    sel_library_close(lib);
    remove(pa);
    remove(pb);
    remove(db);
    rmdir(dbdir);
}

/* I434h: the reSID CPU gate through the engine's PLAY. A host query stands in for the dialog (SEL_HOSTQ_CONFIRM). */
static uint32_t g_asks, g_answer = 1;
static int confirm_query(const sel_host* h, const sel_tag* ask, sel_tag* answer) {
    if (ask != NULL && ask->tag == SEL_HOSTQ_CONFIRM) {
        g_asks++;
        answer->tag = SEL_HOSTQ_CONFIRM;
        answer->value = g_answer;
        return 1;
    }
    return sel_host_default()->query(h, ask, answer);
}

static void perf_gate_test(const char* dir) {
    sel_host host = *sel_host_default();
    sel_engine_config cfg;
    char path[1200], line[1300];
    uint32_t sids, gates0, asks0, meas0, starts0, subsong_flip = 0;
    uint8_t h[0x7c + 16];
    static const uint32_t limits[3] = {100, 50, 33}; /* reference 100: one SID, halved, divided by three */
    FILE* f;
    memset(&cfg, 0, sizeof cfg);
    host.query = confirm_query;
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    s_installed = 1;
    snprintf(path, sizeof path, "%s/perf.sid", dir);
    for (sids = 1; sids <= 4; sids++) { /* 4 = one SID again, for the Stop case */
        uint32_t n = sids > 3 ? 1u : sids;
        write_psid(path, 2, 1, 0ul);
        if (n > 1u) { /* v4 header with the SID2 (and SID3) addresses */
            f = fopen(path, "rb");
            CHECK(f != NULL);
            if (f == NULL) return;
            CHECK(fread(h, 1, sizeof h, f) == sizeof h);
            fclose(f);
            h[5] = 4; h[0x7a] = 0x42; h[0x7b] = n == 3 ? 0x43 : 0;
            f = fopen(path, "wb");
            fwrite(h, 1, sizeof h, f);
            fclose(f);
        }
        eng = sel_engine_create(&host, &cfg);
        CHECK(eng != NULL);
        run("PREF_SET playsid.mode \"6581\"");
        snprintf(line, sizeof line, "QUEUE_ADD %s", path);
        run(line);
        g_perf_n = 0;
        s_meas_ref = 100;
        gates0 = sel_playsid_perf_gates; asks0 = sel_playsid_perf_asks; meas0 = s_n_meas; starts0 = sel_playsid_starts;
        if (sids <= 3) {
            /* measured one above this SID count's limit: the dialog asks, Continue starts the song */
            s_meas_ms = limits[n - 1] + 1u;
            g_answer = 1;
            run("PLAY");
            CHECK(sel_playsid_perf_gates == gates0 + 1u); /* sentinel: the gate ran through the engine's PLAY */
            CHECK(s_n_meas == meas0 + 1u && sel_playsid_perf_asks == asks0 + 1u);
            CHECK(sel_playsid_starts == starts0 + 1u);
            /* same combination, measured exactly at the limit would not ask: drop the cache and the acceptance */
            g_perf_n = 0;
            s_meas_ms = limits[n - 1];
            asks0 = sel_playsid_perf_asks; gates0 = sel_playsid_perf_gates;
            run(subsong_flip++ & 1u ? "SUBSONG 0" : "SUBSONG 1");
            CHECK(sel_playsid_perf_gates == gates0 + 1u && sel_playsid_perf_asks == asks0);
            /* the benchmark runs once per combination: the next start reuses the measurement */
            meas0 = s_n_meas;
            run(subsong_flip++ & 1u ? "SUBSONG 0" : "SUBSONG 1");
            CHECK(sel_playsid_perf_gates == gates0 + 2u && s_n_meas == meas0);
        } else {
            /* Stop aborts the start: no StartSong, the song does not play */
            int start0 = s_n_start;
            s_meas_ms = 101;
            g_answer = 0;
            run("PLAY");
            CHECK(sel_playsid_perf_gates == gates0 + 1u && sel_playsid_perf_asks == asks0 + 1u);
            CHECK(s_n_start == start0 && sel_playsid_starts == starts0);
            CHECK(sel_engine_state(eng)->play != SEL_PLAYING);
        }
        run("STOP");
        sel_engine_destroy(eng);
    }
    g_answer = 1;
    remove(path);
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
    /* reSID quality prefs -> library arguments */
    {
        uint32_t a, b;
        ps_filter_from_pref("internal", &a, &b); CHECK(a == 1u && b == 0u);
        ps_filter_from_pref("internal+external", &a, &b); CHECK(a == 1u && b == 1u);
        ps_filter_from_pref("off", &a, &b); CHECK(a == 0u && b == 0u);
        ps_filter_from_pref("bogus", &a, &b); CHECK(a == 1u && b == 0u);
        ps_filter_from_pref(NULL, &a, &b); CHECK(a == 1u && b == 0u);
        CHECK(ps_boost_from_pref("off") == 1u && ps_boost_from_pref("2x") == 2u && ps_boost_from_pref("3x") == 3u &&
              ps_boost_from_pref("4x") == 4u && ps_boost_from_pref("bogus") == 1u && ps_boost_from_pref(NULL) == 1u);
        CHECK(ps_oversample_from_pref("normal") == 0u && ps_oversample_from_pref("2x") == 1u && ps_oversample_from_pref("3x") == 2u &&
              ps_oversample_from_pref("4x") == 3u && ps_oversample_from_pref("bogus") == 0u && ps_oversample_from_pref(NULL) == 0u);
    }
    /* output -> AHI mode */
    CHECK(ps_ahi_mode_for_output("ahi", "0x0002000B") == 0x0002000Bu);
    CHECK(ps_ahi_mode_for_output("ahi", "20003") == 0x20003u);
    CHECK(ps_ahi_mode_for_output("ahi", "") == 0u && ps_ahi_mode_for_output("ahi", "zz") == 0u);
    CHECK(ps_ahi_mode_for_output("paula", "0x20003") == 0u && ps_ahi_mode_for_output("paula14", "0x20003") == 0u);
    CHECK(ps_ahi_mode_for_output("", "0x20003") == 0u && ps_ahi_mode_for_output(NULL, NULL) == 0u);

    /* I434d: SID count from the first 0x7C header bytes of three real HVSC files (mono Commando v2, 2SID Sparkster
     * Lakeside v3, 3SID Devils ReSIDence v4), then the label the UI shows for each (rp_open -> rp_metadata ->
     * sel_format_label, the call uimodel makes). */
    {
        static const char* const files[3] = {"mono_Commando.hdr", "2sid_Sparkster_Lakeside.hdr", "3sid_Devils_ReSIDence.hdr"};
        static const char* const want[3] = {"SID (sid)", "SID 2SID", "SID 3SID"};
        uint8_t hb[0x7c];
        uint32_t k;
        t_desc = SEL_PLUGIN_ENTRY(1);
        s_installed = 1;
        for (k = 0; k < 3; k++) {
            FILE* hf;
            sel_song_source src;
            sel_rp_info rinfo;
            void* st;
            char label[64], fpath[1100];
            snprintf(fpath, sizeof fpath, "%s/%s", PLAYSID_FIXTURE_DIR, files[k]);
            hf = fopen(fpath, "rb");
            CHECK(hf != NULL && fread(hb, 1, sizeof hb, hf) == sizeof hb);
            if (hf != NULL) fclose(hf);
            CHECK(ps_sid_count(hb, sizeof hb) == k + 1u);
            memset(&src, 0, sizeof src);
            memset(&rinfo, 0, sizeof rinfo);
            src.data = hb;
            src.size = sizeof hb;
            st = rp_open(&src, sel_host_default(), &rinfo);
            CHECK(st != NULL);
            CHECK(rinfo.voice_count == 4u * (k + 1u)); /* MPS-19: classic mode shows 4 mute voices per chip */
            if (st != NULL) {
                sel_format_label((const char*)sel_tag_find(rp_metadata(st), SEL_META_FORMAT, 0), label, sizeof label);
                CHECK(strcmp(label, want[k]) == 0);
                rp_close(st);
            }
        }
        CHECK(ps_sid_count(NULL, 0) == 1u && ps_sid_count(hb, 10) == 1u);
        hb[5] = 3; hb[0x7b] = 0x44; /* v3 ignores a SID3 byte */
        CHECK(ps_sid_count(hb, sizeof hb) == 2u);
        hb[4] = 0; hb[5] = 2; hb[0x7a] = 0x42; /* v2 has no extra SIDs */
        CHECK(ps_sid_count(hb, sizeof hb) == 1u);
        s_installed = 0;
        CHECK(s_open_refs == 0);
    }

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

    CHECK(s_n_filt == 0 && s_n_boost == 0 && s_n_over == 0); /* classic mode: no reSID call */

    run("PAUSE");
    CHECK(s_n_pause >= 1);
    run("PAUSE");
    CHECK(s_n_cont >= 1);

    /* I434e: classic mode is seekable and the song says so through the state the UI model reads */
    CHECK(sel_engine_state(eng)->song.seekable == 1u);
    {
        int fwd0 = s_n_fwd;
        uint32_t seeks0 = sel_playsid_seeks;
        run("SEEK 5000");
        CHECK(s_n_fwd == fwd0 + 5 && sel_playsid_seeks == seeks0 + 5u); /* sentinel: 5 ForwardSong calls for 5 s */
    }
    CHECK(ps_forward_steps(0) == 0u && ps_forward_steps(499) == 0u && ps_forward_steps(500) == 1u &&
          ps_forward_steps(1000) == 1u && ps_forward_steps(5000) == 5u && ps_forward_steps(0xFFFFFFFFu) == PS_FORWARD_MAX_STEPS);
    CHECK(ps_mode_seekable(PS_OM_NORMAL) && !ps_mode_seekable(PS_OM_RESID_6581) && !ps_mode_seekable(PS_OM_RESID_8580) &&
          !ps_mode_seekable(PS_OM_RESID_AUTO) && !ps_mode_seekable(PS_OM_TRINITY));

    run("STOP");

    /* reSID mode through the engine prefs: defaults first, then every value moves the logged argument */
    run("PREF_SET playsid.mode \"6581\"");
    restart();
    CHECK(s_last_om == 1u && s_n_filt == 1 && s_n_boost == 1 && s_n_over == 1);
    CHECK(s_filt_in == 1u && s_filt_ext == 0u && s_boost == 1u && s_over == 0u); /* Hippo defaults: internal, off, normal */
    {   /* I434e: reSID mode is not seekable: RP_SEEK answers 0 (ForwardSong never called) and the state greys the bar */
        int fwd0 = s_n_fwd;
        uint32_t seeks0 = sel_playsid_seeks;
        CHECK(sel_engine_state(eng)->song.seekable == 0u);
        run("SEEK 5000");
        CHECK(s_n_fwd == fwd0 && sel_playsid_seeks == seeks0);
        CHECK(sel_engine_state(eng)->song.seekable == 0u);
    }
    {
        static const char* const filt[3] = {"internal", "internal+external", "off"};
        static const uint32_t fin[3] = {1, 1, 0}, fext[3] = {0, 1, 0};
        static const char* const lvl[4] = {"off", "2x", "3x", "4x"};
        static const char* const ovl[4] = {"normal", "2x", "3x", "4x"};
        for (i = 0; i < 3; i++) {
            snprintf(line, sizeof line, "PREF_SET playsid.filter \"%s\"", filt[i]);
            run(line);
            restart();
            CHECK(s_filt_in == fin[i] && s_filt_ext == fext[i]);
        }
        run("PREF_SET playsid.filter \"internal\"");
        for (i = 0; i < 4; i++) {
            snprintf(line, sizeof line, "PREF_SET playsid.boost \"%s\"", lvl[i]);
            run(line);
            restart();
            CHECK(s_boost == i + 1u);
        }
        run("PREF_SET playsid.boost \"off\"");
        for (i = 0; i < 4; i++) {
            snprintf(line, sizeof line, "PREF_SET playsid.oversample \"%s\"", ovl[i]);
            run(line);
            restart();
            CHECK(s_over == i);
        }
    }
    run("STOP");

    sel_engine_destroy(eng);
    CHECK(g_owner == NULL && s_n_stop >= 1 && s_n_free >= 1); /* released by STOP or at the latest on close */
    CHECK(s_open_refs == 0); /* rp_close released the library */
    remove(path);
    catalogue_length_test(argv[1]);
    viz_scope_test(argv[1]);
    viz_sid_test(argv[1]);
    mute_test(argv[1]);
    perf_gate_test(argv[1]);
    if (g_fail != 0) {
        printf("playsid: %d failures\n", g_fail);
        return 1;
    }
    printf("playsid: ok\n");
    return 0;
}
