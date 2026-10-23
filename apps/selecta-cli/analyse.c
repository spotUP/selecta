/* `analyse` and `library analyse` (I187, design doc 6a): loudness analysis through the engine's ANALYSE
 * command, which renders the song faster than real time into the R128 analyser and stores the gain per song
 * key in the library DB. `analyse --sine <hz>:<dbfs>` feeds a generated stereo sine through the same
 * analyser (the reference signals of T89; no replayer plays a plain waveform). */
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/registry.h>

#include "cli.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(SEL_NO_ANALYSE)
#include <math.h>

#include "loudness.h"
#endif

typedef struct analyse_sink {
    int failed;
    unsigned count;
} analyse_sink;

static void on_event(void* ctx, const sel_message* evt) {
    analyse_sink* s = (analyse_sink*)ctx;

    if (evt->id == SEL_EVT_ERROR) {
        printf("ERROR %s %s\n", evt->args[0].v.s, evt->argc > 1 ? evt->args[1].v.s : "");
        s->failed = 1;
    } else if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        s->count = (unsigned)sel_tag_find(evt->args[0].v.t, SEL_SRC_COUNT, 0);
    }
}

static void centi_print(const char* name, int32_t cdb) {
    int32_t a = cdb < 0 ? -cdb : cdb;

    printf("%s=%s%d.%02d ", name, cdb < 0 ? "-" : "", (int)(a / 100), (int)(a % 100));
}

static void print_stats(sel_engine* e) {
    sel_engine_stats st;

    sel_engine_stats_get(e, &st);
    printf("analyse_jobs=%u analyse_speed=%u gated=%u ", (unsigned)st.analyse_jobs, (unsigned)st.analyse_speed,
           (unsigned)st.analyse_gated);
    centi_print("lufs", st.analyse_lufs_cdb);
    centi_print("peak_db", st.analyse_peak_cdb);
    centi_print("gain_db", st.analyse_gain_cdb);
    printf("gain_cdb=%d\n", (int)st.analyse_gain_cdb);
}

static sel_engine* open_engine(const sel_host* host, const char* db_path, sel_library** lib_out) {
    sel_engine_config cfg;
    sel_engine* e;

    memset(&cfg, 0, sizeof cfg);
    sel_registry_init(host);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = sel_machine_probe(host);
    *lib_out = sel_library_open(host, cfg.machine, 0, db_path);
    if (*lib_out == NULL) {
        puts("db_open=0");
        return NULL;
    }
    cfg.library = *lib_out;
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        fputs("analyse: engine allocation failed\n", stderr);
        sel_library_close(*lib_out);
    }
    return e;
}

static int send_analyse(sel_engine* e, const char* song, analyse_sink* sink) {
    sel_message msg;

    memset(&msg, 0, sizeof msg);
    msg.kind = SEL_MSG_CMD;
    msg.id = SEL_CMD_ANALYSE;
    msg.argc = 1;
    msg.args[0].type = SEL_ARG_STR;
    msg.args[0].v.s = song;
    sel_engine_send(e, &msg);
    sel_engine_pump(e, on_event, sink);
    return !sink->failed;
}

#if !defined(SEL_NO_ANALYSE)
/* analyse --sine <hz>:<dbfs> [--seconds n]: a stereo sine of peak `dbfs` through the analyser. */
static int analyse_sine(const char* spec, unsigned seconds) {
    const unsigned rate = 44100;
    double hz = atof(spec), db = 0.0, amp;
    const char* colon = strchr(spec, ':');
    sel_loudness* an;
    sel_loudness_result r;
    double buf[2 * 4096];
    unsigned long total = (unsigned long)rate * seconds, done = 0;
    clock_t t0;
    double cpu, speed;

    if (colon == NULL || hz <= 0.0) {
        fputs("usage: selecta-cli analyse --sine <hz>:<dbfs> [--seconds <n>]\n", stderr);
        return 2;
    }
    db = atof(colon + 1);
    amp = pow(10.0, db / 20.0);
    an = sel_loudness_new(rate);
    if (an == NULL) return 1;
    t0 = clock();
    while (done < total) {
        unsigned n = total - done > 4096 ? 4096 : (unsigned)(total - done), i;
        for (i = 0; i < n; i++) {
            double v = amp * sin(2.0 * M_PI * hz * (double)(done + i) / (double)rate);
            buf[2 * i] = v;
            buf[2 * i + 1] = v;
        }
        sel_loudness_add_f64(an, buf, n);
        done += n;
    }
    cpu = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    sel_loudness_result_get(an, &r);
    sel_loudness_free(an);
    speed = cpu > 1e-6 ? (double)seconds / cpu : 1000000.0;
    printf("analyse_jobs=1 analyse_speed=%u gated=%d lufs=%.3f rms_db=%.3f peak_db=%.3f gain_db=%.3f gain_cdb=%d\n",
           speed > 1000000.0 ? 1000000u : (unsigned)speed, r.gated, r.lufs, r.fallback_db, r.peak_db, r.gain_db,
           (int)r.gain_cdb);
    return 0;
}
#endif

