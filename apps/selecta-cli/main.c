/* selecta-cli: the top-level entry point every M0 test drives (design doc
 * section 12). Subcommands exercise the core through the same calls a
 * front end makes. */
#include <selecta/api.h>
#include <selecta/protocol.h>
#include <selecta/tags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(FILE* to) {
    fputs("usage: selecta-cli <command> [args]\n"
          "  version                    print version and api level\n"
          "  commands                   list command verbs and signatures\n"
          "  events                     list event verbs and signatures\n"
          "  tagtypes                   list metadata tags and their types\n"
          "  roundtrip                  encode and decode every table row\n",
          to);
    return to == stdout ? 0 : 2;
}

static int cmd_version(void) {
    printf("selecta-cli %s\n", sel_version_string());
    return 0;
}

static int cmd_commands(void) {
    uint32_t i;
    for (i = 0; i < SEL_CMD_COUNT; i++) {
        printf("%s %s\n", sel_cmd_name(i), sel_cmd_args(i));
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
        return cmd_commands();
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
    fprintf(stderr, "selecta-cli: unknown command %s\n", cmd);
    return usage(stderr);
}
