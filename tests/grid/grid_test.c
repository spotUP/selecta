/* T25 grid-unit: the text UI cell grid, key parser, theme tokens and POSIX
 * terminal device (plan M1.1 phase 5, row I59). Behaviour is tested where it
 * lives; the reachability test of the whole UI is T19.
 * Usage: grid_test [group...]
 * groups: flush full keys theme term   (none: all) */
#define _XOPEN_SOURCE 600
#include "vectors.h"

#include "grid.h"
#include "keys.h"
#include "term.h"
#include "theme.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/* Mode bits we change; the kernel owns others (FLUSHO flips on macOS). */
#define MODE_BITS ((tcflag_t)(ICANON | ECHO | ISIG | IEXTEN))

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

static int want(int argc, char** argv, const char* group) {
    int i;
    if (argc < 2) return 1;
    for (i = 1; i < argc; i++)
        if (strcmp(argv[i], group) == 0) return 1;
    return 0;
}

static const txt_style PLAIN = {TXT_COLOR_DEFAULT, TXT_COLOR_DEFAULT, 0};

static long flush_str(txt_grid* g, char* buf, size_t cap) {
    long n = txt_grid_flush(g, buf, cap);
    if (n >= 0) buf[n] = '\0';
    return n;
}

static void test_flush(void) {
    txt_grid g;
    char buf[8192];
    long n;
    CHECK(txt_grid_init(&g, 80, 24) == 0, "init 80x24");
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n > 0, "first flush is a full redraw, got %ld", n);
    /* unchanged frame: zero bytes */
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n == 0, "unchanged frame flushed %ld bytes", n);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n == 0, "second unchanged frame flushed %ld bytes", n);
    /* one cell */
    txt_grid_put(&g, 10, 5, 'X', PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n == (long)strlen(VEC_ONE_CELL) && strcmp(buf, VEC_ONE_CELL) == 0,
          "one-cell flush: %ld bytes, expected the %zu byte vector", n, strlen(VEC_ONE_CELL));
    CHECK(strstr(buf, "2026") == NULL, "a diff flush must not use synchronized output");
    /* writing the same cell again is not a change */
    txt_grid_put(&g, 10, 5, 'X', PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n == 0, "rewriting the same cell flushed %ld bytes", n);
    /* styled cell: SGR and trailing reset */
    txt_grid_put(&g, 10, 5, 'X', txt_style_make(196, 21, TXT_ATTR_BOLD));
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(strcmp(buf, VEC_ONE_CELL_STYLED) == 0, "styled one-cell flush wrong: %s", buf);
    /* adjacent cells share one cursor move */
    txt_grid_put(&g, 0, 0, 'A', PLAIN);
    txt_grid_put(&g, 1, 0, 'B', PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(strcmp(buf, VEC_TWO_ADJACENT) == 0, "adjacent cells flush wrong: %s", buf);
    /* a too-small buffer fails and commits nothing */
    txt_grid_put(&g, 3, 3, 'Z', PLAIN);
    n = txt_grid_flush(&g, buf, 2);
    CHECK(n == -1, "short buffer returned %ld", n);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n > 0 && strstr(buf, "Z") != NULL, "retry after short buffer lost the change");
    /* UTF-8 glyphs and clipping */
    txt_grid_put(&g, 0, 1, 0x2588, PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(strcmp(buf, "\x1b[2;1H\xE2\x96\x88") == 0, "block glyph encoding wrong");
    CHECK(txt_grid_text(&g, 78, 2, "h\xC3\xA9llo", 0, PLAIN) == 2, "text clips at the right edge");
    CHECK(txt_grid_text(&g, 0, 2, "\xC3\xA9\xFF" "ab", 0, PLAIN) == 4, "invalid byte draws one cell");
    CHECK(g.back[2 * 80 + 1].cp == 0xFFFD, "invalid byte is U+FFFD");
    /* I218a: an Amiga console gets Latin-1, '?' above U+00FF */
    (void)flush_str(&g, buf, sizeof buf - 1);
    g.latin1 = 1;
    txt_grid_put(&g, 0, 3, 0xE9, PLAIN);
    txt_grid_put(&g, 1, 3, 0x2588, PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(strcmp(buf, "\x1b[4;1H\xE9?") == 0, "Latin-1 flush wrong");
    g.latin1 = 0;
    txt_grid_put(&g, 200, 200, 'Q', PLAIN); /* out of range: ignored, no crash */
    txt_grid_free(&g);
}

/* I218e amiga-console-first-frame: the Amiga console.device shows no list lines after start. The full
 * first frame must hold only sequences the console understands: no CSI ? 2026 h/l frame (the one
 * private mode that only full frames carry) and no write to the bottom right cell (the console scrolls
 * the window when it is written). Every other cell, and a later diff frame, stay as on any terminal. */
static void test_console_first_frame(void) {
    txt_grid g;
    char buf[1024];
    long n;
    CHECK(txt_grid_init(&g, 3, 2) == 0, "init 3x2");
    g.console = 1;
    txt_grid_text(&g, 0, 0, "abc", 0, PLAIN);
    txt_grid_text(&g, 0, 1, "def", 0, PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n > 0 && strstr(buf, "?2026") == NULL, "console frame carries CSI ? 2026: %s", buf);
    CHECK(strcmp(buf, "\x1b[1;1Habc\x1b[2;1Hde") == 0, "console first frame wrong: %s", buf);
    txt_grid_put(&g, 2, 1, 'X', PLAIN);
    CHECK(txt_grid_flush(&g, buf, sizeof buf) == 0, "corner cell is never written");
    txt_grid_put(&g, 1, 1, 'Y', PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(strcmp(buf, "\x1b[2;2HY") == 0, "console diff frame wrong: %s", buf);
    txt_grid_free(&g);
}

static void test_full(void) {
    txt_grid g;
    char buf[1024];
    long n;
    size_t len;
    CHECK(txt_grid_init(&g, 3, 2) == 0, "init 3x2");
    txt_grid_text(&g, 0, 0, "abc", 0, PLAIN);
    n = flush_str(&g, buf, sizeof buf - 1);
    CHECK(n == (long)strlen(VEC_FULL_3X2) && strcmp(buf, VEC_FULL_3X2) == 0,
          "full redraw vector mismatch (%ld bytes): %s", n, buf);
    len = strlen(buf);
    CHECK(strncmp(buf, VEC_SYNC_BEGIN, strlen(VEC_SYNC_BEGIN)) == 0, "full redraw starts with CSI ? 2026 h");
    CHECK(len >= strlen(VEC_SYNC_END) && strcmp(buf + len - strlen(VEC_SYNC_END), VEC_SYNC_END) == 0,
          "full redraw ends with CSI ? 2026 l");
    /* invalidate and resize force a full redraw again */
    txt_grid_invalidate(&g);
    flush_str(&g, buf, sizeof buf - 1);
    CHECK(strncmp(buf, VEC_SYNC_BEGIN, strlen(VEC_SYNC_BEGIN)) == 0, "invalidate gives a framed redraw");
    CHECK(txt_grid_resize(&g, 4, 4) == 0, "resize");
    flush_str(&g, buf, sizeof buf - 1);
    CHECK(strncmp(buf, VEC_SYNC_BEGIN, strlen(VEC_SYNC_BEGIN)) == 0, "resize gives a framed redraw");
    CHECK(txt_grid_resize(&g, 0, 4) == -1, "zero width refused");
    CHECK(g.flushes == 3u, "flush counter is %u", (unsigned)g.flushes);
    {
        char* big = (char*)malloc(txt_grid_flush_bound(&g));
        txt_grid_invalidate(&g);
        txt_grid_clear(&g, txt_style_make(196, 21, TXT_ATTR_BOLD | TXT_ATTR_REVERSE));
        CHECK(big && txt_grid_flush(&g, big, txt_grid_flush_bound(&g)) > 0, "flush_bound suffices");
        free(big);
    }
    txt_grid_free(&g);
}

static int feed_all(txt_keys* k, const char* s, txt_key* out, int max) {
    int n = 0;
    txt_key key;
    for (; *s; s++)
        if (txt_keys_feed(k, (uint8_t)*s, &key) && n < max) out[n++] = key;
    return n;
}

static void test_keys(void) {
    txt_keys k;
    txt_key o[8];
    int n;
    txt_keys_init(&k);
    n = feed_all(&k, "q", o, 8);
    CHECK(n == 1 && o[0].code == 'q' && o[0].mods == 0, "plain q");
    n = feed_all(&k, "\r", o, 8);
    CHECK(n == 1 && o[0].code == 13, "Enter");
    n = feed_all(&k, "\x1b[A\x1b[B\x1b[C\x1b[D", o, 8);
    CHECK(n == 4 && o[0].code == TXT_KEY_UP && o[1].code == TXT_KEY_DOWN &&
              o[2].code == TXT_KEY_RIGHT && o[3].code == TXT_KEY_LEFT, "arrows");
    n = feed_all(&k, "\x1bOA", o, 8);
    CHECK(n == 1 && o[0].code == TXT_KEY_UP, "SS3 arrow");
    n = feed_all(&k, "\x1b[5~\x1b[6~\x1b[H\x1b[F\x1b[3~", o, 8);
    CHECK(n == 5 && o[0].code == TXT_KEY_PAGE_UP && o[1].code == TXT_KEY_PAGE_DOWN &&
              o[2].code == TXT_KEY_HOME && o[3].code == TXT_KEY_END && o[4].code == TXT_KEY_DELETE,
          "page, home, end, delete");
    n = feed_all(&k, "\x1b[1;5C", o, 8);
    CHECK(n == 1 && o[0].code == TXT_KEY_RIGHT && o[0].mods == TXT_MOD_CTRL, "Ctrl+Right");
    n = feed_all(&k, "\x1b[1;2A", o, 8);
    CHECK(n == 1 && o[0].code == TXT_KEY_UP && o[0].mods == TXT_MOD_SHIFT, "Shift+Up");
    n = feed_all(&k, "\x03", o, 8);
    CHECK(n == 1 && o[0].code == 'c' && o[0].mods == TXT_MOD_CTRL, "Ctrl+C");
    n = feed_all(&k, "\xC3\xA9\xE2\x96\x88", o, 8);
    CHECK(n == 2 && o[0].code == 0xE9 && o[1].code == 0x2588, "UTF-8 keys");
    n = feed_all(&k, "\x1b" "x", o, 8);
    CHECK(n == 1 && o[0].code == 'x' && o[0].mods == TXT_MOD_ALT, "Alt+x");
    /* a lone ESC is Escape only after the timeout */
    n = feed_all(&k, "\x1b", o, 8);
    CHECK(n == 0, "ESC alone waits");
    CHECK(txt_keys_timeout(&k, &o[0]) == 1 && o[0].code == 27, "timeout makes Escape");
    CHECK(txt_keys_timeout(&k, &o[0]) == 0, "no second Escape");
    /* SGR mouse: left press at column 10, row 5; release; wheel */
    n = feed_all(&k, "\x1b[<0;10;5M\x1b[<0;10;5m\x1b[<64;3;4M", o, 8);
    CHECK(n == 3 && o[0].code == TXT_KEY_MOUSE && o[0].x == 9 && o[0].y == 4 && o[0].button == 0 &&
              o[0].pressed == 1 && o[1].pressed == 0 && o[2].button == 64, "SGR mouse");
    /* I218: the Amiga console sends keys after the 8-bit CSI 0x9B with its own codes */
    n = feed_all(&k, "\x9b" "A\x9b" "B\x9b" "C\x9b" "D", o, 8);
    CHECK(n == 4 && o[0].code == TXT_KEY_UP && o[1].code == TXT_KEY_DOWN &&
              o[2].code == TXT_KEY_RIGHT && o[3].code == TXT_KEY_LEFT && o[0].mods == 0,
          "Amiga console arrows");
    n = feed_all(&k, "\x9b" "T\x9b" "S\x9b A\x9b @", o, 8);
    CHECK(n == 4 && o[0].code == TXT_KEY_UP && o[1].code == TXT_KEY_DOWN && o[2].code == TXT_KEY_LEFT &&
              o[3].code == TXT_KEY_RIGHT && o[0].mods == TXT_MOD_SHIFT && o[1].mods == TXT_MOD_SHIFT &&
              o[2].mods == TXT_MOD_SHIFT && o[3].mods == TXT_MOD_SHIFT,
          "Amiga console Shift+arrows");
    n = feed_all(&k, "\x9b" "0~\x9b" "9~\x9b" "10~\x9b" "41~\x9b" "45~\x9b" "54~", o, 8);
    CHECK(n == 6 && o[0].code == TXT_KEY_F1 && o[1].code == TXT_KEY_F1 + 9u && o[2].code == TXT_KEY_F1 &&
              o[2].mods == TXT_MOD_SHIFT && o[3].code == TXT_KEY_PAGE_UP && o[4].code == TXT_KEY_END &&
              o[5].code == TXT_KEY_HOME && o[5].mods == TXT_MOD_SHIFT,
          "Amiga console function and PC keys");
    n = feed_all(&k, "\x9b?~q", o, 8);
    CHECK(n == 1 && o[0].code == 'q', "Amiga Help dropped, next key intact");
    /* I218a: the Amiga console sends Latin-1 bytes, Backspace 0x08 and Del 0x7F */
    k.amiga_console = 1;
    n = feed_all(&k, "\xe4q\x08\x7f\x85q", o, 8);
    CHECK(n == 6 && o[0].code == 0xE4u && o[1].code == 'q' && o[2].code == 127u && o[3].code == TXT_KEY_DELETE &&
              o[4].code == 0xFFFDu && o[5].code == 'q',
          "Amiga console Latin-1, Backspace and Del");
    /* I218c: the console reports a resize event and the window bounds, 8-bit and 7-bit CSI */
    n = feed_all(&k, "\x9b" "12;0;0;0;0;0;0;0|\x9b" "1;1;50;120 r", o, 8);
    CHECK(n == 2 && o[0].code == TXT_KEY_RESIZE && o[1].code == TXT_KEY_WINDOW && o[1].x == 120u &&
              o[1].y == 50u,
          "Amiga console resize event and window bounds");
    n = feed_all(&k, "\x1b[12;0;0;0;0;0;0;0|\x1b[1;1;30;100 rq", o, 8);
    CHECK(n == 3 && o[0].code == TXT_KEY_RESIZE && o[1].code == TXT_KEY_WINDOW && o[1].x == 100u &&
              o[1].y == 30u && o[2].code == 'q',
          "Amiga console resize reports with the 7-bit CSI");
    k.amiga_console = 0;
    n = feed_all(&k, "\x9b" "12;0;0;0;0;0;0;0|\x9b" "1;1;50;120 rq\x1b[12;0;0;0;0;0;0;0|q", o, 8);
    CHECK(n == 2 && o[0].code == 'q' && o[1].code == 'q', "resize reports are not keys without the Amiga console");
    n = feed_all(&k, "\x7f\xc3\xa4", o, 8);
    CHECK(n == 2 && o[0].code == 127u && o[1].code == 0xE4u, "xterm Backspace 0x7F and UTF-8 stay");
    /* an unknown sequence is dropped whole and the parser recovers */
    n = feed_all(&k, "\x1b[99zq", o, 8);
    CHECK(n == 1 && o[0].code == 'q', "unknown CSI dropped, next key intact");
}

static void test_theme(void) {
    txt_theme t, d;
    int bad = 0, n;
    int i;
    txt_theme_default(&d);
    for (i = 0; i < TXT_ROLE_COUNT; i++)
        CHECK(strlen(txt_role_name((txt_role)i)) > 0, "role %d has a name", i);
    t = d;
    n = txt_theme_parse(&t,
                        "# comment\n"
                        "cursor = 16 110 bold\n"
                        "\n"
                        "playing = 46 default reverse # trailing comment\r\n",
                        &bad);
    CHECK(n == 2, "two lines applied, got %d", n);
    CHECK(t.role[TXT_ROLE_CURSOR].fg == 16 && t.role[TXT_ROLE_CURSOR].bg == 110 &&
              t.role[TXT_ROLE_CURSOR].attr == TXT_ATTR_BOLD, "cursor role");
    CHECK(t.role[TXT_ROLE_PLAYING].fg == 46 && t.role[TXT_ROLE_PLAYING].bg == TXT_COLOR_DEFAULT &&
              t.role[TXT_ROLE_PLAYING].attr == TXT_ATTR_REVERSE, "playing role");
    CHECK(memcmp(&t.role[TXT_ROLE_ERROR], &d.role[TXT_ROLE_ERROR], sizeof d.role[0]) == 0,
          "untouched role keeps the default");
    n = txt_theme_parse(&t, "normal = 1 2\nnope = 1 2\n", &bad);
    CHECK(n == -1 && bad == 2, "unknown role reported at line 2, got %d line %d", n, bad);
    n = txt_theme_parse(&t, "normal = 300 2\n", &bad);
    CHECK(n == -1 && bad == 1, "colour 300 refused");
    n = txt_theme_parse(&t, "normal = 1 2 blink\n", &bad);
    CHECK(n == -1, "unknown attribute refused");
}

static void test_term(void) {
    int master, slave;
    txt_term t;
    struct termios before, during, after;
    struct winsize ws;
    uint32_t cols, rows;
    uint8_t in[16];
    char out[256];
    long n;
    master = posix_openpt(O_RDWR | O_NOCTTY);
    CHECK(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0, "pty master");
    if (master < 0) return;
    slave = open(ptsname(master), O_RDWR | O_NOCTTY);
    CHECK(slave >= 0, "pty slave");
    if (slave < 0) {
        close(master);
        return;
    }
    memset(&ws, 0, sizeof ws);
    ws.ws_col = 100;
    ws.ws_row = 30;
    CHECK(ioctl(master, TIOCSWINSZ, &ws) == 0, "set window size");
    tcgetattr(slave, &before);
    CHECK(txt_term_open(&t, slave, slave) == 0, "open on a terminal");
    tcgetattr(slave, &during);
    CHECK((during.c_lflag & (ICANON | ECHO)) == 0, "raw mode: no canonical, no echo");
    txt_term_size(&t, &cols, &rows);
    CHECK(cols == 100 && rows == 30, "size %ux%u", (unsigned)cols, (unsigned)rows);
    n = read(master, out, sizeof out - 1);
    CHECK(n == (long)strlen(txt_term_enter_seq) && memcmp(out, txt_term_enter_seq, (size_t)n) == 0,
          "enter sequence written (%ld bytes)", n);
    CHECK(strstr(txt_term_enter_seq, "\x1b[?1049h") != NULL && strstr(txt_term_enter_seq, "\x1b[?25l") != NULL,
          "enter uses the alternate screen and hides the cursor");
    CHECK(txt_term_read(&t, in, sizeof in, 20) == 0, "read times out with no input");
    CHECK(write(master, "ab", 2) == 2, "feed input");
    n = txt_term_read(&t, in, sizeof in, 1000);
    CHECK(n == 2 && in[0] == 'a' && in[1] == 'b', "read returns the bytes (%ld)", n);
    CHECK(txt_term_write(&t, "hello", 5) == 0, "write");
    n = read(master, out, sizeof out - 1);
    CHECK(n == 5 && memcmp(out, "hello", 5) == 0, "written bytes reach the terminal");
    txt_term_close(&t);
    n = read(master, out, sizeof out - 1);
    CHECK(n == (long)strlen(txt_term_leave_seq) && memcmp(out, txt_term_leave_seq, (size_t)n) == 0,
          "leave sequence written");
    tcgetattr(slave, &after);
    CHECK((after.c_lflag & MODE_BITS) == (before.c_lflag & MODE_BITS) && after.c_iflag == before.c_iflag &&
              after.c_oflag == before.c_oflag, "terminal state restored (l %lx/%lx i %lx/%lx o %lx/%lx)", (unsigned long)before.c_lflag,
          (unsigned long)after.c_lflag, (unsigned long)before.c_iflag, (unsigned long)after.c_iflag,
          (unsigned long)before.c_oflag, (unsigned long)after.c_oflag);
    txt_term_close(&t); /* second close is a no-op */
    {
        int pipefd[2];
        CHECK(pipe(pipefd) == 0, "pipe");
        CHECK(txt_term_open(&t, pipefd[0], pipefd[1]) == -1, "a pipe is not a terminal");
        close(pipefd[0]);
        close(pipefd[1]);
    }
    close(slave);
    close(master);
}

int main(int argc, char** argv) {
    if (want(argc, argv, "flush")) test_flush();
    if (want(argc, argv, "full")) {
        test_full();
        test_console_first_frame();
    }
    if (want(argc, argv, "keys")) test_keys();
    if (want(argc, argv, "theme")) test_theme();
    if (want(argc, argv, "term")) test_term();
    if (failures) {
        printf("grid_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("grid_test: ok\n");
    return 0;
}
