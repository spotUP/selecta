/* I786a: library.sync_snoozed ("key:days" list built like nag_snooze in desktop_ui.c) with 11 sources is
 * longer than the old 128-char pref limit and must be accepted, saved and loaded back byte for byte.
 * The limit itself: SEL_PREF_VALUE_MAX-1 chars accepted, SEL_PREF_VALUE_MAX rejected. */
#include "prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

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

static sel_prefs g_a, g_b;

/* I1162d: a reader polling prefs.conf while a writer saves in a loop never sees a truncated or empty
 * file: every read holds one line per declared key. */
static const char* g_dir;
static volatile int g_stop;

static void* writer_main(void* arg) {
    static sel_prefs w;
    (void)arg;
    sel_prefs_init(&w);
    while (!g_stop) {
        (void)sel_prefs_save(&w, g_dir);
    }
    return NULL;
}

static void concurrent_save_test(const char* dir) {
    char path[160], line[SEL_PREF_VALUE_MAX + 64];
    pthread_t th;
    int r, bad = 0, reads = 0;
    sel_prefs seed;
    g_dir = dir;
    sel_prefs_init(&seed);
    CHECK(sel_prefs_save(&seed, dir) == 1, "seed save failed");
    snprintf(path, sizeof path, "%s/prefs.conf", dir);
    g_stop = 0;
    pthread_create(&th, NULL, writer_main, NULL);
    for (r = 0; r < 20000; r++) {
        FILE* f = fopen(path, "r");
        int lines = 0;
        if (f == NULL) {
            bad++;
            continue;
        }
        while (fgets(line, sizeof line, f) != NULL) {
            lines++;
        }
        fclose(f);
        reads++;
        if (lines != SEL_PREF_COUNT) {
            bad++;
        }
    }
    g_stop = 1;
    pthread_join(th, NULL);
    CHECK(bad == 0, "%d of %d reads saw a missing or incomplete prefs.conf (want %d lines)", bad, reads, SEL_PREF_COUNT);
    snprintf(path, sizeof path, "%s/prefs.conf.tmp", dir);
    CHECK(access(path, F_OK) != 0, "temp file left behind after save");
}

int main(void) {
    char dir[] = "/tmp/sel-prefs-max-XXXXXX";
    char csv[SEL_PREF_VALUE_MAX], path[128], fill[SEL_PREF_VALUE_MAX + 1];
    size_t n = 0;
    int k, id;
    if (mkdtemp(dir) == NULL) {
        printf("FAIL mkdtemp\n");
        return 1;
    }
    csv[0] = '\0';
    for (k = 0; k < 11; k++) { /* same entry format as nag_snooze */
        char entry[48];
        size_t len = (size_t)snprintf(entry, sizeof entry, "%ssource-key-number-%02d:%d", n > 0 ? "," : "", k, 20000 + k);
        memcpy(csv + n, entry, len + 1u);
        n += len;
    }
    CHECK(n > 128 && n < SEL_PREF_VALUE_MAX, "csv length %zu not in (128, %d)", n, SEL_PREF_VALUE_MAX);

    sel_prefs_init(&g_a);
    CHECK(sel_prefs_set(&g_a, "library.sync_snoozed", csv) == SEL_PREFS_SET_OK, "11-source snooze list (%zu chars) rejected", n);
    sel_prefs_save(&g_a, dir);
    sel_prefs_init(&g_b);
    sel_prefs_load(&g_b, dir);
    id = sel_prefs_find("library.sync_snoozed");
    CHECK(id >= 0, "key not declared");
    if (id >= 0) {
        CHECK(strcmp(g_b.value[id], csv) == 0, "round trip: got '%s', want '%s'", g_b.value[id], csv);
    }

    memset(fill, 'x', sizeof fill - 1u);
    fill[SEL_PREF_VALUE_MAX - 1] = '\0';
    CHECK(sel_prefs_set(&g_a, "library.sync_snoozed", fill) == SEL_PREFS_SET_OK, "length MAX-1 rejected");
    fill[SEL_PREF_VALUE_MAX - 1] = 'x';
    fill[SEL_PREF_VALUE_MAX] = '\0';
    CHECK(sel_prefs_set(&g_a, "library.sync_snoozed", fill) == SEL_PREFS_SET_BAD, "length MAX accepted");

    concurrent_save_test(dir);
    snprintf(path, sizeof path, "%s/prefs.conf", dir);
    unlink(path);
    rmdir(dir);
    if (failures == 0) {
        printf("prefs_value_max_ok\n");
    }
    return failures != 0;
}
