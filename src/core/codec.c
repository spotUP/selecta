/* Text codec for the remote protocol (design doc section 8). One line per
 * message: VERB arg arg ... Argument types come from the signature strings
 * in commands.def and events.def. No allocation: decode copies strings and
 * tag lists into the caller's pool. */
#include <selecta/protocol.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sel_msg_info {
    const char* name;
    const char* args;
    const char* area; /* commands only: the commands.def section; NULL for events */
} sel_msg_info;

static const sel_msg_info k_cmds[] = {
#define SEL_CMD(id, args, area) {#id, args, area},
#include <selecta/commands.def>
#undef SEL_CMD
};

static const sel_msg_info k_evts[] = {
#define SEL_EVT(id, args) {#id, args, NULL},
#include <selecta/events.def>
#undef SEL_EVT
};

static const sel_msg_info* table_of(uint32_t kind, uint32_t* count) {
    if (kind == SEL_MSG_CMD) {
        *count = SEL_CMD_COUNT;
        return k_cmds;
    }
    *count = SEL_EVT_COUNT;
    return k_evts;
}

static const sel_msg_info* info_of(uint32_t kind, uint32_t id) {
    uint32_t count;
    const sel_msg_info* t = table_of(kind, &count);
    return id < count ? &t[id] : NULL;
}

static int by_name(uint32_t kind, const char* name, uint32_t* out) {
    uint32_t count, i;
    const sel_msg_info* t = table_of(kind, &count);
    for (i = 0; i < count; i++) {
        if (strcmp(t[i].name, name) == 0) {
            if (out) {
                *out = i;
            }
            return 1;
        }
    }
    return 0;
}

const char* sel_cmd_name(uint32_t id) {
    const sel_msg_info* i = info_of(SEL_MSG_CMD, id);
    return i ? i->name : NULL;
}
const char* sel_cmd_args(uint32_t id) {
    const sel_msg_info* i = info_of(SEL_MSG_CMD, id);
    return i ? i->args : NULL;
}
const char* sel_cmd_area(uint32_t id) {
    const sel_msg_info* i = info_of(SEL_MSG_CMD, id);
    return i ? i->area : NULL;
}
int sel_cmd_by_name(const char* name, uint32_t* out) {
    return by_name(SEL_MSG_CMD, name, out);
}
const char* sel_evt_name(uint32_t id) {
    const sel_msg_info* i = info_of(SEL_MSG_EVT, id);
    return i ? i->name : NULL;
}
const char* sel_evt_args(uint32_t id) {
    const sel_msg_info* i = info_of(SEL_MSG_EVT, id);
    return i ? i->args : NULL;
}
int sel_evt_by_name(const char* name, uint32_t* out) {
    return by_name(SEL_MSG_EVT, name, out);
}

const char* sel_codec_strerror(int err) {
    switch (err) {
    case SEL_CODEC_OK:
        return "ok";
    case SEL_CODEC_E_EMPTY:
        return "empty_line";
    case SEL_CODEC_E_UNKNOWN_VERB:
        return "unknown_command";
    case SEL_CODEC_E_ARG_COUNT:
        return "argument_count";
    case SEL_CODEC_E_ARG_TYPE:
        return "argument_type";
    case SEL_CODEC_E_UNTERMINATED:
        return "unterminated";
    case SEL_CODEC_E_BAD_TAG:
        return "bad_tag";
    case SEL_CODEC_E_OVERFLOW:
        return "overflow";
    default:
        return "unknown_error";
    }
}

/* ---- encode ---------------------------------------------------------- */

typedef struct writer {
    char* out;
    size_t cap;
    size_t len;
    int overflow;
} writer;

static void put_char(writer* w, char c) {
    if (w->len + 1 < w->cap) {
        w->out[w->len] = c;
    } else {
        w->overflow = 1;
    }
    w->len++;
}

static void put_str(writer* w, const char* s) {
    while (*s) {
        put_char(w, *s++);
    }
}

static int needs_quotes(const char* s) {
    if (*s == '\0') {
        return 1;
    }
    for (; *s; s++) {
        if (*s == ' ' || *s == '"' || *s == '\\' || *s == '{' || *s == '}' || *s == ',' ||
            *s == '=' || *s == '\t' || *s == '\n') {
            return 1;
        }
    }
    return 0;
}

