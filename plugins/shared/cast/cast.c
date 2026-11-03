/* CASTV2 wire layer; see cast.h. */
#include "cast/cast.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "source/json_scan.h"

/* ---- protobuf ---- */

static size_t put_varint(unsigned char* p, unsigned long v) {
    size_t n = 0;

    while (v >= 0x80u) {
        p[n++] = (unsigned char)(v | 0x80u);
        v >>= 7;
    }
    p[n++] = (unsigned char)v;
    return n;
}

static int get_varint(const unsigned char* b, size_t len, size_t* pos, unsigned long* v) {
    unsigned shift = 0;

    *v = 0;
    while (*pos < len && shift < 32u) {
        unsigned char c = b[(*pos)++];

        *v |= (unsigned long)(c & 0x7Fu) << shift;
        if ((c & 0x80u) == 0) {
            return 0;
        }
        shift += 7;
    }
    return -1;
}

/* Appends a length-delimited field; *n is the running size. */
static int put_bytes(unsigned char* out, size_t cap, size_t* n, unsigned field, const char* s) {
    size_t l = strlen(s);

    if (*n + 1u + 5u + l > cap) {
        return -1;
    }
    out[(*n)++] = (unsigned char)(field << 3 | 2u);
    *n += put_varint(out + *n, (unsigned long)l);
    memcpy(out + *n, s, l);
    *n += l;
    return 0;
}

size_t sel_cast_encode(unsigned char* out, size_t cap, const char* source, const char* dest, const char* ns,
                       const char* payload_utf8) {
    size_t n = 4; /* body starts after the prefix */

    if (cap < 16u || strlen(payload_utf8) > SEL_CAST_PAYLOAD_MAX) {
        return 0;
    }
    out[n++] = 0x08; /* 1 protocol_version = CASTV2_1_0 (0) */
    out[n++] = 0x00;
    if (put_bytes(out, cap, &n, 2, source) || put_bytes(out, cap, &n, 3, dest) || put_bytes(out, cap, &n, 4, ns)) {
        return 0;
    }
    if (n + 2u > cap) {
        return 0;
    }
    out[n++] = 0x28; /* 5 payload_type = STRING (0) */
    out[n++] = 0x00;
    if (put_bytes(out, cap, &n, 6, payload_utf8)) {
        return 0;
    }
    n -= 4;
    out[0] = (unsigned char)(n >> 24);
    out[1] = (unsigned char)(n >> 16);
    out[2] = (unsigned char)(n >> 8);
    out[3] = (unsigned char)n;
    return n + 4u;
}

unsigned long sel_cast_frame_len(const unsigned char h[4]) {
    return (unsigned long)h[0] << 24 | (unsigned long)h[1] << 16 | (unsigned long)h[2] << 8 | h[3];
}

static int copy_field(char* dst, size_t cap, const unsigned char* src, unsigned long l) {
    if (l >= cap) {
        return -1;
    }
    memcpy(dst, src, l);
    dst[l] = '\0';
    return 0;
}

int sel_cast_decode(const unsigned char* b, size_t len, sel_cast_msg* out) {
    size_t pos = 0;

    memset(out, 0, sizeof *out);
    while (pos < len) {
        unsigned long tag, v;

        if (get_varint(b, len, &pos, &tag)) {
            return -1;
        }
        switch (tag & 7u) {
        case 0:
            if (get_varint(b, len, &pos, &v)) {
                return -1;
            }
            if ((tag >> 3) == 5u) {
                out->binary = v == 1u;
            }
            break;
        case 2:
            if (get_varint(b, len, &pos, &v) || v > len - pos) {
                return -1;
            }
            switch (tag >> 3) {
            case 2:
                if (copy_field(out->source, sizeof out->source, b + pos, v)) return -1;
                break;
            case 3:
                if (copy_field(out->dest, sizeof out->dest, b + pos, v)) return -1;
                break;
            case 4:
                if (copy_field(out->ns, sizeof out->ns, b + pos, v)) return -1;
                break;
            case 6:
                if (copy_field(out->payload, sizeof out->payload, b + pos, v)) return -1;
                break;
            default:
                break; /* 7 payload_binary and unknown fields are not read */
            }
            pos += v;
            break;
        case 1:
            if (len - pos < 8u) return -1;
            pos += 8;
            break;
        case 5:
            if (len - pos < 4u) return -1;
            pos += 4;
            break;
        default:
            return -1;
        }
    }
    return 0;
}

/* ---- JSON payload ---- */

typedef struct walk {
    sel_cast_status* st;
    int in_app; /* inside the first application record */
    int in_media; /* inside the first MEDIA_STATUS entry */
} walk;

