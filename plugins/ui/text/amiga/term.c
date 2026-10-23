/* AmigaOS terminal device (UP-Term or the Shell console), plan M2 phase 6, row I130.
 * Raw mode is SetMode(Input(), 1); a timed key wait is WaitForChar; the window size is asked of
 * the terminal with CSI 18 t once at open. in_fd and out_fd are ignored: the Amiga has no
 * descriptor numbers, the streams are Input() and Output(). */
#include "../term.h"

#include <dos/dos.h>
#include <proto/dos.h>

#include <string.h>

const int txt_term_amiga_console = 1;
const char txt_term_enter_seq[] = "\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h";
const char txt_term_leave_seq[] = "\x1b[?1006l\x1b[?1000l\x1b[?25h\x1b[0m\x1b[?1049l";

/* What lives in saved_bytes. */
typedef struct amiga_state {
    uint32_t cols, rows;
    int was_raw; /* stdin was an interactive stream we switched to raw */
} amiga_state;

static amiga_state* st(txt_term* t) { return (amiga_state*)(void*)t->saved_bytes; }
static const amiga_state* cst(const txt_term* t) { return (const amiga_state*)(const void*)t->saved_bytes; }

/* Reads "ESC [ 8 ; rows ; cols t" (xterm) from the stream; keeps 80x24 when nothing answers. */
static void query_size(txt_term* t) {
    static const char ask[] = "\x1b[18t";
    uint8_t b[32];
    long n = 0, got;
    uint32_t v[3] = {0, 0, 0};
    uint32_t k = 0, i;
    amiga_state* s = st(t);
    txt_term_write(t, ask, sizeof ask - 1);
    while (n < (long)sizeof b && WaitForChar(Input(), 200000)) {
        got = Read(Input(), b + n, (LONG)(sizeof b - (size_t)n));
        if (got <= 0) break;
        n += got;
        if (b[n - 1] == 't') break;
    }
    for (i = 0; i < (uint32_t)n; i++) {
        if (b[i] >= '0' && b[i] <= '9') {
            if (k < 3) v[k] = v[k] * 10u + (uint32_t)(b[i] - '0');
        } else if (b[i] == '[') {
            k = 0;
        } else if (b[i] == ';') {
            k++;
        }
    }
    if (n > 0 && b[n - 1] == 't' && v[0] == 8 && v[1] > 0 && v[2] > 0) {
        s->rows = v[1];
        s->cols = v[2];
    }
}

int txt_term_open(txt_term* t, int in_fd, int out_fd) {
    amiga_state* s;
    memset(t, 0, sizeof *t);
    t->in_fd = in_fd;
    t->out_fd = out_fd;
    s = st(t);
    s->cols = 80;
    s->rows = 24;
    if (!IsInteractive(Input())) return -1;
    if (SetMode(Input(), 1) == 0) return -1;
    s->was_raw = 1;
    t->active = 1;
    if (txt_term_write(t, txt_term_enter_seq, strlen(txt_term_enter_seq)) != 0) {
        txt_term_close(t);
        return -1;
    }
    query_size(t);
    return 0;
}

void txt_term_close(txt_term* t) {
    if (!t->active) return;
    t->active = 0;
    (void)txt_term_write(t, txt_term_leave_seq, strlen(txt_term_leave_seq));
    if (st(t)->was_raw) (void)SetMode(Input(), 0);
    st(t)->was_raw = 0;
}

void txt_term_size(const txt_term* t, uint32_t* cols, uint32_t* rows) {
    const amiga_state* s = cst(t);
    *cols = s->cols > 0 ? s->cols : 80;
    *rows = s->rows > 0 ? s->rows : 24;
}

long txt_term_read(txt_term* t, uint8_t* buf, size_t cap, int timeout_ms) {
    LONG n;
    (void)t;
    if (!IsInteractive(Input())) {
        /* Redirected stdin (a file or a pipe under a test): reading never waits on a keyboard. */
    } else if (timeout_ms < 0) {
        /* WaitForChar takes microseconds; loop until a character is there. */
        while (!WaitForChar(Input(), 1000000)) {
        }
    } else if (!WaitForChar(Input(), (LONG)timeout_ms * 1000L)) {
        return 0;
    }
    n = Read(Input(), buf, (LONG)cap);
    return n > 0 ? (long)n : -1; /* 0 from Read is end of input */
}

int txt_term_write(txt_term* t, const char* buf, size_t n) {
    (void)t;
    while (n > 0) {
        LONG w = Write(Output(), (APTR)buf, (LONG)n);
        if (w <= 0) return -1;
        buf += w;
        n -= (size_t)w;
    }
    return 0;
}