static void put_quoted(writer* w, const char* s) {
    if (s == NULL) {
        s = "";
    }
    if (!needs_quotes(s)) {
        put_str(w, s);
        return;
    }
    put_char(w, '"');
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') {
            put_char(w, '\\');
        }
        put_char(w, *s);
    }
    put_char(w, '"');
}

static void put_u32(writer* w, uint32_t v) {
    char buf[16];
    int n = snprintf(buf, sizeof buf, "%lu", (unsigned long)v);
    if (n > 0) {
        put_str(w, buf);
    }
}

static void put_i32(writer* w, int32_t v, int relative) {
    char buf[16];
    int n = snprintf(buf, sizeof buf, relative ? "%+ld" : "%ld", (long)v);
    if (n > 0) {
        put_str(w, buf);
    }
}

static void put_tags(writer* w, const sel_tag* list) {
    const sel_tag* it = list;
    const sel_tag* t;
    int first = 1;
    char buf[32];
    put_char(w, '{');
    while ((t = sel_tag_next(&it)) != NULL) {
        const char* name = sel_tag_name(t->tag);
        sel_tag_type type = SEL_TT_INT;
        if (!first) {
            put_char(w, ',');
        }
        first = 0;
        if (name) {
            put_str(w, name);
            sel_tag_type_of(t->tag, &type);
        } else {
            snprintf(buf, sizeof buf, "0x%lX", (unsigned long)t->tag);
            put_str(w, buf);
            type = SEL_TT_PTR;
        }
        put_char(w, '=');
        switch (type) {
        case SEL_TT_INT:
            put_u32(w, (uint32_t)t->value);
            break;
        case SEL_TT_STR:
            put_quoted(w, (const char*)t->value);
            break;
        case SEL_TT_PTR:
        case SEL_TT_LIST:
        default:
            snprintf(buf, sizeof buf, "0x%lX", (unsigned long)t->value);
            put_str(w, buf);
            break;
        }
    }
    put_char(w, '}');
}

int sel_codec_encode(const sel_message* msg, char* out, size_t cap) {
    writer w;
    const sel_msg_info* info = info_of(msg->kind, msg->id);
    uint32_t i;
    if (info == NULL || out == NULL || cap == 0) {
        return -1;
    }
    w.out = out;
    w.cap = cap;
    w.len = 0;
    w.overflow = 0;
    put_str(&w, info->name);
    for (i = 0; i < msg->argc && i < SEL_MAX_ARGS; i++) {
        const sel_arg* a = &msg->args[i];
        put_char(&w, ' ');
        switch (a->type) {
        case SEL_ARG_STR:
            put_quoted(&w, a->v.s);
            break;
        case SEL_ARG_INT:
            put_i32(&w, a->v.i, (a->flags & SEL_ARG_RELATIVE) != 0);
            break;
        case SEL_ARG_UINT:
            put_u32(&w, a->v.u);
            break;
        case SEL_ARG_TAGS:
            put_tags(&w, a->v.t);
            break;
        default:
            return -1;
        }
    }
    if (w.overflow) {
        out[cap - 1] = '\0';
        return -1;
    }
    out[w.len] = '\0';
    return (int)w.len;
}

/* ---- decode ---------------------------------------------------------- */

typedef struct pool {
    char* base;
    size_t cap;
    size_t used;
} pool;

static void* pool_take(pool* p, size_t bytes, size_t align) {
    size_t start = (p->used + align - 1) & ~(align - 1);
    if (start + bytes > p->cap) {
        return NULL;
    }
    p->used = start + bytes;
    return p->base + start;
}

static const char* skip_ws(const char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }
    return s;
}

/* Reads one token at *cursor into the pool as a NUL-terminated string.
 * Quoted tokens lose their quotes and escapes. `stop` is an extra
 * terminator for bare tokens (',' or '}' inside a tag list). */