int sel_cli_analyse(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = ":memory:";
    const char* song = NULL;
    const char* sine = NULL;
    unsigned seconds = 20;
    analyse_sink sink;
    sel_library* lib;
    sel_engine* e;
    char key[1100];
    int32_t stored = 0, have;
    int i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--sine") == 0 && i + 1 < argc) {
            sine = argv[++i];
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = (unsigned)strtoul(argv[++i], NULL, 10);
        } else if (song == NULL && argv[i][0] != '-') {
            song = argv[i];
        } else {
            song = NULL;
            sine = NULL;
            break;
        }
    }
    if (sine != NULL) {
#if !defined(SEL_NO_ANALYSE)
        return analyse_sine(sine, seconds < 1 ? 1 : seconds);
#else
        fputs("analyse: desktop only\n", stderr);
        return 1;
#endif
    }
    if (song == NULL) {
        fputs("usage: selecta-cli analyse [--db <path>] <song> | analyse --sine <hz>:<dbfs> [--seconds <n>]\n", stderr);
        return 2;
    }
    memset(&sink, 0, sizeof sink);
    e = open_engine(host, db_path, &lib);
    if (e == NULL) return 1;
    send_analyse(e, song, &sink);
    if (!sink.failed) {
        print_stats(e);
        snprintf(key, sizeof key, strncmp(song, "local:", 6) == 0 ? "%s" : "local:%s", song);
        have = sel_library_loudness_get(lib, key, &stored);
        printf("stored=%d stored_gain_cdb=%d key=%s\n", have, (int)stored, key);
    }
    sel_engine_destroy(e);
    sel_library_close(lib);
    return sink.failed ? 1 : 0;
}

/* library analyse --db <path> [<query>]: ANALYSE for every song the query finds (default source:local). */
int sel_cli_library_analyse(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = NULL;
    const char* query = "source:local";
    analyse_sink sink;
    sel_library* lib;
    sel_engine* e;
    char line[1200], pool[1200];
    unsigned i, ok = 0, bad = 0;
    int a;

    for (a = 0; a < argc; a++) {
        if (strcmp(argv[a], "--db") == 0 && a + 1 < argc) {
            db_path = argv[++a];
        } else if (argv[a][0] != '-') {
            query = argv[a];
        }
    }
    if (db_path == NULL) {
        fputs("usage: selecta-cli library analyse --db <path> ['source:local']\n", stderr);
        return 2;
    }
    memset(&sink, 0, sizeof sink);
    e = open_engine(host, db_path, &lib);
    if (e == NULL) return 1;
    snprintf(line, sizeof line, "LIBRARY_SEARCH %s", query);
    sel_engine_send_line(e, line, pool, sizeof pool);
    sel_engine_pump(e, on_event, &sink);
    for (i = 0; !sink.failed && i < sink.count; i++) {
        analyse_sink one;
        int32_t gain = 0;
        const char* key = sel_engine_result_key(e, i);

        memset(&one, 0, sizeof one);
        if (send_analyse(e, key, &one) && sel_library_loudness_get(lib, key, &gain)) {
            printf("key=%s gain_cdb=%d\n", key, (int)gain);
            ok++;
        } else {
            bad++;
        }
    }
    printf("analysed=%u failed=%u ", ok, bad);
    print_stats(e);
    sel_engine_destroy(e);
    sel_library_close(lib);
    return sink.failed || bad > 0 ? 1 : 0;
}
