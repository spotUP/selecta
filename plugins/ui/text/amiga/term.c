/* AmigaOS terminal device (UP-Term or the Shell console), plan M2 phase 6, row I130.
 * Raw mode is SetMode(Input(), 1); a timed key wait is WaitForChar; the window size is asked of
 * the terminal at open (console CSI 0 SPACE q, xterm CSI 18 t only when unanswered, row I218f) and again on each resize event.
 * in_fd and out_fd are ignored: the Amiga has no
 * descriptor numbers, the streams are Input() and Output(). */
#include "../term.h"
#include "../sizeprobe.h"

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
    int native;  /* the console.device answered the Window Status Request: resize events are on */
} amiga_state;

static amiga_state* st(txt_term* t) { return (amiga_state*)(void*)t->saved_bytes; }
static const amiga_state* cst(const txt_term* t) { return (const amiga_state*)(const void*)t->saved_bytes; }

static void probe_write(void* ctx, const char* s, size_t n) { (void)txt_term_write((txt_term*)ctx, s, n); }

static long probe_read(void* ctx, uint8_t* buf, size_t cap) {
    LONG got;
    (void)ctx;
    if (!WaitForChar(Input(), 200000)) return 0;
    got = Read(Input(), buf, (LONG)cap);
    return got > 0 ? (long)got : 0;
}

/* The size is asked of the console.device first (Window Status Request CSI 0 SPACE q, answer
 * "CSI 1;1;rows;cols SPACE r"; the CSI may be the 8-bit 0x9B). Only when it does not answer is the
 * xterm CSI 18 t sent (UP-Term): the console.device reads CSI n t as Set Page Length (row I218f).
 * 80x24 stays when nothing answers. The native answer also turns on the window-resized raw input
 * event CSI 12 { (row I218c). */
static void query_size(txt_term* t) {
    static const char events_on[] = "\x1b[12{";
    txt_probe_io io;
    txt_probe_result r;
    amiga_state* s = st(t);
    io.ctx = t;
    io.write = probe_write;
    io.read = probe_read;
    r = txt_size_probe(&io);
    if (r.rows > 0 && r.cols > 0) {
        s->rows = r.rows;
        s->cols = r.cols;
    }
    s->native = r.native;
    if (s->native) txt_term_write(t, events_on, sizeof events_on - 1);
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
    if (st(t)->native) (void)txt_term_write(t, "\x1b[12}", 5);
    st(t)->native = 0;
    (void)txt_term_write(t, txt_term_leave_seq, strlen(txt_term_leave_seq));
    if (st(t)->was_raw) (void)SetMode(Input(), 0);
    st(t)->was_raw = 0;
}

void txt_term_size(const txt_term* t, uint32_t* cols, uint32_t* rows) {
    const amiga_state* s = cst(t);
    *cols = s->cols > 0 ? s->cols : 80;
    *rows = s->rows > 0 ? s->rows : 24;
}

void txt_term_ask_size(txt_term* t) {
    if (st(t)->native) (void)txt_term_write(t, "\x1b[0 q", 5);
}

void txt_term_set_size(txt_term* t, uint32_t cols, uint32_t rows) {
    if (cols > 0 && rows > 0) {
        st(t)->cols = cols;
        st(t)->rows = rows;
    }
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
