/* T36: command line (plugins/ui/text/cmdline.c, spawn.c). Tab completion from
 * the COMMANDS table, status-line match list, history recall and its 100-line
 * cap, and `!program` with the selected paths as arguments. */
#include <selecta/uimodel/cmdline.h>
#include <selecta/uimodel/keys.h>
#include "spawn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static txt_cmdline g_c;

static txt_cmd_result type(const char* s) {
    txt_cmd_result r = TXT_CMD_EDITING;
    for (; *s != '\0'; s++) {
        r = txt_cmdline_key(&g_c, (unsigned char)*s, 0);
    }
    return r;
}

static int is(const char* want) {
    return strcmp(txt_cmdline_text(&g_c), want) == 0;
}

int main(int argc, char** argv) {
    char line[512], out[512];
    FILE* f;
    const char* paths[2];
    uint32_t i;
    size_t n;
    memset(&g_c, 0, sizeof g_c);

    /* Regression: a double quote inside a path must be escaped for AmigaDOS. */
    {
        char q[64];
        CHECK(txt_quote_arg(q, sizeof q, "Work:My \"Song\".mod") == 22u);
        CHECK(strcmp(q, "\"Work:My *\"Song*\".mod\"") == 0);
        CHECK(txt_quote_arg(q, sizeof q, "a*b") == 6u && strcmp(q, "\"a**b\"") == 0);
        CHECK(txt_quote_arg(q, 6u, "abc\"") == 0); /* too small: refused */
        CHECK(txt_quote_arg(q, sizeof q, "") == 2u && strcmp(q, "\"\"") == 0);
    }

    /* Tab: one match completes; a second Tab on a single match keeps it. */
    txt_cmdline_open(&g_c);
    type("QUEUE_SH");
    txt_cmdline_key(&g_c, 9, 0);
    CHECK(is("QUEUE_SHUFFLE"));
    txt_cmdline_key(&g_c, 9, 0);
    CHECK(is("QUEUE_SHUFFLE"));

    /* lower case input completes to the verb */
    txt_cmdline_open(&g_c);
    type("queue_sh");
    txt_cmdline_key(&g_c, 9, 0);
    CHECK(is("QUEUE_SHUFFLE"));

    /* several matches: listed in the status line, then cycled */
    txt_cmdline_open(&g_c);
    type("QU");
    txt_cmdline_key(&g_c, 9, 0);
    CHECK(strstr(g_c.status, "QUEUE_ADD") != NULL);
    CHECK(strstr(g_c.status, "QUEUE_SHUFFLE") != NULL);
    CHECK(strstr(g_c.status, "STOP") == NULL);
    snprintf(line, sizeof line, "%s", txt_cmdline_text(&g_c));
    txt_cmdline_key(&g_c, 9, 0);
    CHECK(strncmp(txt_cmdline_text(&g_c), "QUEUE_", 6) == 0);
    CHECK(strcmp(line, txt_cmdline_text(&g_c)) != 0 || strcmp(line, "QUEUE_") != 0);

    /* an argument position is not completed */
    txt_cmdline_open(&g_c);
    type("SEEK +5");
    txt_cmdline_key(&g_c, 9, 0);
    CHECK(is("SEEK +5"));

    /* history: Up recalls the previous line, Down returns to the draft */
    txt_cmdline_open(&g_c);
    type("NEXT");
    CHECK(txt_cmdline_key(&g_c, 13, 0) == TXT_CMD_SUBMIT);
    txt_cmdline_open(&g_c);
    type("PAUSE");
    CHECK(txt_cmdline_key(&g_c, 13, 0) == TXT_CMD_SUBMIT);
    txt_cmdline_open(&g_c);
    type("PL");
    txt_cmdline_key(&g_c, TXT_KEY_UP, 0);
    CHECK(is("PAUSE"));
    txt_cmdline_key(&g_c, TXT_KEY_UP, 0);
    CHECK(is("NEXT"));
    txt_cmdline_key(&g_c, TXT_KEY_UP, 0);
    CHECK(is("NEXT"));
    txt_cmdline_key(&g_c, TXT_KEY_DOWN, 0);
    CHECK(is("PAUSE"));
    txt_cmdline_key(&g_c, TXT_KEY_DOWN, 0);
    CHECK(is("PL"));
    CHECK(txt_cmdline_key(&g_c, 27, 0) == TXT_CMD_CANCEL);

    /* history keeps the last 100 lines */
    memset(&g_c, 0, sizeof g_c);
    for (i = 0; i < 105; i++) {
        txt_cmdline_open(&g_c);
        snprintf(line, sizeof line, "SEEK +%u", (unsigned)i);
        type(line);
        txt_cmdline_key(&g_c, 13, 0);
    }
    CHECK(g_c.history_count == 100);
    txt_cmdline_open(&g_c);
    for (i = 0; i < 150; i++) {
        txt_cmdline_key(&g_c, TXT_KEY_UP, 0);
    }
    CHECK(is("SEEK +5"));
    n = txt_cmdline_history_dump(&g_c, out, sizeof out);
    CHECK(n == 0); /* 100 lines do not fit in 512 bytes: dump refuses, writes nothing */

    /* history text round trip */
    memset(&g_c, 0, sizeof g_c);
    CHECK(txt_cmdline_history_load(&g_c, "PLAY\nNEXT\n\nSTOP\n") == 3);
    n = txt_cmdline_history_dump(&g_c, out, sizeof out);
    CHECK(n == strlen("PLAY\nNEXT\nSTOP\n") && strcmp(out, "PLAY\nNEXT\nSTOP\n") == 0);

    /* `!program` with two selected paths: argdump.sh writes its arguments to
     * the file named by ARGDUMP_OUT, one per line */
    if (argc > 2) {
        char dump[512];
        int status;
        snprintf(dump, sizeof dump, "%s", argv[2]);
        remove(dump);
        setenv("ARGDUMP_OUT", dump, 1);
        paths[0] = "/music/one song.mod";
        paths[1] = "/music/two.xm";
        snprintf(line, sizeof line, "%s --flag", argv[1]);
        status = txt_spawn(line, paths, 2);
        CHECK(status == 0);
        f = fopen(dump, "r");
        CHECK(f != NULL);
        if (f != NULL) {
            n = fread(out, 1, sizeof out - 1, f);
            out[n] = '\0';
            fclose(f);
            CHECK(strcmp(out, "--flag\n/music/one song.mod\n/music/two.xm\n") == 0);
        }
        CHECK(txt_spawn("/nonexistent/program", paths, 2) != 0);
    } else {
        printf("FAIL: argdump script and output path missing\n");
        g_fail++;
    }
    printf(g_fail == 0 ? "cmdline_test: ok\n" : "cmdline_test: %d failures\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