static int read_number(FILE* f, int first, double* out, int* end) {
    char buf[40];
    size_t n = 0;
    int c = first;

    while (c != EOF && (isdigit(c) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') && n < sizeof buf - 1u) {
        buf[n++] = (char)c;
        c = fgetc(f);
    }
    buf[n] = '\0';
    *out = atof(buf);
    while (c != EOF && isspace(c)) {
        c = fgetc(f);
    }
    *end = c;
    return n > 0;
}

static int walk_value(FILE* f, int c, const char* key, int depth, walk* w, int* end);

/* Walks the members of an object or array whose opening bracket is consumed. Depth: the top object is 1, its members
 * 2, the members of "status" 3, application records 4, their strings 5. `parent` is the key an array element inherits. */
static int walk_container(FILE* f, int close, int depth, walk* w, const char* parent, int* end) {
    int is_obj = close == '}';
    int c = sel_json_skip_ws(f);

    if (c == close) {
        *end = sel_json_skip_ws(f);
        return 1;
    }
    for (;;) {
        char key[24] = "";

        if (is_obj) {
            if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
                return 0;
            }
            c = sel_json_skip_ws(f);
        }
        if (!walk_value(f, c, is_obj ? key : parent, depth + 1, w, &c)) {
            return 0;
        }
        if (c == ',') {
            c = sel_json_skip_ws(f);
            continue;
        }
        if (c != close) {
            return 0;
        }
        *end = sel_json_skip_ws(f);
        return 1;
    }
}

static int walk_value(FILE* f, int c, const char* key, int depth, walk* w, int* end) {
    sel_cast_status* st = w->st;

    if (c == '{') {
        if (depth == 4 && strcmp(key, "applications") == 0) {
            st->app_count++;
            w->in_app = st->app_count == 1;
        }
        if (depth == 3 && strcmp(key, "status") == 0) {
            st->media_count++;
            w->in_media = st->media_count == 1;
        }
        return walk_container(f, '}', depth, w, key, end);
    }
    if (c == '[') {
        return walk_container(f, ']', depth, w, key, end);
    }
    if (c == '"') {
        char v[SEL_CAST_ID_MAX];

        if (!sel_json_read_string(f, v, sizeof v)) {
            return 0;
        }
        if (depth == 2 && strcmp(key, "type") == 0) {
            snprintf(st->type, sizeof st->type, "%s", v);
        } else if (depth == 4 && w->in_media) {
            if (strcmp(key, "playerState") == 0) {
                snprintf(st->player_state, sizeof st->player_state, "%s", v);
            } else if (strcmp(key, "idleReason") == 0) {
                snprintf(st->idle_reason, sizeof st->idle_reason, "%s", v);
            }
        } else if (depth == 5 && w->in_app) {
            if (strcmp(key, "appId") == 0) {
                snprintf(st->app_id, sizeof st->app_id, "%s", v);
            } else if (strcmp(key, "transportId") == 0) {
                snprintf(st->transport_id, sizeof st->transport_id, "%s", v);
            } else if (strcmp(key, "sessionId") == 0) {
                snprintf(st->session_id, sizeof st->session_id, "%s", v);
            } else if (strcmp(key, "displayName") == 0) {
                snprintf(st->display_name, sizeof st->display_name, "%s", v);
            }
        }
        *end = sel_json_skip_ws(f);
        return 1;
    }
    if (isdigit(c) || c == '-') {
        double d;

        if (!read_number(f, c, &d, end)) {
            return 0;
        }
        if (depth == 2 && strcmp(key, "requestId") == 0) {
            st->request_id = (long)d;
        } else if (depth == 4 && w->in_media && strcmp(key, "mediaSessionId") == 0) {
            st->media_session_id = (long)d;
        } else if (depth == 4 && w->in_media && strcmp(key, "currentTime") == 0) {
            st->current_time = d;
        } else if (depth == 5 && w->in_media && strcmp(key, "duration") == 0) {
            st->duration = d;
        } else if (strcmp(key, "level") == 0) {
            st->has_volume = 1;
            st->volume_level = d;
        }
        return 1;
    }
    if (strcmp(key, "muted") == 0) {
        st->volume_muted = c == 't';
    }
    return sel_json_skip_value(f, c, end);
}

int sel_cast_parse_status(const char* json, sel_cast_status* out) {
    FILE* f = tmpfile();
    walk w;
    int end = 0, ok = 0;

    memset(out, 0, sizeof *out);
    out->request_id = -1;
    if (f == NULL) {
        return -1;
    }
    fputs(json, f);
    rewind(f);
    memset(&w, 0, sizeof w);
    w.st = out;
    if (sel_json_skip_ws(f) == '{') {
        ok = walk_container(f, '}', 1, &w, "", &end);
    }
    fclose(f);
    return ok ? 0 : -1;
}