static int read_token(const char** cursor, pool* p, char stop, const char** out, int* quoted) {
    const char* s = *cursor;
    char* dst;
    size_t start = p->used;
    *quoted = 0;
    if (*s == '"') {
        *quoted = 1;
        s++;
        for (;;) {
            char c = *s;
            if (c == '\0') {
                return SEL_CODEC_E_UNTERMINATED;
            }
            if (c == '"') {
                s++;
                break;
            }
            if (c == '\\') {
                s++;
                c = *s;
                if (c == '\0') {
                    return SEL_CODEC_E_UNTERMINATED;
                }
            }
            dst = pool_take(p, 1, 1);
            if (dst == NULL) {
                return SEL_CODEC_E_OVERFLOW;
            }
            *dst = c;
            s++;
        }
    } else {
        while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n' && *s != stop) {
            dst = pool_take(p, 1, 1);
            if (dst == NULL) {
                return SEL_CODEC_E_OVERFLOW;
            }
            *dst = *s++;
        }
    }
    dst = pool_take(p, 1, 1);
    if (dst == NULL) {
        return SEL_CODEC_E_OVERFLOW;
    }
    *dst = '\0';
    *out = p->base + start;
    *cursor = s;
    return SEL_CODEC_OK;
}

static int parse_u32(const char* s, uint32_t* out) {
    char* end;
    unsigned long v;
    if (*s == '\0' || *s == '-' || *s == '+') {
        return 0;
    }
    errno = 0;
    v = strtoul(s, &end, 0);
    if (errno != 0 || *end != '\0' || v > 0xFFFFFFFFul) {
        return 0;
    }
    *out = (uint32_t)v;
    return 1;
}

static int parse_i32(const char* s, int32_t* out, uint8_t* flags) {
    char* end;
    long v;
    if (*s == '\0') {
        return 0;
    }
    *flags = (*s == '+' || *s == '-') ? SEL_ARG_RELATIVE : 0;
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || v > 0x7FFFFFFFl || v < -0x7FFFFFFFl - 1) {
        return 0;
    }
    *out = (int32_t)v;
    return 1;
}

/* Parses {NAME=value,...} at *cursor into a SEL_TAG_END terminated list in
 * the pool. Tags are read one at a time; the list grows in place because
 * string values are copied before the next sel_tag slot is taken, so
 * every slot is allocated at the current end of the pool and the array
 * stays contiguous only if we collect entries first. To keep it simple
 * and allocation-free we take a two-pass approach: count entries, reserve
 * the array, then fill values. */
static int count_tags(const char* s) {
    int n = 0;
    int in_quote = 0;
    if (*s != '{') {
        return -1;
    }
    s++;
    if (*s == '}') {
        return 0;
    }
    n = 1;
    for (; *s; s++) {
        if (in_quote) {
            if (*s == '\\' && s[1]) {
                s++;
            } else if (*s == '"') {
                in_quote = 0;
            }
        } else if (*s == '"') {
            in_quote = 1;
        } else if (*s == ',') {
            n++;
        } else if (*s == '}') {
            return n;
        }
    }
    return -1;
}

static int read_tags(const char** cursor, pool* p, const sel_tag** out) {
    const char* s = *cursor;
    int n = count_tags(s);
    sel_tag* list;
    int i;
    if (n < 0) {
        return SEL_CODEC_E_UNTERMINATED;
    }
    list = pool_take(p, (size_t)(n + 1) * sizeof(sel_tag), sizeof(uintptr_t));
    if (list == NULL) {
        return SEL_CODEC_E_OVERFLOW;
    }
    s++; /* '{' */
    for (i = 0; i < n; i++) {
        const char* name;
        const char* value;
        int quoted;
        int rc;
        uint32_t tag;
        sel_tag_type type = SEL_TT_PTR;
        s = skip_ws(s);
        rc = read_token(&s, p, '=', &name, &quoted);
        if (rc != SEL_CODEC_OK) {
            return rc;
        }
        if (*s != '=') {
            return SEL_CODEC_E_BAD_TAG;
        }
        s++;
        if (!sel_tag_by_name(name, &tag)) {
            if (!parse_u32(name, &tag)) {
                return SEL_CODEC_E_BAD_TAG;
            }
        } else {
            sel_tag_type_of(tag, &type);
        }
        rc = read_token(&s, p, (i + 1 < n) ? ',' : '}', &value, &quoted);
        if (rc != SEL_CODEC_OK) {
            return rc;
        }
        list[i].tag = tag;
        if (type == SEL_TT_STR) {
            list[i].value = (uintptr_t)value;
        } else {
            uint32_t v;
            if (quoted || !parse_u32(value, &v)) {
                return SEL_CODEC_E_BAD_TAG;
            }
            list[i].value = v;
        }
        s = skip_ws(s);
        if (i + 1 < n) {
            if (*s != ',') {
                return SEL_CODEC_E_BAD_TAG;
            }
            s++;
        }
    }
    if (*s != '}') {
        return SEL_CODEC_E_BAD_TAG;
    }
    s++;
    list[n].tag = SEL_TAG_END;
    list[n].value = 0;
    *out = list;
    *cursor = s;
    return SEL_CODEC_OK;
}

