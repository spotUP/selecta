/* selecta-cli: the top-level entry point every M0 test drives (design doc
 * section 12). Subcommands exercise the core through the same calls a
 * front end makes. */
#include <selecta/api.h>
#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <selecta/tags.h>

#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(FILE* to) {
    fputs("usage: selecta-cli <command> [args]\n"
          "  version                    print version and api level\n"
          "  commands                   list command verbs and signatures\n"
          "  events                     list event verbs and signatures\n"
          "  tagtypes                   list metadata tags and their types\n"
          "  roundtrip                  encode and decode every table row\n"
          "  commands --unimplemented   list verbs the engine answers not_implemented\n"
          "  script <file>              feed command lines to the engine, print events\n"
          "  abi [voices]               print sel_host (or sel_voices) size and field offsets\n"
          "  plugins [--lint]           list plugins; --lint counts bad descriptor tags\n"
          "  play-null --rate <hz> --frames <n>  pump the null output, print counters\n"
          "  play <song> --output <id> --seconds <n>  play through a real output, print counters\n"
          "  render <song> --output <wav> --seconds <n> --rate <hz> --resample <name>\n"
          "                             render a song to a WAV file, print counters\n"
          "  machine                    probe the machine, print the sel_machine tags\n",
          to);
    return to == stdout ? 0 : 2;
}

static int cmd_version(void) {
    printf("selecta-cli %s\n", sel_version_string());
    return 0;
}

static int cmd_commands(int unimplemented_only) {
    uint32_t i;
    for (i = 0; i < SEL_CMD_COUNT; i++) {
        if (unimplemented_only) {
            if (!sel_engine_cmd_implemented(i)) {
                printf("%s\n", sel_cmd_name(i));
            }
        } else {
            printf("%s %s\n", sel_cmd_name(i), sel_cmd_args(i));
        }
    }
    return 0;
}

static int cmd_events(void) {
    uint32_t i;
    for (i = 0; i < SEL_EVT_COUNT; i++) {
        printf("%s %s\n", sel_evt_name(i), sel_evt_args(i));
    }
    return 0;
}

static const char* tag_type_name(sel_tag_type t) {
    switch (t) {
    case SEL_TT_INT:
        return "int";
    case SEL_TT_STR:
        return "string";
    case SEL_TT_PTR:
        return "pointer";
    case SEL_TT_LIST:
        return "list";
    default:
        return "unknown";
    }
}

static int cmd_tagtypes(void) {
    uint32_t count, i;
    const sel_tag_info* table = sel_tag_table(&count);
    for (i = 0; i < count; i++) {
        if ((table[i].tag & 0xF000u) == 0x8000u) {
            printf("%s %s\n", table[i].name, tag_type_name(table[i].type));
        }
    }
    return 0;
}

/* Builds a sample message for a signature, so every letter and the
 * optional and repeat markers are exercised. */
static const sel_tag k_sample_tags[] = {
    {SEL_META_TITLE, (uintptr_t) "Space Debris"},
    {SEL_META_AUTHOR, (uintptr_t) "Captain"},
    {SEL_META_YEAR, 1991},
    {SEL_META_MESSAGE, (uintptr_t) "say \"hi\", {ok}"},
    {SEL_TAG_END, 0},
};

static void fill_sample(sel_message* m, uint32_t kind, uint32_t id, const char* sig) {
    size_t n = strlen(sig);
    size_t i;
    int repeat = 0;
    memset(m, 0, sizeof *m);
    m->kind = kind;
    m->id = id;
    if (n && sig[n - 1] == '?') {
        n--;
    } else if (n && sig[n - 1] == '*') {
        n--;
        repeat = 1;
    }
    for (i = 0; i < n + (repeat ? 2u : 0u) && i < SEL_MAX_ARGS; i++) {
        char letter = i < n ? sig[i] : sig[n - 1];
        sel_arg* a = &m->args[i];
        a->type = (uint8_t)letter;
        switch (letter) {
        case 's':
            a->v.s = (i % 2) ? "plain" : "with space \"and\" quote";
            break;
        case 'i':
            a->v.i = (i % 2) ? 7 : -5;
            a->flags = (i % 2) ? 0 : SEL_ARG_RELATIVE;
            break;
        case 'u':
            a->v.u = 42u + (uint32_t)i;
            break;
        case 't':
            a->v.t = k_sample_tags;
            break;
        default:
            break;
        }
        m->argc++;
    }
}

