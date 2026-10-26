/* text-ui-keys-wait (T19 companion): the text UI plugin's keys-file mode (plan M1.1 phase 5,
 * row I60 fix). A `wait <ms>` line must end after that much shell time under
 * both shell pacings: a clockless output (the shell passes the turn length
 * as UI_WAIT's timeout, the run stays deterministic) and a device-clocked
 * output (the shell passes 0 and the device paces the turn).
 * Usage: text_ui_test <scratch dir> */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/registry.h>
#include <selecta/ui.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

static int failures;

#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

#define WAIT_MS 300u
#define TURN_MS 20u

typedef struct ui_hooks {
    sel_ui_open_fn open;
    sel_ui_wait_fn wait;
    sel_ui_close_fn close;
} ui_hooks;

static const char* dump_path; /* --screen-dump target, "" when the run needs no dump */

static void* open_ui(const sel_host* host, sel_engine* e, const ui_hooks* h, const char* keys) {
    static const char* argv[6];
    sel_ui_open_args a;
    argv[0] = "text_ui_test";
    argv[1] = "--keys";
    argv[2] = keys;
    argv[3] = NULL;
    if (dump_path != NULL) {
        argv[3] = "--screen-dump";
        argv[4] = dump_path;
        argv[5] = NULL;
    }
    memset(&a, 0, sizeof a);
    a.engine = e;
    a.host = host;
    a.argc = dump_path != NULL ? 5 : 3;
    a.argv = argv;
    return h->open(&a);
}

/* Runs the UI until it quits; returns the number of UI_WAIT calls, or 0 when
 * it had not quit after `cap_ms` of wall time. Each turn with timeout 0
 * sleeps one shell turn, as a device-clocked out_pump does. */
static uint32_t run(const sel_host* host, sel_engine* e, const ui_hooks* h, void* ui, uint32_t timeout,
                    uint32_t cap_ms, uint32_t* elapsed_ms) {
    struct timespec turn = {0, (long)TURN_MS * 1000000L};
    uint32_t start = host->clock_ms(host);
    uint32_t calls = 0;
    for (;;) {
        int r = h->wait(ui, timeout);
        calls++;
        sel_engine_pump(e, NULL, NULL);
        *elapsed_ms = host->clock_ms(host) - start;
        if (r < 0) {
            return calls;
        }
        if (*elapsed_ms > cap_ms) {
            return 0;
        }
        if (timeout == 0) {
            nanosleep(&turn, NULL);
        }
    }
}

/* Reads the last screen dump (24 rows) into buf; returns the byte count. */
static size_t read_dump(char* buf, size_t cap) {
    FILE* f = fopen(dump_path, "rb");
    size_t n = 0;
    if (f != NULL) {
        n = fread(buf, 1, cap - 1u, f);
        fclose(f);
    }
    buf[n] = '\0';
    return n;
}

/* Row 24 of a dump, or NULL. */
static const char* last_row(const char* dump) {
    const char* p = dump;
    int row;
    for (row = 1; row < 24 && p != NULL; row++) {
        p = strchr(p, '\n');
        p = p != NULL ? p + 1 : NULL;
    }
    return p;
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const sel_tag* desc;
    const sel_tag* tags;
    sel_engine_config cfg;
    sel_engine* e;
    ui_hooks h;
    void* ui;
    char keys[1024];
    FILE* f;
    uint32_t calls, elapsed = 0;
    if (argc < 2) {
        fputs("usage: text_ui_test <scratch dir>\n", stderr);
        return 2;
    }
    snprintf(keys, sizeof keys, "%s/text_ui_wait.keys", argv[1]);
    f = fopen(keys, "wb");
    if (f == NULL) {
        return 2;
    }
    fprintf(f, "wait %u\nq\n", WAIT_MS);
    fclose(f);

    sel_registry_init(host);
    desc = sel_registry_find(SEL_PLUGIN_KIND_UI, "text");
    tags = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    h.open = (sel_ui_open_fn)sel_tag_find(tags, SEL_UI_OPEN, 0);
    h.wait = (sel_ui_wait_fn)sel_tag_find(tags, SEL_UI_WAIT, 0);
    h.close = (sel_ui_close_fn)sel_tag_find(tags, SEL_UI_CLOSE, 0);
    if (h.open == NULL || h.wait == NULL || h.close == NULL) {
        puts("FAIL the text UI plugin is not registered");
        return 1;
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        return 1;
    }

    /* Clockless shell: the wait is counted in turns, exactly. One call reads
     * the wait line, WAIT_MS / TURN_MS calls burn it, one reads `q`. */
    ui = open_ui(host, e, &h, keys);
    CHECK(ui != NULL, "open failed");
    if (ui != NULL) {
        calls = run(host, e, &h, ui, TURN_MS, 5000u, &elapsed);
        CHECK(calls == 1u + WAIT_MS / TURN_MS + 1u, "clockless: %u UI_WAIT calls, want %u", (unsigned)calls,
              (unsigned)(1u + WAIT_MS / TURN_MS + 1u));
        h.close(ui);
    }

    /* Device-clocked shell: timeout 0, the turn takes wall time. The wait
     * must end (it hung before the fix) and must not end early. */
    ui = open_ui(host, e, &h, keys);
    CHECK(ui != NULL, "open failed");
    if (ui != NULL) {
        calls = run(host, e, &h, ui, 0u, 3000u, &elapsed);
        CHECK(calls != 0u, "clocked: the keys-file wait never ended (%u ms)", (unsigned)elapsed);
        CHECK(calls == 0u || elapsed >= WAIT_MS, "clocked: quit after %u ms, before the %u ms wait",
              (unsigned)elapsed, (unsigned)WAIT_MS);
        h.close(ui);
    }

    /* I355: with no song and no --exec the first frame is the start screen; any key ends it and is handled as
     * usual (Space runs its normal binding), and the header row returns. */
    {
        static char dump[8192], keys2[1024], dpath[1024];
        const char* row24;
        uint32_t i;
        snprintf(keys2, sizeof keys2, "%s/text_ui_splash.keys", argv[1]);
        snprintf(dpath, sizeof dpath, "%s/text_ui_splash.dump", argv[1]);
        f = fopen(keys2, "wb");
        if (f == NULL) {
            return 2;
        }
        fprintf(f, "wait 60\nSpace\nwait 60\nq\n");
        fclose(f);
        remove(dpath);
        dump_path = dpath;
        ui = open_ui(host, e, &h, keys2);
        CHECK(ui != NULL, "splash: open failed");
        if (ui != NULL) {
            int r = h.wait(ui, TURN_MS); /* the first frame */
            read_dump(dump, sizeof dump);
            row24 = last_row(dump);
            CHECK(row24 != NULL && strstr(row24, "sPACE t0 sTART!") != NULL, "splash: row 24 of the first frame is '%s'",
                  row24 != NULL ? row24 : "(none)");
            CHECK(strstr(dump, "Subsong") == NULL, "splash: the player header shows on the first frame");
            for (i = 0; r >= 0 && i < 40u; i++) {
                r = h.wait(ui, TURN_MS);
                read_dump(dump, sizeof dump);
                if (strstr(dump, "sPACE t0 sTART!") == NULL) {
                    break;
                }
            }
            CHECK(strstr(dump, "sPACE t0 sTART!") == NULL, "splash: still shown after Space");
            CHECK(strstr(dump, "Subsong") != NULL, "splash: the header row did not return after Space");
            h.close(ui);
        }
        dump_path = NULL;
    }

    sel_engine_destroy(e);
    printf("%s text-ui-keys-wait\n", failures ? "FAIL" : "ok");
    return failures ? 1 : 0;
}
