#include "sizeprobe.h"

#include <string.h>

int txt_size_parse(const uint8_t* b, long n, txt_probe_result* out) {
    uint32_t v[5] = {0, 0, 0, 0, 0};
    uint32_t k = 0;
    long i;
    int found = 0;
    for (i = 0; i < n; i++) {
        if (b[i] >= '0' && b[i] <= '9') {
            if (k < 5) v[k] = v[k] * 10u + (uint32_t)(b[i] - '0');
        } else if (b[i] == '[' || b[i] == 0x9B) {
            k = 0;
            memset(v, 0, sizeof v);
        } else if (b[i] == ';') {
            k++;
        } else if (b[i] == 't' && v[0] == 8 && v[1] > 0 && v[2] > 0) {
            out->rows = v[1];
            out->cols = v[2];
            found = 1;
        } else if (b[i] == 'r' && v[0] == 1 && v[1] == 1 && v[2] > 0 && v[3] > 0) {
            out->rows = v[2];
            out->cols = v[3];
            out->native = 1;
            found = 1;
        }
    }
    return found;
}

static int ask(const txt_probe_io* io, const char* seq, txt_probe_result* r) {
    uint8_t b[48];
    long n = 0, got;
    io->write(io->ctx, seq, strlen(seq));
    while (n < (long)sizeof b) {
        got = io->read(io->ctx, b + n, sizeof b - (size_t)n);
        if (got <= 0) break;
        n += got;
        if (b[n - 1] == 't' || b[n - 1] == 'r') break;
    }
    return txt_size_parse(b, n, r);
}

txt_probe_result txt_size_probe(const txt_probe_io* io) {
    txt_probe_result r = {0, 0, 0};
    if (ask(io, "\x1b[0 q", &r)) {
        txt_probe_result again = {0, 0, 0};
        io->write(io->ctx, "\x1b[t", 3); /* page length back to automatic, in case an older run set one */
        if (ask(io, "\x1b[0 q", &again)) r = again;
        return r;
    }
    (void)ask(io, "\x1b[18t", &r);
    return r;
}