static int tags_equal(const sel_tag* a, const sel_tag* b) {
    const sel_tag* ia = a;
    const sel_tag* ib = b;
    for (;;) {
        const sel_tag* x = sel_tag_next(&ia);
        const sel_tag* y = sel_tag_next(&ib);
        sel_tag_type type = SEL_TT_PTR;
        if (x == NULL || y == NULL) {
            return x == y;
        }
        if (x->tag != y->tag) {
            return 0;
        }
        sel_tag_type_of(x->tag, &type);
        if (type == SEL_TT_STR) {
            if (strcmp((const char*)x->value, (const char*)y->value) != 0) {
                return 0;
            }
        } else if (x->value != y->value) {
            return 0;
        }
    }
}

static int messages_equal(const sel_message* a, const sel_message* b) {
    uint32_t i;
    if (a->kind != b->kind || a->id != b->id || a->argc != b->argc) {
        return 0;
    }
    for (i = 0; i < a->argc; i++) {
        const sel_arg* x = &a->args[i];
        const sel_arg* y = &b->args[i];
        if (x->type != y->type) {
            return 0;
        }
        switch (x->type) {
        case SEL_ARG_STR:
            if (strcmp(x->v.s, y->v.s) != 0) {
                return 0;
            }
            break;
        case SEL_ARG_INT:
            if (x->v.i != y->v.i || x->flags != y->flags) {
                return 0;
            }
            break;
        case SEL_ARG_UINT:
            if (x->v.u != y->v.u) {
                return 0;
            }
            break;
        case SEL_ARG_TAGS:
            if (!tags_equal(x->v.t, y->v.t)) {
                return 0;
            }
            break;
        default:
            return 0;
        }
    }
    return 1;
}

static int roundtrip_one(uint32_t kind, uint32_t id, const char* name, const char* sig) {
    sel_message in, back;
    char line[512], again[512];
    char pool[1024];
    int n, rc;
    fill_sample(&in, kind, id, sig);
    n = sel_codec_encode(&in, line, sizeof line);
    if (n < 0) {
        fprintf(stderr, "[ERROR] %s: encode failed\n", name);
        return 0;
    }
    rc = sel_codec_decode(line, kind, &back, pool, sizeof pool);
    if (rc != SEL_CODEC_OK) {
        fprintf(stderr, "[ERROR] %s: decode %s on: %s\n", name, sel_codec_strerror(rc), line);
        return 0;
    }
    if (!messages_equal(&in, &back)) {
        fprintf(stderr, "[ERROR] %s: decoded message differs: %s\n", name, line);
        return 0;
    }
    if (sel_codec_encode(&back, again, sizeof again) < 0 || strcmp(line, again) != 0) {
        fprintf(stderr, "[ERROR] %s: re-encode differs:\n  %s\n  %s\n", name, line, again);
        return 0;
    }
    return 1;
}

static int cmd_roundtrip(void) {
    uint32_t i, ok = 0;
    for (i = 0; i < SEL_CMD_COUNT; i++) {
        ok += (uint32_t)roundtrip_one(SEL_MSG_CMD, i, sel_cmd_name(i), sel_cmd_args(i));
    }
    for (i = 0; i < SEL_EVT_COUNT; i++) {
        ok += (uint32_t)roundtrip_one(SEL_MSG_EVT, i, sel_evt_name(i), sel_evt_args(i));
    }
    printf("commands=%u events=%u ok=%u\n", (unsigned)SEL_CMD_COUNT, (unsigned)SEL_EVT_COUNT,
           (unsigned)ok);
    return ok == SEL_CMD_COUNT + SEL_EVT_COUNT ? 0 : 1;
}

/* Prints one event as the wire line the codec produces. */
static void print_event(void* ctx, const sel_message* evt) {
    char line[1024];
    (void)ctx;
    if (sel_codec_encode(evt, line, sizeof line) < 0) {
        fprintf(stderr, "selecta-cli: event %u does not encode\n", (unsigned)evt->id);
        return;
    }
    printf("%s\n", line);
}

/* Script directives (lines starting with '@') are not protocol verbs; they let
 * a script observe and drive what a front end and an audio thread would:
 *   @queue              QUEUE version=V playing=ID ids=a,b,c
 *   @render N           render N frames on this thread, then pump and print
 *   @render_until_end   render until SONG_ENDED is pumped; POSITION events
 *                       before it are not printed (their count depends on
 *                       the song length) */
#define SCRIPT_RENDER_CHUNK 4096u
#define SCRIPT_RENDER_LIMIT 100000u

typedef struct script_sink {
    int quiet_position; /* drop POSITION until SONG_ENDED */
    int ended;
} script_sink;

static void script_event(void* ctx, const sel_message* evt) {
    script_sink* s = ctx;
    if (s->quiet_position && !s->ended && evt->id == SEL_EVT_POSITION) {
        return;
    }
    if (evt->id == SEL_EVT_SONG_ENDED) {
        s->ended = 1;
    }
    print_event(NULL, evt);
}