size_t sel_codec_verb(const char* line, char* out, size_t cap) {
    const char* s = skip_ws(line);
    size_t n = 0;
    while (s[n] && s[n] != ' ' && s[n] != '\t' && s[n] != '\r' && s[n] != '\n') {
        n++;
    }
    if (cap) {
        size_t c = n < cap - 1 ? n : cap - 1;
        memcpy(out, s, c);
        out[c] = '\0';
    }
    return n;
}

int sel_codec_decode(const char* line, uint32_t kind, sel_message* out, char* poolbuf,
                     size_t poolcap) {
    pool p;
    const char* s;
    const char* verb;
    const char* sig;
    const sel_msg_info* info;
    size_t siglen, required, maximum, i;
    int repeat = 0;
    int quoted;
    int rc;

    p.base = poolbuf;
    p.cap = poolcap;
    p.used = 0;
    memset(out, 0, sizeof *out);
    out->kind = kind;

    s = skip_ws(line);
    if (*s == '\0') {
        return SEL_CODEC_E_EMPTY;
    }
    rc = read_token(&s, &p, '\0', &verb, &quoted);
    if (rc != SEL_CODEC_OK) {
        return rc;
    }
    if (!by_name(kind, verb, &out->id)) {
        return SEL_CODEC_E_UNKNOWN_VERB;
    }
    info = info_of(kind, out->id);
    sig = info->args;
    siglen = strlen(sig);
    required = maximum = siglen;
    if (siglen && sig[siglen - 1] == '?') {
        required = siglen - 1;
        maximum = siglen - 1;
        siglen--;
    } else if (siglen && sig[siglen - 1] == '*') {
        required = siglen - 1;
        maximum = SEL_MAX_ARGS;
        repeat = 1;
        siglen--;
    }
    if (required > SEL_MAX_ARGS) {
        required = SEL_MAX_ARGS;
    }
    if (maximum > SEL_MAX_ARGS) {
        maximum = SEL_MAX_ARGS;
    }

    for (i = 0; i < maximum; i++) {
        sel_arg* a = &out->args[i];
        char letter;
        s = skip_ws(s);
        if (*s == '\0') {
            break;
        }
        letter = repeat && i >= siglen - 1 ? sig[siglen - 1] : sig[i];
        a->type = (uint8_t)letter;
        switch (letter) {
        case 's': {
            const char* v;
            rc = read_token(&s, &p, '\0', &v, &quoted);
            if (rc != SEL_CODEC_OK) {
                return rc;
            }
            a->v.s = v;
            break;
        }
        case 'i': {
            const char* v;
            rc = read_token(&s, &p, '\0', &v, &quoted);
            if (rc != SEL_CODEC_OK) {
                return rc;
            }
            if (quoted || !parse_i32(v, &a->v.i, &a->flags)) {
                return SEL_CODEC_E_ARG_TYPE;
            }
            break;
        }
        case 'u': {
            const char* v;
            rc = read_token(&s, &p, '\0', &v, &quoted);
            if (rc != SEL_CODEC_OK) {
                return rc;
            }
            if (quoted || !parse_u32(v, &a->v.u)) {
                return SEL_CODEC_E_ARG_TYPE;
            }
            break;
        }
        case 't':
            rc = read_tags(&s, &p, &a->v.t);
            if (rc != SEL_CODEC_OK) {
                return rc;
            }
            break;
        default:
            return SEL_CODEC_E_ARG_TYPE;
        }
        out->argc++;
    }
    s = skip_ws(s);
    if (out->argc < required || *s != '\0') {
        return SEL_CODEC_E_ARG_COUNT;
    }
    return SEL_CODEC_OK;
}
