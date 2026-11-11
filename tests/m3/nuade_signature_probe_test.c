/* I1305i2: the Amiga scan missed nuade songs (selecta-test.mod, FORM MODL) because the nuade manifest had no
 * probe and loading the 1.6 MB player module to ask was not an option. The manifest now matches the magic byte
 * table in plugins/replayers/nuade/signatures.c. This test runs that table over every file in the directories
 * given on the command line and checks, per file and per player with a table row, that the table agrees with the
 * player's real probe: a table hit must be a real Supported/Unsure, and a real Supported must be a table hit.
 * The real probe gets a neutral file name, so only the content decides. Prints nuade_signature_probe_ok. */
#include "../../plugins/replayers/nuade/signatures.h"

#include <nuade/nuade.h>
#include <nuade/player.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static unsigned g_files, g_hits, g_bad;
static unsigned g_ptk4_hits; /* sentinel: the FORM MODL test song must hit protracker4 */

static int table_hit(const char* id, const uint8_t* data, size_t size) {
    size_t i;
    for (i = 0; i < sel_nuade_sig_count(); i++) {
        if (strcmp(sel_nuade_sig_player(i), id) == 0 && sel_nuade_sig_match_entry(i, data, size)) {
            return 1;
        }
    }
    return 0;
}

static void check_file(const char* path) {
    FILE* f = fopen(path, "rb");
    uint8_t* data;
    long len;
    size_t i;
    const char* last = NULL;
    if (f == NULL) {
        return;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 16L * 1024 * 1024 || (data = malloc((size_t)len)) == NULL) {
        fclose(f);
        return;
    }
    if (fread(data, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(data);
        return;
    }
    fclose(f);
    g_files++;
    for (i = 0; i < sel_nuade_sig_count(); i++) {
        const char* id = sel_nuade_sig_player(i);
        const NuadePlayer* p;
        int hit, real;
        if (last != NULL && strcmp(last, id) == 0) {
            continue; /* rows of one player are consecutive */
        }
        last = id;
        p = nuade_player_find(id);
        if (p == NULL || p->probe == NULL) {
            printf("FAIL table names unknown player %s\n", id);
            g_bad++;
            continue;
        }
        hit = table_hit(id, data, (size_t)len);
        real = p->probe(data, (size_t)len, "song");
        if (hit) {
            g_hits++;
            if (strcmp(id, "protracker4") == 0) {
                g_ptk4_hits++;
            }
        }
        if ((hit && real == NuadeProbe_Unsupported) || (!hit && real == NuadeProbe_Supported)) {
            printf("FAIL %s: table %s, real probe %s\n", path, hit ? "hit" : "miss",
                   real == NuadeProbe_Supported ? "Supported" : real == NuadeProbe_Unsure ? "Unsure" : "Unsupported");
            g_bad++;
        }
    }
    free(data);
}

static void walk(const char* dir) {
    DIR* d = opendir(dir);
    struct dirent* e;
    if (d == NULL) {
        check_file(dir);
        return;
    }
    while ((e = readdir(d)) != NULL) {
        char path[2048];
        struct stat st;
        if (e->d_name[0] == '.') {
            continue;
        }
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            walk(path);
        } else if (S_ISREG(st.st_mode)) {
            check_file(path);
        }
    }
    closedir(d);
}

int main(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; i++) {
        walk(argv[i]);
    }
    printf("files=%u table_hits=%u protracker4_hits=%u mismatches=%u\n", g_files, g_hits, g_ptk4_hits, g_bad);
    if (g_bad != 0u || g_ptk4_hits == 0u) {
        return 1;
    }
    printf("nuade_signature_probe_ok\n");
    return 0;
}