static void script_render(sel_engine* e, uint32_t frames) {
    static int16_t buf[SCRIPT_RENDER_CHUNK * 2u];
    while (frames > 0) {
        uint32_t n = frames < SCRIPT_RENDER_CHUNK ? frames : SCRIPT_RENDER_CHUNK;
        sel_engine_render(e, buf, n);
        frames -= n;
    }
}

static void script_directive(sel_engine* e, const char* line) {
    unsigned long n = 0;
    script_sink sink = {0, 0};
    if (strcmp(line, "@queue") == 0) {
        uint32_t i;
        const sel_state* st = sel_engine_state(e);
        printf("QUEUE version=%u playing=%u ids=", (unsigned)st->queue_version, (unsigned)st->queue_playing);
        for (i = 0; i < st->queue_count; i++) {
            printf(i ? ",%u" : "%u", (unsigned)st->queue[i].id);
        }
        printf("\n");
    } else if (sscanf(line, "@render %lu", &n) == 1) {
        script_render(e, (uint32_t)n);
        sel_engine_pump(e, script_event, &sink);
    } else if (strcmp(line, "@render_until_end") == 0) {
        uint32_t i;
        sink.quiet_position = 1;
        for (i = 0; i < SCRIPT_RENDER_LIMIT && !sink.ended; i++) {
            script_render(e, SCRIPT_RENDER_CHUNK);
            sel_engine_pump(e, script_event, &sink);
        }
        if (!sink.ended) {
            printf("ERROR script_timeout @render_until_end\n");
        }
    } else {
        fprintf(stderr, "selecta-cli: unknown directive %s\n", line);
    }
}

/* T3: every line of the file goes through codec, queue, dispatch and back
 * out as events. One pump per line so the decode pool can be reused. */
static int cmd_script(const char* path) {
    char line[1024];
    char pool[2048];
    char verb[64];
    FILE* f = fopen(path, "r");
    sel_engine_config cfg;
    sel_engine* e;
    memset(&cfg, 0, sizeof cfg);
    if (f == NULL) {
        fprintf(stderr, "selecta-cli: cannot open %s\n", path);
        return 1;
    }
    sel_registry_init(sel_host_default());
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = sel_machine_probe(sel_host_default());
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL) {
        fclose(f);
        fprintf(stderr, "selecta-cli: engine allocation failed\n");
        return 1;
    }
    while (fgets(line, sizeof line, f)) {
        sel_message m;
        int err;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (line[0] == '@') {
            script_directive(e, line);
            continue;
        }
        err = sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool);
        if (err == SEL_CODEC_OK) {
            if (!sel_engine_send(e, &m)) {
                sel_engine_report_error(e, "queue_full", NULL);
            }
        } else {
            sel_codec_verb(line, verb, sizeof verb);
            sel_engine_report_error(e, err == SEL_CODEC_E_UNKNOWN_VERB ? "unknown_command"
                                                                      : sel_codec_strerror(err),
                                    verb);
        }
        sel_engine_pump(e, print_event, NULL);
    }
    fclose(f);
    sel_engine_destroy(e);
    return 0;
}

int main(int argc, char** argv) {
    const char* cmd;
    if (argc < 2) {
        return usage(stderr);
    }
    cmd = argv[1];
    if (strcmp(cmd, "version") == 0) {
        return cmd_version();
    }
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0) {
        return usage(stdout);
    }
    if (strcmp(cmd, "commands") == 0) {
        if (argc > 2 && strcmp(argv[2], "--unimplemented") == 0) {
            return cmd_commands(1);
        }
        return cmd_commands(0);
    }
    if (strcmp(cmd, "script") == 0) {
        if (argc < 3) {
            return usage(stderr);
        }
        return cmd_script(argv[2]);
    }
    if (strcmp(cmd, "events") == 0) {
        return cmd_events();
    }
    if (strcmp(cmd, "tagtypes") == 0) {
        return cmd_tagtypes();
    }
    if (strcmp(cmd, "roundtrip") == 0) {
        return cmd_roundtrip();
    }
    if (strcmp(cmd, "abi") == 0) {
        return sel_cli_abi(argc > 2 ? argv[2] : NULL);
    }
    if (strcmp(cmd, "plugins") == 0) {
        return sel_cli_plugins(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "machine") == 0) {
        return sel_cli_machine();
    }
    if (strcmp(cmd, "play-null") == 0) {
        return sel_cli_play_null(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "play") == 0) {
        return sel_cli_play(argc - 2, argv + 2);
    }
    if (strcmp(cmd, "render") == 0) {
        return sel_cli_render(argc - 2, argv + 2);
    }
    fprintf(stderr, "selecta-cli: unknown command %s\n", cmd);
    return usage(stderr);
}
